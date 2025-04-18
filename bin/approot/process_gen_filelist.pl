#!/bin/perl

# process the file that ./gen_filelist_from_map creates
#
# 1) ./gen_filelist_from_map writes to a file.  ARGV[0] is the input filename.
# 2) query database for all shaders known
#    build a filelist and load all of them into a single string
# 3) open filename and read
#       first the shader names and if shader not found, is name of png
#       second the spawns can be parsed for model, noise and music
# 4) output what is found

use strict;
use warnings;

use DBI;
use Data::Dumper;

my $config;
open my $cnf, '<', 'process.cfg' or die "Could not open process.cfg $!";
while ( <$cnf> ) {
    chomp;
    s/^\s+|\s+$//g;
    next if /^$/;

    if ( /(\w+)=([a-z0-9~\.\/:;=]+)/i ) {
        $config->{$1} = "$2";
    }
}
close $cnf;
# die Dumper $config;

# need input file, ~/q3a/ shader list and this pak file shader list if any
my $APPROOT = $config->{"APPROOT_DIR"}; # APPROOT_DIR=~/Projects/IOQuake3/ioq3wasm/approot

if ( $ENV{HOME} && $APPROOT =~ /~/ ) {
    $APPROOT =~ s/\~/$ENV{HOME}/;
}
#die $APPROOT;

my $genfilelist = $ARGV[0] || "";
if ( ! -f $genfilelist ) {
    die "usage: $0 <genfilelist> ( output from ./gen_filelist_from_map )";
}

my $dbh = DBI->connect( $config->{dsn}, $config->{user}, $config->{pass},
{
    PrintError          => 1, # TODO
    RaiseError          => 1, # TODO
} ) or die "Could not open connection to database $DBI::errstr";
# do_log "Setting search_path to $config->{search_path}";
$dbh->do( "set search_path to $config->{search_path}" );

# get a list of all shaders
my $shaderlist = $dbh->selectcol_arrayref(
    "SELECT DISTINCT md5hash || ' ' || ( SELECT CASE WHEN path != '' THEN path || '/' ELSE '' END ) || a.filename as filename
    FROM assets a
    WHERE a.filename like '%.shader'
    ORDER BY filename;"
);
# die Dumper $shaderlist;

# open those shaders to combine into large single string $shadertext
my $shadertext = '';
for my $shader ( @$shaderlist ) {
    my ( $hash, $filename ) = split /\s+/, $shader;

    chomp $filename;
    print "$filename\n"; # add all shaders to filelist TODO test

    {
        my $shaderfile = "$APPROOT/$hash";

        open my $fh, '<', $shaderfile or die "could not open $shaderfile $!";
        local $/ = undef;
        $shadertext .= <$fh>;
        close $fh
    }
}
# die $shadertext;

die 'TGA!' if $shadertext =~ /\.tga/;

# open my $fho, '>shadertext.txt' or die "Could not open shadertext.txt for writing";
# print $fho $shadertext;
# close $fho;

my @map_types = qw ( alphaMap animMap clampmap q3map_lightimage qer_editorimage map );

my @sound_exts = qw(wav ogg);
my @text_exts  = qw(bmp png jpg jpeg tga);
my @model_exts = qw(md3);

my @exts = (
    @sound_exts,
    @text_exts,
    @model_exts
);
# die Dumper \@exts;

my @results;

sub categorize_ext {
    my $ext = shift;

    grep /$ext/, @text_exts  and return "texture";
    grep /$ext/, @sound_exts and return "sound";
    grep /$ext/, @model_exts and return "model";

    return "UNKNOWN";
}

sub MAX {
    my $a = shift;
    my $b = shift;

    if ( $a > $b ) {
        return $a;
    }

    return $b;
}

sub shader_text {
    my $shadertext = shift;
    my $line       = shift;

    my $sindex     = index($shadertext, $line, 0);

    return undef if $sindex < 0;

    $sindex += length ( $line ) + 1;
    my $eindex = $sindex;

    # print "start: $sindex\n";

    my $char       = substr $shadertext, $eindex, 1;

    while ( $char ne '{' ) {
        $char = substr $shadertext, $eindex++, 1;
    }

    my $brace = 0;
    while ( 1 ) {
        $brace++ if $char eq '{';
        $brace-- if $char eq '}';

        if ( $brace == 0 ) {
            # print "end $eindex\n";
            last;
        }

        $eindex++;
        $char = substr $shadertext, $eindex, 1;
    }

    # die "$sindex, $eindex";
    return substr $shadertext, $sindex, $eindex - $sindex + 1;
}

my $g_items = { };

# get g_item list from ./g_items
open my $gh, '-|', './g_items' or die "$!";
while(my $line = <$gh>) {
    chomp $line;
    my ($classname, @rest ) = split /\s+/, $line;
    if ( $classname ) {
        $g_items->{$classname} = \@rest;
    }
}
close $gh;
# die Dumper $g_items;

open my $fh, '<', $genfilelist or die "$!";

my $past_shaders = 0;
# each line is a name of a shader or the name of a texture if shader not found
while ( my $line = <$fh> ) {
    chomp $line;
    next if $line =~ /^\s*$/;
    next if $line =~ /^\s*\/\//;

    if ( $line =~ /^\{|^\}/ ) {
        $past_shaders = 1;
        next;
    }

    if ( ! $past_shaders ) {

        next if $line =~ /noshader/;
        next if $line =~ /caulk/;

        # look in shadertext for a match on line which is the name of the shader
        my $stxt = shader_text ( $shadertext, $line );

        if ( ! $stxt ) {
            # print "'$shadertext' '$line' not found"; exit;
            push @results, "$line.png";
            next;
        }

        process_shader ( $stxt, $line );
        next;
    }

    if ( $line =~ /"(?:music|model|noise)" "(.*)"/ ) {
        push @results, $1 if $1 !~ /\*\d/;
    } elsif ( $line =~ /"classname".*"(.*?)"/ ) {
        my $classname = $1;

        if ( $g_items->{$classname} ) {
            my $g_item = $g_items->{$classname};
            # print Dumper $line, $g_item;

            for my $item ( @$g_item ) {
                next if ! $item;

                if ( $item =~ /\.md3$/ ) {
                    # look up hash filename of .md3 file from database
                    # run loadmd3 ./approot/$hash it prints to STDOUT

                    push @results, "$item\n"; #TODO a shader here doesn't get processed
                    my $hash = $dbh->selectrow_hashref ( "select md5hash from assets where path || '/' || filename = ?", { Slice => { } }, $item );
                    if ( $hash ) {
                        # print Dumper $hash;
                        $hash = $hash->{md5hash};

                        open my $mh, '-|', "./loadmd3 ./approot/$hash" or die "$!"; # system "./loadmd3 ./approot/$hash";
                        while ( <$mh> ) {
                            chomp;
                            push @results, $_;
                        }
                        close $mh;
                    }
                } elsif ( $item =~ /\.ogg|\.wav|\.png|\.jpe?g|\.bmp/ ) {
                    push @results, "$item\n"; #TODO a shader here doesn't get processed
                } else {
                    my $stxt = shader_text ( $shadertext, $item );
                    if ( $stxt ) {
                        process_shader ( $stxt, $line );
                    } else {
                        print STDERR "failed to find match for shader $item\n";
                    }
                }
            }
        } else {
            print STDERR "no match for classname $classname\n";
        }
    }
}

for my $result ( sort @results ) {
    next if $result =~ /^\$/;
    next if $result =~ /^\*/;
    next if $result =~ /^\s*$/;

    chomp $result;
    print "$result\n";
}

sub process_shader {
    my $stxt = shift;
    my $line = shift;

    # print "SHADER TEXT: $stxt\n";

    for my $shader_line ( split /\r|\n/, $stxt ) {
        chomp $shader_line;

        next if $shader_line =~ /^\s*\/\//; # line commented out
        next if $shader_line =~ /^\s*$/;
        next if $shader_line =~ /\s*{/;
        next if $shader_line =~ /\s*}/;

        $shader_line =~ s/^\s+|\s+$//g;
        # print "shader_line: '$shader_line'\n";

        my $processed = 0;
        for my $map_type ( @map_types ) {
            if ( $shader_line =~ /$map_type\s+/is ) {
                my $start = $-[0];
                my $end   = $+[0];

                my $result = substr $shader_line, $end;
                $result =~ s/^\s+|\s+$//g;

                # print "> $result\n";
                my @textures = split /\s+/, $result;

                if ( $map_type eq 'animMap' ) {
                    shift @textures;
                }

                for my $texture ( @textures ) {
                    # print "pushing result $texture\n";
                    push @results, $texture;
                }

                $processed = 1;
                last;
            }
        }

        next if $processed;

        if ( ! $processed ) {
            if  ( $shader_line =~ /q3map_surfacelight|light|surfaceparm|rgbGen|blendFunc|geomtrn|tcmod/i ) {
                # TODO ?
                print STDERR "pass? $shader_line\n";
            } elsif ( $shader_line =~ /png/ ) {
                # print "$line.png\n";
                # TODO if there is a match, add shader filename to filelist
                # TODO keep track of what shader is used based on offset
                # die Dumper $stxt, $shader_line, $line; die Dumper $stxt;
                push @results, "$line.png";
            }
        }
    }
}

# "ambient"
# "angle"
#  "classname"
# "_color"
# "dmg"
# "gametype"
# "light"
# "message"
#  "model"
#  "music"
#  "noise"
# "notbot"
# "notcctf"
# "notq3a"
# "notq3ayescctf"
# "notta"
# "origin"
# "spawnflags"
# "target"
# "targetname"
# "team"
