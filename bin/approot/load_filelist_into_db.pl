#!/usr/bin/perl
# Load the all.assets filelist into database
# associate asset to map via map_assets table
#
# Usage: perl load_filelist_into_db.pl testmap /tmp/tmp.Nt6aJTnjYu/assets.<mapname>

use strict;
use warnings;

use Data::Dumper;
use File::Basename;
use DBI;

sub Usage;
my $config;

load_config();

my $mapname  = $ARGV[0] || Usage;
my $filelist = $ARGV[1] || Usage;

if ( ! -f $filelist or ! $mapname ) {
    Usage;
}

my $dbh = DBI->connect( $config->{dsn}, $config->{user}, $config->{pass},
{
    PrintError          => 1, # TODO
    RaiseError          => 1, # TODO
    AutoInactiveDestroy => 1
} ) or die "Could not open connection to database $DBI::errstr";

open my $fh, '<', $filelist or die "Could not open $filelist for reading $!";

my $map_id = $dbh->selectrow_hashref( "INSERT INTO map ( mapname, filename ) VALUES ( LOWER(?), LOWER(?) ) ON CONFLICT DO NOTHING RETURNING map_id", { Slice => { } }, $mapname, "$mapname.bsp" );

if ( $map_id ) {
    $map_id = $map_id->{map_id};
} else {
    $map_id = $dbh->selectrow_hashref( "SELECT map_id FROM map WHERE LOWER(mapname) = LOWER(?)", { Slice => { } }, $mapname );
    die "no map_id" unless $map_id;
    $map_id = $map_id->{map_id};
}

print "map_id: $map_id\n";

while ( my $line = <$fh> ) {
    chomp $line;

    $line =~ s/^\s+|\s+$//g;

    # my $dirname  = dirname  ( $line );
    # my $filename = basename ( $line );

    my $match    = $dbh->selectrow_hashref("SELECT asset_id FROM assets WHERE LOWER(path || '/' || filename) = LOWER(?)", undef, $line);

    if ( $match ) {
        my $asset_id  = $match->{asset_id};
        # insert meta into db
        print "asset_id $asset_id filename: $line\n";
        $dbh->do ( "INSERT INTO map_assets ( asset_id, map_id ) values ( ?, ? ) ON CONFLICT DO NOTHING", undef, $asset_id, $map_id );
    } else {
        # die "no match for $line ";
        print STDERR "$0 No match for '$line' in assets table\n";
    }
}

$dbh->disconnect;

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

sub Usage {
    die "Usage: $0 <pakfilename> /tmp/<dir>/all.assets";
}

