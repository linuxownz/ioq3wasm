#!/usr/bin/perl

# for file in $(file approot/* | grep Ogg | grep stereo | cut -d':' -f1); do file=${file/approot\//}; echo $file; perl ./tool_get_filename_for_hash.pl $file; done

use strict;
use warnings;

use DBI;
use Data::Dumper;
use File::Basename;

my $filename = $ARGV[0];

if ( ! $filename ) {
    print "Usage: $0 <hash>";
    exit 1;
}

my $config;
load_config();

my $dbh = DBI->connect( $config->{dsn}, $config->{user}, $config->{pass},
{
    PrintError          => 1, # TODO
    RaiseError          => 1, # TODO
    AutoInactiveDestroy => 1
} ) or die "Could not open connection to database $DBI::errstr";

my $fileinfo = $dbh->selectrow_hashref ( "select md5hash  from assets where path || '/' || filename = ?", undef, $filename );
# die Dumper $fileinfo;

if ( $fileinfo ) {
    print $fileinfo->{md5hash}, " $filename\n";
} else {
    print "$filename NOT FOUND\n";
}

sub load_config {
    # TODO get config from database ONLY

    if ( open ( my $cfgfile, '<', 'load_assets.cfg' ) ) {
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

