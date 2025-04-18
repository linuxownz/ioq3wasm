#!/usr/bin/env perl
# this proxy handles all the details in spawning a server, config taken from a database ( DONE )
# polls for commands from server to send to q3 instance id'd by instance_id
# sends command over unix socket to ioq3ded instance, waits for reply and updated database
#
# check for already running servers after login
# make function to list and compare $servers list to what is really running
#
# running state of server and db can be out of sync
# running state of server and db can be out of sync
# running state of server and db can be out of sync
# running state of server and db can be out of sync
#
# proxy logs -> server on exit
#
# TODO
# ----- Server Shutdown (Server fatal crashed: FS_Read: -1 bytes read) FIXED
# testing
# shutdown all servers nicely after sig
# fill out 'proxy commands' like list_servers, etc
# info on running $servers/ps -ef etc
# exit/quit
# compare against db view ( running_servers )
#
#  TODO make sure this instance is the only one running ...
#  TODO make sure this instance is the only one running ...
#  TODO make sure this instance is the only one running ...
#  TODO make sure this instance is the only one running ...
#
# #############################################################################################
# uuid="7632f5ab-4bac-11e6-bcb7-0cc47a6c4dbd"
# pattern='^\{?[A-Z0-9a-z]{8}-[A-Z0-9a-z]{4}-[A-Z0-9a-z]{4}-[A-Z0-9a-z]{4}-[A-Z0-9a-z]{12}\}?$'
#
# if [[ "$uuid" =~ $pattern ]]; then
#     echo "true"
# else
#     echo "false"
# fi
# #############################################################################################
#
# LISTEN/NOTIFY
#
# TESTING
#
# shutdown
#   closes sockets
#   removes config and loads/removes log and removes .socket file
#   clean up/update db ( running_servers )
#
#       q3 server quit request
#       q3 server catches SIG kill q3server externally
#       proxy quit request
#       proxy control c/signal

use strict;
use warnings;

use POSIX;
use IO::Socket::UNIX;
use IO::Select;
use Time::HiRes qw /gettimeofday tv_interval usleep/;
use DBI;
use JSON;
use Try::Tiny;
use Data::Dumper;
$Data::Dumper::Sortkeys = 1;

my $VALGRIND = 0;
my $config;
my $servers;
my $logfh;

my @child_pids; # pids of exited processes to clean up

sub initialize;
sub signal_handler;
sub do_log;
sub do_die;
sub do_exit;

my $homedir = $ENV{HOME};
my $dbh;

my $proxy_commands = {
    'quit' => \&do_exit, # TODO test
    'exit' => \&do_exit,
    'list' => \&list_servers,
};

initialize();

select STDOUT; $| = 1;
my @p = qw( \ | / - );
my $p = 0; # used for timing.  do not remove

$dbh->do("LISTEN proxy_notify_channel");

my $run = 1;
while ( $run ) {

    my $notified = 0;
    while ( my $notify = $dbh->pg_notifies ) { # non blocking
        my ($topic, $pid, $payload) = @$notify;
        $notified = 1;

        do_log "$topic, $pid, $payload\n";

        my $t1 = [ gettimeofday ];
        if ( $payload eq 'proxy_request' ) {
            check_proxy_requests();
        } elsif ( $payload eq 'server_request' ) {
            q3server_requests();
        } elsif ( $payload eq 'server_spawn_request' ) {
            q3server_spawn_requests();
        } else {
            do_log "unknown NOTIFY payload '$payload'";
        }
        do_log "Time to respond to notify '$payload' " . tv_interval ( $t1 );
    }

    if ( ! $notified ) {
        my $t1 = [ gettimeofday ];

        destroy_servers();
        get_server_status();

        my $dur = sprintf("%.02f",  tv_interval ( $t1 ) );
        if ( $dur > 0.1 ) {
            do_log "Time to check destroy/server status $dur";
        }
    }

    $p++;

    # print "\b" . $p[ $p % ( $#p+1 ) ];

    sleep 1;
}

&do_exit();

sub send_command_to_server {
    my $server  = shift;
    my $cmd     = shift;
    my $maxwait = shift || 5;

    my ( $sock, $error ) = open_connection_to_server ( $server->{sockpath} );

    if ( $error ) {
        $server->{error} = $error;
        do_log ( "failed to open connection $error");
        return '{ "error": "failed to open connection ' . $error . '"}';
    }

    if ( $sock ) {
        $sock->send ( "$cmd\n" );
        $sock->flush;

        my $reply;
        my $select = IO::Select->new( $sock ); $! = 0;
        if ( $select->can_read ( $maxwait ) ) {
            my $data;
            while ( $sock->recv( $data, 4097, MSG_DONTWAIT ) ) {
                # do_log "recv " . ( length $data ) . "'$data'" ;
                $reply .= $data if $data;
                $data = undef;
                usleep 15000;
            }
        }

        $select->remove ( $sock ); $select = undef;
        $sock->close; $sock = undef;

        $! = 0;

        return $reply || '{}';
    }

    $! = 0 ; $@ = 0;
}

my $server_status_tic = $p - 5;
sub get_server_status {
    # do_log "get_server_status\n";

    if ( $server_status_tic++ < $p ) {
        return;
    }

    for my $instance_id ( keys %$servers ) {
        my $server = $servers->{$instance_id};

        if ( $server->{checktic} > $p ) {
            next;
        }

        $server->{checktic} = $p + 10;

        my $jsonobj = JSON->new;
        $jsonobj->relaxed(1);

        my $serverinfo_response = send_command_to_server ( $server, "serverinfo" ) ;
        my $playerinfo_response = send_command_to_server ( $server, "playerinfo" ) ;

        my $serverinfo = undef;
        my $playerinfo = undef;

        try {
            $serverinfo = $jsonobj->utf8->decode( $serverinfo_response );
            $playerinfo = $jsonobj->utf8->decode( $playerinfo_response );
        } catch {
            do_log "failed to decode json $! $_";
            do_log "server response: '$serverinfo_response'";
            do_log "player response: $playerinfo_response";
        };

        if ( $server->{error} ) {
            do_log "ERROR: SERVER" . Dumper $server->{error}, $server;
            next;
        } elsif ( $serverinfo->{error} ) {
            do_log "ERROR: SERVERINFO" . Dumper $serverinfo->{error}, $serverinfo;
            next;
        } elsif ( $playerinfo->{error} ) {
            do_log "ERROR: PLAYERINFO" . Dumper $playerinfo->{error}, $playerinfo;
            next;
        }

        my $reply = {
            serverinfo => $serverinfo,
            playerinfo => $playerinfo,
        };

        $reply = encode_json ( $reply );

        try {
            $dbh->selectrow_arrayref (
                # update_instance_id uuid, pid integer, result server_spawn_request_result, spawn_reply text, port integer, mapname text
                "SELECT * FROM webapp.update_server_info(?,?,?,?,?);", undef,
                $instance_id,
                $serverinfo->{net_port},
                $serverinfo->{mapname} || 'not set',
                $reply,
                'running',
            );
        } catch {
            do_log "failed to update server status $! $@"; # TODO
        }
    }

    $server_status_tic = $p + 5;
}

sub destroy_servers {

    while ( my $pid = pop @child_pids ) {
        do_log "destroy_servers pid:$pid";

        my $server = find_server_by_pid ( $pid );
        if ( $server ) {
            # do_log "got server\n" . Dumper $server;

            my $config_path = $server->{config_path};
            if ( $config_path and -f $config_path ) {
                do_log "NOT unlinking $config_path\n";
                # unlink $config_path;
            }

            my $q3apath     = $config->{q3apath};
            my $q3logdir    = $config->{q3logdir};
            my $basegame    = $server->{spawn_req}->{basegame};
            my $instance_id = $server->{instance_id};

            my $log_path = _glob_path ( "${q3apath}/${basegame}/${q3logdir}/$instance_id.log" );

            update_running_server_by_pid ( $pid, 'finished', 'caught signal CHLD' );

            #pid file not being removed.  TODO here or q3ded?

            # todo journal
            #
            # time this TODO
            if ( -f $log_path ) {
                do_log "loading log $log_path into db";

                my $t1 = [ gettimeofday ];
                my $log_line_count = 0;

                if ( open my $fh, '<', $log_path ) {
                    # TODO TOOOO SLOOOWWWWWWW
                    while ( my $line = <$fh> ) {
                        chomp $line;
                        $dbh->do( "INSERT INTO q3serverlog ( instance_id, log_line ) VALUES ( ?, ? )", undef, $instance_id, $line );
                        $log_line_count++;
                    }
                    close $fh;
                    unlink $log_path;

                    do_log "Time to upload log ($log_line_count): " . tv_interval ( $t1 );

                } else {
                    do_log "failed to open log $log_path $!";
                }
            }


            # do_log "deleting instance_id:$instance_id server from \$servers";
            delete $servers->{$instance_id};

        } else {
            do_log "COULD NOT FIND pid in child_pids $pid";
        }

    }
}

sub list_servers { # TODO
    return Dumper $servers;
}

sub check_proxy_requests {

    my $proxy_requests = $dbh->selectall_hashref ( "SELECT * FROM webapp.get_proxy_requests()", 'request_id', { Slice => { } } );

    if ( $proxy_requests and %$proxy_requests ) {

        do_log "received proxy request";

        for my $request_id ( keys %$proxy_requests ) {

            my $request = $proxy_requests->{$request_id};
            my $reqtext = $request->{request_text};
            my $sub     = $proxy_commands->{$reqtext};
            my $reply   = '';

            if ( $sub ) {
                $reply = $sub->(); # TODO args?
            } else {
                $reply = "no proxy command '$reqtext'";
                do_log "check_proxy_requests: $reply. \n" . Dumper $proxy_commands;
            }

            $dbh->do("SELECT update_proxy_request(?, ?)", undef, $request_id, $reply);
        }
    }
}

sub q3server_requests {
    my $commands = $dbh->selectall_hashref ( "SELECT * FROM webapp.get_requests();", 'request_id', { Slice => {} } );

    if ( $commands and %$commands ) {
        do_log "processing q3server_request";
        server_send_command($commands);
    } else {
        # do_log "NOT processing q3server_request.  no command";
    }
}

sub q3server_spawn_requests {
    my $spawn_reqs = $dbh->selectall_hashref ( "SELECT * FROM webapp.get_server_spawn_requests();", 'instance_id', { Slice => { } } );

    if ( $spawn_reqs and %$spawn_reqs ) {

        do_log "processing server spawn request";

        for my $instance_id ( keys %$spawn_reqs ) {

            my $spawn_req = $spawn_reqs->{$instance_id};

            my $result      = server_write_config ( $spawn_req );

            my $config_path = $result->{config_path};
            my $error       = $result->{error};

            my $pid = 0;

            if ( $config_path and not $error ) {
                $result = server_spawn ( $spawn_req );
                $error  = $result->{error};

                unless ( $error ) {
                    $pid    = $servers->{$instance_id}->{pid};
                    $servers->{$instance_id}->{config_path} = $config_path;
                }

                $dbh->selectrow_arrayref (
                    "SELECT * FROM webapp.update_server_spawn_request(?,?,?,?);", undef,
                    $instance_id, $pid, $error ? 'failed' : 'success', $error
                );
            }

            if ( $error ) {
                do_log "server_spawn error: $error\n";
            }
        }
    }
}

sub server_send_command {
    my $commands = shift;

    # TODO
    # responses from ioq3dedws.x86_64 need to be updated so this script can convert to json
    # for all commands in server_console_commands

    # may need a 'protocol' so proxy and ioq3ded know the state of a request/response
    #
    # hunklog should be blocked. It dumps a lot of data

    do_log 'server_send_command';

    for my $request_id ( keys %$commands ) {

        do_log "server_send_command request_id $request_id";

        my $command     = $commands->{$request_id};
        my $instance_id = $command->{instance_id};
        my $server      = $servers->{$instance_id};

        my $reply = '';

        if ( $server ) {

            if ( ! server_is_running ( $instance_id ) ) {

                $reply = "server_send_command: server_is_running($instance_id) returned false";

            } else {

                ( my $sock, $reply ) = open_connection_to_server( $server->{sockpath} );

                if ( $sock ) {
                    unless ( $sock->connected ) {
                        do_log "socket not connected";
                        next;
                    }

                    my $cmd_text = $command->{request_command};
                    my $cmd_args = $command->{request_arguments} || '';

                    $cmd_text .= " $cmd_args" if $cmd_args;
                    chomp $cmd_text;

                    do_log "sending cmd '$cmd_text\n'";

                    $sock->send( "$cmd_text\n" );
                    $sock->flush;
                    $reply = '';

                    my $select = IO::Select->new( $sock );

                    $! = 0;
                    if ( $select->can_read ( 0.15 ) ) {
                        # do_log "can read";
                        # add test function test_write.. to the C code to return a bunch of data
                        my $data;
                        while ( $sock->recv( $data, 4096, MSG_DONTWAIT ) ) {
                            do_log "recv " . ( length $data ) . "'$data'" ;
                            $reply .= $data if $data;
                            $data = undef;
                        }
                    }

                    do_log "read error: $!" if $! and $! ne 'Resource temporarily unavailable';
                    do_log "read complete\n";

                    $select->remove ( $sock );
                    $select = undef;

                    $sock->close;
                    $sock = undef;
                }
            }
        } else {
            $reply = "server $instance_id not found in servers";
        }

        do_log "server_send_command: reply:'$reply'\n";
        $dbh->do("SELECT post_response(?,?,?)", undef, $instance_id, $request_id, $reply);
    }
}

sub server_write_config {
    my $spawn_req   = shift;
    my $q3apath     = $config->{q3apath};
    my $basegame    = $spawn_req->{basegame} || 'baseq3'; # TODO FIXME
    my $instance_id = $spawn_req->{instance_id};
    #my $owner_id   = $spawn_req->{owner};
    my $config_uuid = $spawn_req->{config_uuid};

    my $config_path;
    $config_path  = _glob_path ( "${q3apath}/${basegame}/" );
    $config_path .= "${instance_id}.cfg";

    if ( open my $fh, '>', $config_path ) {
        my $config = $dbh->selectall_arrayref ( "SELECT config_name, config_value FROM get_server_config(?)", { Slice => { } }, $config_uuid );

        # do_log Dumper $config;

        my $written = 0;
        for my $config ( @$config ) {
            my $name  = $config->{config_name};
            my $value = $config->{config_value};

            $value = "\"$value\"" if length $value;
            my $config_line = "";

            $config_line .= "$name $value";

            print {$fh} "$config_line\n";
            $written = 1;
        }

        my $error = undef;
        $error = "no config" unless $written;

        close $fh;
        return ( { config_path => $config_path, error => $error } );
    }

    return ( { config_path => undef, error => "could not open $q3apath/$basegame/$instance_id.cfg for writing. $!" } );
}

sub server_spawn {
# check basegame existance
# check that server_name does not already exist
# spawn server, when the signal comes ( from where, the spawned process? ), update running_server table
    #
    # Use different directory other than .q3a
    # ! .q3a
    # +set com_homepath ~/q3x +set fs_homepath ~/q3x

    my $spawn_req = shift;
    my $path      = _glob_path ( $config->{q3dedpath} );
    my $exec      = $config->{q3dedname};
    my $q3apath   = $config->{q3apath};
    my $sockdir   = $config->{socketdir};
    my $sockext   = $config->{socketext};

    my $com_homepath = $config->{com_homepath} || '';
    my $fs_homepath  = $config->{fs_homepath}  || '';

    my $instance_id = $spawn_req->{instance_id};
    my $basegame    = $spawn_req->{basegame};
    my $map_name    = $spawn_req->{mapname} // 'q3ctf1';
    my $server_name = $spawn_req->{server_name};
    my $port_number = $spawn_req->{port};

    if ( server_is_running ( $instance_id ) ) {
        # this should never happen TODO
        return ( { error => "server already running" } );
    }

    # ~/Projects/IOQuake3/ioq3wasm/build/ioq3dedws.x86_64 +set com_sockfile sockets/q3ctf1.socket +map q3ctf1
    my $exe      = "$path/$exec";
    my $inst_arg = "set com_instance_id $instance_id";
    my $exec_arg = "+exec $instance_id.cfg";
    my $port_arg = "+net_port $port_number";

    my $com_homepath_arg = $com_homepath ? "+set com_homepath $com_homepath" : '';
    my $fs_homepath_arg  =  $fs_homepath ? "+set  fs_homepath  $fs_homepath" : '';

    my $args     = "$inst_arg $port_arg $exec_arg $com_homepath_arg $fs_homepath_arg"; # removed $map_arg
    my $sockpath = "$q3apath/$basegame/$sockdir/$instance_id$sockext";

    if ( $basegame ne 'baseq3' ) {
        $args += " +set com_basegame $basegame";
        die "need to test this";
    }

    my @args = ( $exe, split / /, $args );

    if ( ! grep /com_instance_id/, $args ) {
        return ( { error => "ERROR: +set com_instance_id <uuid> MUST be defined. command line: '$args'" } );
    }

    if ( $VALGRIND ) {
        my $q3apath = _glob_path ( $q3apath );
        @args = ("valgrind", "--track-origins=yes", "--show-leak-kinds=all", "--log-file=$q3apath/baseq3/log/${instance_id}-valgrind.log", "--leak-check=full",  @args );
        do_log Dumper @args;
    }

    do_log "spawn server: @args";

    my $pid = fork();

    if ( $pid ) {
        $servers->{$instance_id} = {
            instance_id => $instance_id,
            name      => $server_name,
            args      => $args,
            pid       => $pid,
            sockpath  => _glob_path ( $sockpath ),
            spawn_req => $spawn_req,
            checktic  => $p + 5, # TODO
        };

        return ( { error => undef } );
    }

    close STDIN;
    close STDOUT;
    close STDERR;

    exec { $args[0] } @args;
}

# override the 'status' and other commands to convert output to json
sub server_is_running {
    my $instance_id = shift;

    for my $server ( keys %$servers ) {
        if ( $server eq $instance_id ) {
            my $pid = $servers->{$instance_id}->{pid};
            if ( $pid ) {
                return kill 0, $pid;
            } else {
                do_log "server_is_running; pid not defined for instance: $instance_id";
            }
        }
    }

    return 0;
}

sub do_exit {

    do_log "do_exit proxy logout";
    $dbh->do( "SELECT proxy_logout(?)", undef, $$ );

    do_log "do_exit shut_down_servers";
    &shut_down_servers();

    do_log "do_exit destroy_servers";
    &destroy_servers();

    do_log "do_exit disconnect";
    $dbh->disconnect;

    exit;
}

sub _glob_path {
    my $path = shift;

    if ( $path =~ /^~/ ) {
        $path = ( $path ) =~ s/^~/$homedir/r;
    }

    return $path;
}

sub load_config {
    # TODO get config from database ONLY

    if ( open ( my $cfgfile, '<', 'proxy.cfg' ) ) {
        while ( my $line = readline ( $cfgfile ) ) {
            next unless $line =~ /=/;
            chomp $line;

            $line =~ s/^\s+|\s+$//g;

            next if $line =~ /^$/;
            next if $line =~ m|^//|;

            ( $line, undef ) = split /\/\//, $line;

            my ( $n, $v ) = split /=/, $line, 2;

            $n =~ s/^\s+|\s+$//g;
            $v =~ s/^\s+|\s+$//g;


            $config->{$n} = $v;
        }

        close $cfgfile;
        return ;
    }

    die "could not load config";
}

sub open_connection_to_server {
    my $sockpath = shift;
    my $error    = undef;
    my $socket   = undef;

    # kill 0, $pid; to see if alive before creating? TODO
    # do_log "open_connection_to_server: connecting to UNIX socket $sockpath\n";

    if ( -e $sockpath ) {
        if ( -S $sockpath ) {

            $socket = IO::Socket::UNIX->new (
                Type    => SOCK_STREAM(),
                Peer    => "$sockpath",
                Timeout => 5,
            );

            if ( $socket ) {
                $socket->autoflush(1);
            } else {
                $error = "failed to create PEER socket $IO::Socket::errstr";
            }
        } else {
            $error = "$sockpath not a socket";
        }
    } else {
        $error = "$sockpath does not exist";
    }

    unless ( $socket ) {
        $error = "open_connection_to_server: failed to connect '$sockpath' '$error'";
    }

    $@ = 0 ; $! = 0;

    return ( $socket, $error );
}

sub shut_down_servers {
    # TODO instead of killing all children seperately?
    # kill -$$

    do_log "shut_down_servers:";

    for my $instance_id ( keys %$servers ) {

        my $server = $servers->{$instance_id};
        my $pid    = $server->{pid};
        my $error  = '';

        if ( kill 0, $pid ) {
            ( my $sock, $error ) = open_connection_to_server( $server->{sockpath} );

            if ( $sock ) {
                do_log "sent 'quit' to server ";
                $sock->print("quit\n");
                $sock->close;
                $sock = undef;
            } else {
                $error = "failed to connect to $instance_id over UNIX socket to send quit command" unless $error;
            }
            push @child_pids, $pid;
        } else {
            $error = "process does not exist $!";
        }

        update_running_server_state( $instance_id, 'finished', 'shutting down' );

        do_log $error if $error;
    }
}

sub find_server_by_pid {
    my $pid = shift;
    for my $instance_id ( keys %$servers ) {
        if ( $pid eq $servers->{$instance_id}->{pid} ) {
            return $servers->{$instance_id};
        }
    }

    return undef;
}

sub update_running_server_state {
    my $instance_id = shift;
    my $state       = shift;
    my $reason      = shift || '';

    my $server = $servers->{$instance_id};

    if ( $server ) {
        $dbh->selectrow_arrayref ( "SELECT * FROM update_server_state(?,?,?)", undef, $server->{instance_id}, $state, $reason );
    } else {
        do_log "ERROR; no server for instance id $instance_id";
    }
}

sub update_running_server_by_pid {
    my $pid    = shift;
    my $state  = shift;
    my $reason = shift;

    my $server = find_server_by_pid ( $pid );

    if ( $server ) {
        $dbh->selectrow_arrayref ( "SELECT * FROM update_server_state(?,?,?)", undef, $server->{instance_id}, $state, $reason );
    } else {
        do_log "update_running_server: WARNING: server not found by pid:$pid"; # TODO always here
    }
}

my $recv_sig_term = 0;
sub signal_handler {
    my $signal = shift;
    do_log "signal: $signal\n";

    if ( $signal =~ /INT|TERM/ ) {

        if ( $recv_sig_term ) {
            die "double INT";
        }

        $recv_sig_term = 1;
        $run = 0;
    } elsif ( $signal =~ /CHLD/ ) {
        # TODO move this out of signal handler!!!
        # TODO have to remove this server from $servers
        my $pid;
        do {
            $pid = waitpid(-1, WNOHANG);

            if ( $pid > 0 ) {
                push @child_pids, $pid;
                do_log " pid: $pid '$?'\n";
            }
        } while $pid > 0;
    } elsif ( $signal =~ /PIPE/ ) {
        do_log "SIGPIPE";
    }
}

sub do_log {
    my $msg  = shift;
    my (undef, $micro) = split /\./, gettimeofday;
    my $date = strftime "%Y%m%d %H:%M:%S.", localtime;

    $date .= sprintf("%05d", $micro);

    print {$logfh} "$date: $msg\n";
    print          "$date: $msg\n";
}

sub open_log {
    my $logfilename = _glob_path ( "$config->{logpath}/$config->{logfile}" );
    $logfilename =~ s|//|/|g;

    open $logfh, '>>', $logfilename or die "could not open logfile $logfilename for appending $!";
    $logfh->autoflush(1);

    do_log "$0 starting up";
    do_log "Opened log $$";
}

sub do_die {
    my $msg = shift;
    do_log "Die: $msg";

    $dbh->disconnect if $dbh;
    die $msg;
}

sub connect_to_database {
# https://unix.stackexchange.com/questions/252596/dbd-dbi-crashes-if-a-program-is-forked
# If the database activity is confined to the parent,
# you may set AutoInactiveDestroy early on the db handle (since DBI 1.614).
# This should set InactiveDestroy automatically in the childs and just solve the problem. See InactiveDestroy in DBI documentation:
#
# https://metacpan.org/pod/DBI#AutoInactiveDestroy
#
#  child would exit, calling DESTROY which would close the shared db handle

    $dbh = DBI->connect( $config->{dsn}, $config->{user}, $config->{pass},
    {
        PrintError          => 1, # TODO
        RaiseError          => 1, # TODO
        AutoInactiveDestroy => 1
    } ) or do_die "Could not open connection to database $DBI::errstr";

    do_log "Setting search_path to $config->{search_path}";
    $dbh->do( "set search_path to $config->{search_path}" );
}

sub initialize {

    load_config();

    open_log();

    # check that config is correct and there is a ioq3ded executable
    my $ioq3ded = _glob_path($config->{q3dedpath}).$config->{q3dedname};
    unless ( -X $ioq3ded ) {
        do_die "$ioq3ded path is not an executable";
    }

    connect_to_database();

    # check that we are the only instance of proxy.pl
    # have to logout at close TODO
    do_log "logging in";
    unless ( $dbh->selectrow_hashref ( "SELECT * FROM proxy_login(?)", undef, $$ ) ) {
        do_die "$0 failed to login to postgres, already proxies running $DBI::errstr";
    }

    # kill already running processes...
    my $q3dedname = substr( $config->{q3dedname}, 0, 15 );
    system "pkill $q3dedname"; # TODO

    $SIG{TERM} = \&signal_handler;
    $SIG{INT}  = \&signal_handler;
    $SIG{PIPE} = \&signal_handler; # TODO
    $SIG{CHLD} = \&signal_handler; # TODO
}

