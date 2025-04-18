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

my $dbh = DBI->connect( $config->{dsn}, $config->{user}, $config->{pass},
{
    PrintError          => 1, # TODO
    RaiseError          => 1, # TODO
    AutoInactiveDestroy => 1
} ) or die "Could not open connection to database $DBI::errstr";

my @md5hashes = @{$dbh->selectcol_arrayref( "SELECT distinct md5hash FROM assets", undef, )};
# die Dumper @md5hashes;

for my $hash ( @md5hashes ) {
    chomp $hash;

    my $exist = -f "../../approot/$hash";

    unless ( $exist ) {
        print "$hash does not exist\n";
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

