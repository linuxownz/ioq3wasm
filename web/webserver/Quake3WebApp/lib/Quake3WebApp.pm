package Quake3WebApp;

use Dancer2;
use Dancer2::Plugin::Database;
# use Dancer2::Plugin::Ajax;

use Bytes::Random::Secure qw(random_bytes_hex);

use Data::Dumper;

our $VERSION = '0.1';

# set 'logger' => 'Console';
set 'logger' => 'File';

my @modes = (
    'login',
    'logout',
#   'send_file',
#   'not_found',
    'ajax',
    'welcome',
    'settings',
    'servers',
    'play',
    'admin',

    'spawn_server',

#   'q3config',
#   'filelist',
#   'shaderlist',

    'setting_change',
    'binding_change',
);

any ['get', 'post'] => '/' => sub {
    debug 'any get,post /';

    if ( not session('loggedin') ) {
        debug 'not logged in';
        return login();
    }

    my $rm = params->{rm} || '';

    if ( $rm eq '' ) {
        if ( request->is_post ) {
            my $json_request = decode_json request->body || '{}';
            debug Dumper $json_request;
            if ( exists $json_request->{rm} ) {
                $rm = $json_request->{rm};
            }
        }
    }

    if ( $rm ) {
        if ( grep { /$rm/ } @modes ) {
            debug "rm: $rm";
            if (exists &{$rm}) {
                no strict 'refs';
                return &{$rm}();
                use strict;
            } else {
                die "not such method $rm";
                error "no such method $rm";
            }
        }
    } else {
        return welcome();
    }
};

hook 'before' => sub {
    debug '---------------------------------------------------------';
    debug 'hook before';
    var appname => setting('appname');
};

get '/[a-f0-9]{32}' => sub {
    my ( $filename, undef ) = splat;
    debug "asset $filename";

    unless ( session 'loggedin' ) {
        return '';
    }
};

get '/filelist.*' => sub {
    my ( $mapname, undef ) = splat;
    debug "mapname $mapname ";

    unless ( session 'loggedin' ) {
        return '';
    }

    my $owner_id    = session('userid');

    my $reply = '';
    if ( $mapname ) {

        my $filelist = database->selectcol_arrayref (
        "SELECT md5hash || ' ' || ( SELECT CASE WHEN path != '' THEN path || '/' ELSE '' END ) || a.filename as filename
        FROM assets a
        JOIN map_assets ma ON a.asset_id = ma.asset_id
        JOIN map m         ON ma.map_id  = m.map_id
        WHERE mapname = ?
        ORDER BY a.path, a.filename;",
        { Slice => { } }, $mapname );

        for my $line ( @$filelist ) {
            $reply .= "$line\n";
        }

        # $reply = Dumper $filelist;
        # $self->log->info ( Dumper $filelist );
        # $self->log->info ( $reply );
    }

    return $reply;
};

get '/q3config.cfg' => sub {

    debug 'q3config.cfg';

    unless ( session 'loggedin' ) {
        return '';
    }

    my $owner_id = session('userid');

    debug "owner_id $owner_id";

    my $config = database->selectall_arrayref (
        "SELECT config_name, unnest(string_to_array(config_value, ' ')) AS config_value, config_bind
        FROM get_player_config(?)", { Slice => { } }, $owner_id );

    my $reply = "unbindall\n";
    for my $config ( @$config ) {
        my $name  = $config->{config_name};
        my $value = $config->{config_value};
        my $bind  = $config->{config_bind};

        my $config_line = "";

        if ( $bind ) {
            $config_line = "bind $value \"$name\"";
        } else {
            $config_line = "seta $name \"$value\"";
        }

        $reply .= "$config_line\n";
    }

    # info ( Dumper $reply );

    # $self->header_add ( -cache_control => 'no-store' ); # Cache-Control: no-store

    return $reply;
};

get '/shader.list.*' => sub {

    unless ( session 'loggedin' ) {
        return '';
    }
    my ( $map, undef ) = splat;
    debug "shader list map $map ";

    my $owner_id    = session('userid');
    my $reply = '';

    my $filelist = database->selectcol_arrayref (
        #"SELECT md5hash || ' ' || ( SELECT CASE WHEN path != '' THEN path || '/' ELSE '' END ) || a.filename as filename
    "SELECT ( SELECT CASE WHEN path != '' THEN path || '/' ELSE '' END ) || a.filename as filename
    FROM assets a
    JOIN map_assets ma ON a.asset_id = ma.asset_id
    JOIN map m         ON ma.map_id  = m.map_id
    WHERE mapname = ? and a.filename like '%.shader'
    ORDER BY a.filename;",
    { Slice => { } }, $map );

    for my $line ( @$filelist ) {
        $reply .= "$line\n";
    }

    # $reply = Dumper $filelist;
    # $self->log->info ( Dumper $filelist );
    # $self->log->info ( $reply );

    return $reply;
};

sub login {

    my $login_message = undef;
    my $flag = params->{flag} || '';

    info 'login';

    my $loggedin = session('loggedin') || '';

    if ( $loggedin eq '1' ) {
        debug 'already logged in';
        return welcome();
    } elsif ( $flag eq '1' ) {

        my $username = params->{username};
        my $password = params->{password};

        $login_message = "Please provide username and password" unless $username && $password;

        unless ( $login_message ) {
            my ( $retval, $message, $userinfo ) = do_login( $username, $password );

            if ( $retval == 0 ) {
                debug 'login fail';
                $login_message = $message;
            } else {
                debug 'login ok';
                session 'userid'   => $userinfo->{id};
                session 'loggedin' => '1';
                session 'admin'    => $userinfo->{admin};
                session 'fullname' => $userinfo->{fullname};

                my $loginid = database->selectcol_arrayref ( "INSERT INTO logins ( userid, ipaddress ) values ( ?, ? ) returning loginid", undef, $userinfo->{id}, request->address() )->[0];

                session 'loginid' => $loginid;

                debug Dumper session;

                return welcome();
            }
        }
    } else {
        database->do("INSERT INTO visits(ipaddress) VALUES ( ? )", undef, request->address());
    }

    my $appname = vars->{appname};
    return template 'login', { template_name => 'login', login_message => $login_message, appname => $appname };
}

sub logout {
    debug 'logout';

    my $userid = session 'userid';
    my $loginid = session 'loginid';

    if ( $userid ) {
        database->do ( "INSERT INTO logouts ( userid, loginid ) values ( ?, ? )", undef, $userid, $loginid );
    }

    my $appname = vars->{appname};
    app->destroy_session;
    #cookie 'expires' => '-1d';

    return template 'login', { template_name => 'login', login_message => "You have logged out", appname => $appname };
}

sub welcome {
    my $user  = session( 'fullname' );
    my $admin = session( 'admin'    );
    my $flag  = 0;

    debug "welcome $user";
    # debug Dumper session;

    my $appname = vars->{appname};

    return template ( 'welcome', { admin => $admin, fullname => $user, template_name => 'welcome', appname => $appname } );
}

sub servers {
    my $servers = database->selectall_arrayref (
        "SELECT instance_id, server_name, mapname, COALESCE(json, '{}') as json, a.md5hash as levelshot
        FROM running_servers
        LEFT JOIN assets a on a.path = 'levelshots' and filename = json::json->'serverinfo'->>'mapname' || '.png'
        WHERE state = 'running'
        ORDER BY timestamp", { Slice => { } } );

    for my $server ( @$servers ) {
        $server->{json} = decode_json ( $server->{json} );

        my $g_gametype = $server->{json}->{serverinfo}->{g_gametype} // 0;
        my $g_gametypestr = "FFA";

        if ( $g_gametype == 1 ) {
            $g_gametypestr = "Tournament";
        } elsif ( $g_gametype == 3 ) {
            $g_gametypestr = "Team Deathmatch";
        } elsif ( $g_gametype == 4 ) {
            $g_gametypestr = "Capture the Flag";
        }

        $server->{json}->{serverinfo}->{g_gametypestr} = $g_gametypestr;
    }

    my $admin    = session 'admin';
    my $fullname = session 'fullname';
    my $appname  = vars->{appname};

    return template ( 'servers', {
            admin => $admin, fullname => $fullname, template_name => 'servers', appname => $appname,
            servers          => $servers,
            serverinfo_names => [ qw / mapfullname mapname g_gametype g_gametypestr timelimit sv_maxclients fraglimit dmflags capturelimit / ],
            playerinfo_names => [ qw / name ping rate handicap snaps                                                                       / ]
        }
    );
}

sub spawn_server {
    if (! session('admin')) {
        return welcome();
    }

    my @allmaps = database->selectall_array(
        "SELECT mapname, map_id, a.md5hash mapscreenshothash
        FROM map m
        LEFT JOIN assets a on a.path = 'levelshots' and a.filename = m.mapname || '.png'
        ORDER BY mapname;", { Slice => { } });

    # debug Dumper @allmaps;

    my $flag = param ( 'spawn' );

    my $msg = "";

    if ( $flag eq '1' ) {
        # TODO HUNT for all g_ server cvars and populate tables cvar and cvars game/g_local.h

        # timestamp requests to limit spawn requests

        my $owner_id    = session('userid'); # ok
        my $cvar_names  = database->selectall_hashref ( "SELECT name, value FROM cvars WHERE owner = ? and clientserver = 'server' AND NAME NOT LIKE 'net_%' ORDER BY 1", 'name', { Slice => {} }, $owner_id);
        my $config_uuid = database->selectrow_hashref ( "SELECT gen_random_uuid uuid FROM gen_random_uuid()" )->{uuid};

        my $cvars;
        map { $cvars->{$_} = $cvar_names->{$_}->{value} } keys %$cvar_names;
        # debug Dumper $cvars; # ok

        body_parameters->remove('rm');
        body_parameters->remove('flag');

        my $body_params_hash = body_parameters;

        my $servername = $body_params_hash->{servername} // 'Server 1';
        $body_params_hash->remove('servername');
        # debug "server name: $servername"; ok

        # TODO backup db
        # insert into cvar ( name, default_value, clientserver, bind ) values ( 'bot_minplayers', '0', 'server', false );
        # insert into cvar ( name, default_value, clientserver, bind ) values ( 'g_inactivity', '0', 'server', false );
        # insert into cvar ( name, default_value, clientserver, bind ) values ( 'snaps', '40', 'server', false );
        # ALTER table cvars ADD CONSTRAINT fk_cvar FOREIGN KEY ( name, clientserver ) references cvar(name, clientserver);
        # DONE create table server_spawn_config ( name varchar(64) not null, value varchar(64) not null, config_uuid uuid not null );
        # GRANT select,update on server_spawn_config to quake3webapp ;

        my @maps = $body_params_hash->get_all('map');
        $body_params_hash->remove ( 'map' );
        # debug Dumper @maps;  # ok

        # debug Dumper $body_params_hash;

        for my $key ( keys %$body_params_hash ) {
            if ( exists $cvars->{$key} ) {
                $cvars->{$key} = body_parameters->{$key}
            }
        }
        # debug Dumper $cvars; # ok

        # DONE
        # create table server_spawn_config ( name varchar(64) not null, value varchar(64) not null, config_uuid uuid not null );
        # ok
        for my $key ( sort keys %$cvars ) {
            database->do ( "INSERT INTO server_spawn_config ( name, value, config_uuid ) VALUES ( ?, ?, ? );", undef, $key, $cvars->{$key}, $config_uuid );
        }

        debug Dumper @maps;

        my @mapnames;
        if ( ! @maps ) {
            database->do ( "INSERT INTO server_spawn_config ( name, value, config_uuid ) VALUES ( ?, ?, ? );", undef, "map", "q3ctf1", $config_uuid );
        } else {
            # get map names from @maps uuids
            my $mapquery = "SELECT mapname FROM map WHERE map_id IN ( ";
            $mapquery .= join ', ', ( ('?') x scalar @maps );
            $mapquery .= ')';

            debug $mapquery;
            @mapnames = @{database->selectcol_arrayref ( $mapquery, undef, @maps )};
            debug Dumper \@mapnames;

            #     d1
            # set d1 "map q3dm1 ; set nextmap vstr d2"
            # set d2 "map q3dm2 ; set nextmap vstr d3"
            # set d3 "map q3dm3 ; set nextmap vstr d1" <<<< d1
            # vstr d1 <<<<

            if ( scalar @maps == 1 ) {
                debug 'INSERT one mapname';
                database->do ( "INSERT INTO server_spawn_config ( name, value, config_uuid ) VALUES ( ?, ?, ? );", undef, "map", @mapnames[0], $config_uuid );
            } else {
                my $pos = 1;
                my $nummaps = scalar @mapnames;
                for my $mapname ( sort @mapnames ) {
                    my $vstr = "map $mapname ; set nextmap vstr d" . ( $pos + 1 );

                    if ( $pos == $nummaps ) {
                        $vstr = "map $mapname ; set nextmap vstr d1";
                    }

                    debug $vstr;
                    database->do ( "INSERT INTO server_spawn_config ( name, value, config_uuid ) VALUES ( ?, ?, ? );", undef, "set d$pos", $vstr, $config_uuid );
                    $pos++;
                }

                database->do ( "INSERT INTO server_spawn_config ( name, value, config_uuid ) VALUES ( ?, ?, ? );", undef, "vstr d1", "", $config_uuid );
            }
        }

        # DONE on both
        # ALTER TABLE server_spawn_request rename map to config_uuid;
        # ALTER TABLE server_spawn_request alter config_uuid set not null;
        # ALTER table server_spawn_config ADD COLUMN id serial;
        # GRANT USAGE, SELECT ON server_spawn_config_id_seq TO  quake3webapp ;

        # DONE ON BOTH ALTER this SP to instead take config_uuid instead of $map_id ( didn't rename param )
        #
        debug "create_server_spawn_request ( $servername, $config_uuid, $owner_id )";
        database->do ( "SELECT * FROM webapp.create_server_spawn_request ( ?, ?, ? );", undef, $servername, $config_uuid, $owner_id );

        $msg = "server spawned";
    }

    return template ( 'spawn_server.tt', { maps => \@allmaps, template_name => 'spawn_server', appname => vars->{appname}, msg => $msg } );
}

sub settings {

    my $owner_id = session('userid');

    my @binds  = database->selectall_array ( "SELECT config_name as name, config_value as value FROM get_player_config(?) WHERE config_bind = true",  { Slice => { } }, $owner_id );
    my @config = database->selectall_array ( "SELECT config_name as name, config_value as value FROM get_player_config(?) WHERE config_bind = false", { Slice => { } }, $owner_id );
    my $keys   = database->selectcol_arrayref ( "SELECT key FROM key ORDER BY 1", );

    my $binds; my $config;
    map {  $binds->{$_->{name}} = $_->{value} } @binds;
    map { $config->{$_->{name}} = $_->{value} } @config;

    # info ( Dumper $binds );
    # info ( Dumper $config );
    # info ( Dumper $keys );

    my $admin    = session 'admin';
    my $fullname = session 'fullname';
    my $appname  = vars->{appname};

    return template ( 'settings', {
        admin => $admin, fullname => $fullname, template_name => 'settings', appname => $appname,
        config => $config, binds => $binds, keys => JSON->new->utf8->encode($keys)
    } );
}

sub play {
    info ( 'play' );

    my $flag    = param ( 'flag' ) || '';
    my $inst_id = param ('instance_id');

    info ( "instance id request $inst_id" );

    my $server  = {
        instance_id => '000000-0000-0000-000000-000000',
        server_name => 'no name',
        mapname     => 'not set',
        port        => '1',
    };

    if ( $flag eq '1' ) {
        if ( $inst_id && $inst_id =~ /^\b(uuid:){0,1}\s*([a-f0-9\\-]*){1}\s*$/ && length($inst_id) == 36 ) {
            $server = database->selectrow_hashref (
                "SELECT instance_id, server_name, mapname, port, COALESCE(json, '{}') as json, a.md5hash as levelshot
                FROM running_servers
                LEFT JOIN assets a on a.path = 'levelshots' and filename = json::json->'serverinfo'->>'mapname' || '.png'
                WHERE instance_id = ?", undef, $inst_id
            );

            $server->{json} = decode_json ( $server->{json} );

        } else {
            info ( "invalid instance_id uuid : '$inst_id'" );
        }
    }

    my $admin    = session 'admin';
    my $fullname = session 'fullname';
    my $appname  = vars->{appname};

    my $g_gametype = $server->{json}->{serverinfo}->{g_gametype} // 0;
    my $g_gametypestr = "FFA";

    if ( $g_gametype == 1 ) {
        $g_gametypestr = "Tournament";
    } elsif ( $g_gametype == 3 ) {
        $g_gametypestr = "Team Deathmatch";
    } elsif ( $g_gametype == 4 ) {
        $g_gametypestr = "Capture the Flag";
    }

    $server->{json}->{serverinfo}->{g_gametypestr} = $g_gametypestr;

    return template ( 'play.tt',
        {
            instance_id => $server->{instance_id},
            server_name => $server->{server_name},
            mapname     => $server->{mapname},
            port        => $server->{port},
            json        => $server->{json},
            levelshot   => $server->{levelshot},

            admin => $admin, fullname => $fullname, template_name => 'play', appname => $appname,

            serverinfo_names => [ qw / mapfullname mapname g_gametype g_gametypestr timelimit sv_maxclients fraglimit dmflags capturelimit / ],
            playerinfo_names => [ qw / name ping rate handicap snaps                                                                       / ]
        }
    );
}

sub binding_change {
    debug 'binding_change';

    my $postdata = request->body || '{}';
    if ( $postdata ) {
        my $jsondata = decode_json($postdata);
        info("binding_change: ", Dumper ( $jsondata ));

        delete $jsondata->{rm};

        my $req = {
            'key'   => $jsondata->{bind},    # key to be set to +forward, to be *removed* from current 'R' binding if any
            'mouse' => undef,                # not a mouse bind
            'name'  => $jsondata->{binding}, # bind
            'value' => '',                   # key CURRENTLY assigned to +forward, could be space seperated multiples
        };

        return updateBind( $req );
    }

    return '';
};

sub updateBind {
    my $json = shift;

    # info ( 'updateBind ' . Dumper ( $json ) );

    my $req = {
        'key'   => 'R',        # key to be set to +forward, to be *removed* from current 'R' binding if any
        'mouse' => undef,      # not a mouse bind
        'name'  => '+forward', # bind
        'value' => 'e',        # key CURRENTLY assigned to +forward, could be space seperated multiples
    };

    my $uid = session('userid');

    my $name   = $json->{name}  || '';
    my $key    = $json->{mouse} || $json->{key} || '';
    my $curval = $json->{value} || '';

    # if key in curval return ;

    my $re = qr/[^a-zA-Z0-9+ -_\[\]]|^.{64,}$/; # TODO any punctuation for say .... say :}  have to handle say/vstr seperately?

    if ( $name =~ $re or $key =~ $re ) {
        warning ( "updateBind illegal value or length error in name, key '$name', '$key'" );
        return '{ "updateBind" : false }';
    }

    # info ( "update_player_bind ( ?, ?, ? ), name:$name, key:$key, $uid " );
    my $result = database->selectrow_hashref ( "SELECT * FROM update_player_bind ( ?, ?, ? )", undef, $name, $key, $uid );

    # info ( 'result:' . Dumper $result );

    # if key == value return true  # assigning same key to existing bind -> ignore
    #
    # if key not in values:
    #    update to remove binds currently bound to 'key'
    #    add key to value for keys that find +forward
    #
    # if key is DEL
    #    backspace removes all binds for +forward
    #
    # binds are REMOVED by adding a new config ( therefore latest version ) for '+forward' with key >>''<<

    if ( $result and $result->{update_player_bind} and $result->{update_player_bind} eq '1' ) {
        my $binds = database->selectrow_hashref (
           "SELECT LOWER(config_name) as config_name, array_agg ( LOWER(config_value) ) AS keys
            FROM get_player_config ( ? )
            WHERE config_bind = true AND LOWER(config_name) = LOWER(?)
            GROUP BY LOWER(config_name)", undef, $uid, $name
        );

        # $self->log->info ( "binds: " . Dumper $binds );

        my $bind = {
            update_player_bind => 1,
        };

        if ( $binds->{config_name} ) {
            $bind->{$binds->{config_name} || $name } = $binds->{keys} || $key;
        }

        # info ( "bind: " . Dumper $bind );

        return encode_json($bind);
    }

    return '{ "updateBind" : false }';
}

sub setting_change {
    my $jsondata = undef;

    my $postdata = request->body || '{}';
    if ( $postdata ) {
        $jsondata = decode_json($postdata);
        info("setting_change: ", Dumper ( $jsondata ));

        delete $jsondata->{rm};

        for my $key ( keys %$jsondata ) {
            next if $key =~ /com_unfocused/;

            my $req = {
              'cvar'   => $key,
              'value'  => $jsondata->{$key}
            };

            return updateCvar( $req );
        }
    }

    return '';
}

sub updateCvar {
    my $json = shift;

    info ( 'updateCvar ' . Dumper ( $json ) );

    my $req = {
      'value'  => '0.4',
      'cvar'   => 's_volume',
      'action' => 'updateCvar',
      'rm'     => 'ajax'
    };

    my $uid = session('userid');

    my $cvar   = $json->{cvar}  || '';
    my $value  = $json->{value}      ; # TODO 0 will be ''

    my $re = qr/[^a-zA-Z0-9+ -_\[\]]|^.{64,}$/; # TODO any punctuation for say .... say :}  have to handle say/vstr seperately?

    if ( $cvar =~ $re or $value =~ $re ) {
        warning ( "updateCvar illegal value or length error in name, key '$cvar', '$value'" );
        return '{ "updateCvar" : false }';
    }

    info ( "update_player_cvar ( ?, ?, ? ), $cvar, $value, $uid " );
    my $result = database->selectrow_hashref ( "SELECT * FROM update_player_cvar ( ?, ?, ? )", undef,  $cvar, $value, $uid );

    info ( 'result:' . Dumper $result );

    return '{ "updateCvar" : true }';
}

sub ajax {

    debug 'rm=ajax';

    my $postdata = request->body || '{}';

    # debug Dumper $postdata;

    my $jsondata = decode_json ( $postdata );

    # debug Dumper $jsondata;

    my $action = $jsondata->{action} || '';

    my $ajax_response = "{}";

    #response_header 'type' => 'applicaton/json';
    content_type 'applicaton/json';

    unless ( $action ) {
        return $ajax_response;
    }

    if ( $action eq 'servers' ) {
        # ???
        return servers(); # return a page?  was this for an overlay?
    }

    if ( $action eq 'updateBind' ) {
        return updateBind ( $jsondata );
    }

    if ( $action eq 'updateCvar' ) {
        return updateCvar ( $jsondata ) ;
    }

    info ( 'unknown ajax request ' . Dumper $jsondata );

    return "";
}

sub admin {

    if ( ! session('admin')  ) {
        return welcome();
    }

    my $flag = param ( 'flag' );

    if ( $flag eq '1' ) {
        info ( 'form submitted' );

        my $id        = param ( 'id' )        || '';
        my $action    = param ( 'action' )    || '';
        my $username  = param ( 'username' )  || '';
        my $fullname  = param ( 'fullname' )  || '';
        my $enabled   = param ( 'enabled' )   || 0;
        my $admin     = param ( 'admin' )     || 0;
        my $plaintext = param ( 'plaintext' ) || '';

        my $result = undef;
        # my $pbkdf2 = create_crypt_obj();

        info ( "$action" );

        if ( $plaintext eq '' ) {
            $plaintext = random_bytes_hex(8);
        }

        info ( "plaintext $plaintext" );
        # my $hash = $pbkdf2->generate($plaintext);
        my $hash = "PASSHASH";

        if ( $action eq 'editUser' ) {
            $result = database->do ( "UPDATE users SET username = ?, fullname = ?, enabled = ?::boolean, admin = ?::boolean, plaintext = ?, password = ? WHERE id = ?",
                undef, $username, $fullname, $enabled, $admin, $plaintext, $hash, $id ) or info( database->errstr );
        } elsif ( $action eq 'addUser' ) {
            $result = database->do (" SELECT * FROM webapp.create_user(?, ?::boolean, ?, ?::boolean, ?, ?)", undef, $username, 1, $hash, 0, $fullname, $plaintext ) or info ( database->errstr );

            # $result = $dbh->do ( "
            #     INSERT INTO users ( username, fullname, enabled, admin, password, plaintext )
            #     VALUES ( ?, ?, ?::boolean, ?::boolean, ?, ? );", undef, $username, $fullname, $enabled, $admin, $hash, $plaintext) or $self->log->info( $dbh->errstr );
        } else {
            info ( "form submitted action unknown '$action'" );
        }

        info ( Dumper $result );
    }

    my @users   = database->selectall_array ( "SELECT id, username, enabled, fullname, admin, plaintext FROM users WHERE username != 'system' ORDER BY admin DESC",  { Slice => { } } );
    my @servers = database->selectall_array ( "SELECT instance_id, server_name, state, port, mapname, json FROM running_servers order by port",   { Slice => { } } );

    # $self->log->info ( Dumper \@users, \@servers );
    return template ( 'admin.tt', { servers => \@servers, users => \@users, template_name => 'admin' } );
}

sub do_login {
    my $user = shift;
    my $pass = shift;

    debug 'do_login';

    my $sth = database->prepare ( "SELECT id, username, password, admin, fullname, plaintext FROM users WHERE username=? AND enabled = true" );
    my $exe = $sth->execute ( $user );

    if ( $exe ) {
        my $userinfo = $sth->fetchrow_hashref();
        if ( $userinfo ) {
            #my $password_ok = check_password ( $userinfo->{password}, $pass );
            my $password_ok = check_password ( $userinfo->{plaintext}, $pass );
            return ( 1, undef, $userinfo ) if $password_ok;
        }
    }

    return ( 0, 'Invalid username or password or user not enabled', undef );
}

sub check_password {
    my $db_pass = shift;
    my $pass    = shift;

    return $db_pass eq $pass;
}



true;

# sub cgiapp_prerun {
#
#     my $method = $query->request_method || '';
#     if ( $method eq 'POST' ) {
#         my $rm = $query->param('rm');
#         if ( $rm ) {
#             $self->prerun_mode ( $rm );
#             $self->log->info ( "setting run mode to $rm" );
#             return;
#         } else {
#             my $postdata = $query->param('POSTDATA');
#             if ( $postdata ) {
#                 my $jsondata = JSON->new->utf8->decode($postdata);
#
#                 if ( $jsondata ) {
#                     my $rm = $jsondata->{rm} || '';
#                     if ( $rm eq 'setting_change' ) {
#                         $self->prerun_mode('setting_change');
#                         return;
#                     } elsif ( $rm eq 'binding_change' ) {
#                         $self->prerun_mode('binding_change');
#                         return;
#                     } else {
#                         $self->prerun_mode('ajax');
#                     }
#                     $self->log->info ( "setting run mode to 'ajax'" . Dumper ( $postdata ) );
#                     return ;
#                 }
#             } else {
#                 # shouldn't get here
#                 my $postparams = $query->param('keywords');
#                 $self->log->info ( Dumper $query, $postparams );
#             }
#         }
#
#         $self->prerun_mode('welcome');
#         return;
#
#     } elsif ( $method eq 'GET' ) {
#         # not working TODO have to get working to limit access to logged in users for quake3 assets
#         # wget -q -S -O - http://localhost:8000?/css/admin.css
#         $self->log->info ( "processing GET request" );
#
#         my $querystring = $query->query_string || '';
#         my $request_uri = $query->request_uri  || '';
#
#         if ( $request_uri eq '/approot/q3config.cfg' ) {
#             $self->prerun_mode('q3config');
#         } elsif ( $request_uri =~ m|approot/filelist.*| ) {
#             $self->prerun_mode('filelist');
#         } elsif ( $request_uri =~ m|approot/shader\.list| ) {
#             $self->prerun_mode('shaderlist');
#         } elsif ( $querystring ) {
#             $self->log->info ( "setting prerun_mode to 'send_file' for qs: '$querystring'" );
#             $self->prerun_mode ( 'send_file' );
#             # Something wrong here.  any request sends file TODO
#             # $self->prerun_mode ( 'not_found' );
#         } else {
#             $self->prerun_mode('welcome');
#         }
#     } else {
#         $self->log->info ( "METHOD $method ignored" );
#     }
#
#     $self->log->info ( "run mode: " . $self->get_current_runmode );
#     $self->log->info ( "cgiapp_prerun end" );
# }



# sub log_request {
#     my $self  = shift;
#     my $query = $self->query;
#     my $log   = $self->log;
#
#     $log->info ( "\n >>> Request <<< " );
#
#     my $method = $query->request_method || '';
#     $log->info ( "request method: '$method'" );
#
#     my $content_type = $query->content_type() || '';
#     $log->info ( "content type: '$content_type'" );
#
#     my $referer = $query->referer() || '';
#     $log->info ( "referer: '$referer'" );
#
#     my $path_translated = $query->path_translated() || '';
#     $log->info ( "path_translated: '$path_translated'" );
#
#     my $user_rm   = $query->param ( 'rm' ) || '';
#     my $currentrm = $self->get_current_runmode();
#
#     $log->info ( "current runmode: '$currentrm'" );
#     $log->info ( "user rm: $user_rm" );
#
#     my $querystring = $query->query_string();
#     my $path_info   = $query->path_info();
#
#     $log->info ( "querystring : '$querystring'" );
#     $log->info ( "pathinfo    : '$path_info'" );
#
#     my @namesm = $query->multi_param;
#     my @names  = $query->param;
#
#     $log->info ( "multi parameter names: [" . join ( ', ', sort @namesm ) . ']' );
#     $log->info ( "      parameter names: [" . join ( ', ', sort @names  ) . ']'  );
#
#     $log->info ( "ENV querystring: " . $ENV{QUERY_STRING} );
#
#     my %http_headers  = map { $_ => $query->http ($_) } $query->http();
#     my %https_headers = map { $_ => $query->https($_) } $query->https();
#
#     $log->info ( "http request headers: \n" . Dumper \%http_headers ) ;
#     $log->info ( "https request headers:\n" . Dumper \%https_headers ) ;
#
#     $log->info ( ">>> Request end <<<\n" );
# }

# sub create_crypt_obj {
#     return Crypt::PBKDF2->new(
#         hash_class => 'HMACSHA1', # this is the default
#         iterations => 1000,       # so is this
#         output_len => 20,         # and this
#         salt_len   => 4,          # and this.
#     );
# }

# sub is_admin {
#     my $self  = shift;
#     my $user  = $self->session->param ( 'username' );
#     my $admin = 0;
#
#     my $dbh  = $self->dbh;
#
#     my $sth = $dbh->prepare ( "SELECT admin FROM users WHERE username=?" );
#     if ( $sth->execute ( $user ) ) {
#         my $userinfo = $sth->fetchrow_hashref();
#         $admin = $userinfo->{admin};
#     }
#
#     return $admin;
# }

# sub send_file {
#     my $self = shift;
#     my $log  = $self->log;
#     my $path = $self->query->env_query_string();
#     my $sdir = $self->config_param ( 'site.staticdir' );
#
#     # seperate out &x=1 args? nahhhh let it fail check and 404
#
#     $log->info ( "send_file ---------------------------------------------------------------------------------------------------------------------" );
#     $log->info ( "static dir '$sdir'" );
#     $log->info ( "send_file  '$path'" );
#
#     # path must start with /
#     my $isvalidpath = $path =~ m|^(?:(?:\/[a-zA-Z0-9-_]+)+\.[a-zA-Z0-9-_]{1,3})$|;
#     $log->info ( "send_file is valid path: '$isvalidpath'" );
#
#     if ( ! $isvalidpath ) {
#         $log->error ( "send_file isvalidpath:$isvalidpath not a valid path, returning" );
#         return $self->not_found();
#     }
#
#     my $catpath = File::Spec->catfile ( $sdir, $path );
#     $log->info ( "catpath: '$catpath'" );
#
#     my $canonpath = File::Spec->canonpath ( $catpath );
#     $log->info ( "canonpath: '$canonpath'" );
#
#     my $dirname = dirname ( $canonpath );
#     $log->info ( "dirname: '$dirname'" );
#
#     my $basename = basename ( $canonpath );
#     $log->info ( "basename: '$basename'" );
#
#     my $abspath = abs_path ( $dirname  );
#     if ( ! $abspath ) {
#         $log->error ( "abs_path error: dirname: '$dirname' $!" );
#         return $self->not_found();
#     }
#
#     $log->info ( "abs path: '$abspath'" );
#
#     if ( ! -d $abspath ) {
#         $abspath = '<NULL>' if ! $abspath;
#         $log->error ( "abs_path of '$sdir', '$path' => '$abspath' does not exist" );
#         return $self->not_found();
#     }
#
#     if ( $dirname ne $abspath ) {
#         $log->error ( "'$dirname' ne '$abspath'" );
#         return $self->not_found();
#     }
#
#     ( my $ext ) = $basename =~ m|\.(\w+)$|;
#
#     my $re = $self->config_param ( 'site.valid_extensions' ) || '^(?:jpe?g|png|gif)$';
#     my $ext_re = qr/$re/;
#     if ( $ext !~ $ext_re ) { # TODO
#         $log->error ( "file extension '$ext' not found in site.valid_extensions regular expression /$re/" );
#         return $self->not_found();
#     }
#     $log->info ( "ext:'$ext' pass re /$re/" );
#
#     my $fullpath = "$dirname/$basename";
#     $log->info ( "fullpath: '$fullpath'" );
#
#     my $mimetype = mimetype ( $basename );
#
#     if ( $mimetype ) {
#         $log->info ( "mimetype: '$mimetype'" );
#         $log->info ( "settings headers for x_sendfile for fullpath '$fullpath' mimetype:'$mimetype'" );
#
#         $self->header_add ( -content_type => $mimetype );
#         $self->header_add ( -x_sendfile   => $fullpath );
#     }
# }



true;

