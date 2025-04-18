#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;
use Cwd qw/abs_path/;
use File::Find;
use File::Path qw/ make_path /;
use File::Copy;
use Digest::MD5 qw(md5 md5_hex md5_base64);
use DBI;

sub Usage;
my $config;
load_config();

my $ROOT_DIR = "~/Projects/IOQuake3/ioq3wasm/approot";

$ROOT_DIR = $config->{root_dir};
$ROOT_DIR =~ s|^~|$ENV{HOME}|; #die $ROOT_DIR;

my $src_dir = $ARGV[0] || die Usage;

$src_dir = abs_path($src_dir);

if ( ! -d $src_dir or $src_dir !~ m|^/tmp/| ) {
    die "Usage: $0 /tmp/<dir> to process";
}

my $dbh = DBI->connect( $config->{dsn}, $config->{user}, $config->{pass},
{
    PrintError          => 1, # TODO
    RaiseError          => 1, # TODO
    AutoInactiveDestroy => 1
} ) or die "Could not open connection to database $DBI::errstr";

sub process_source {
    my $fullpath = $File::Find::name;
    my $dirname  = $File::Find::dir;
    my $filename = $_;

    return unless -f $filename;

    open my $file, '<', $fullpath or die "File open failed $! $@ '$fullpath'"  ;
    my $md5hash  = Digest::MD5->new->addfile($file)->hexdigest;
    close $file;

    $fullpath =~ s|$src_dir/?||;
    $dirname  =~ s|$src_dir/?||;

    # print "src_dir:$src_dir fullname: $fullpath >path: $dirname >filename: $filename >md5hash: $md5hash\n";
    # TODO skip cfgs, etc

    # insert meta into db
    $dbh->do ( "INSERT INTO assets ( path, filename, md5hash ) values ( LOWER(?), LOWER(?), ? ) ON CONFLICT DO NOTHING", undef, $dirname, $filename, $md5hash );

    my $newname = "$ROOT_DIR/$md5hash";

    if ( -f $newname ) {
        print "Warning: New file $newname already exists!\n";
    } else {
        # copy and rename file as its hash and move to approot
        # copy because files still need to be processed
        copy( $File::Find::name, $newname ) or die "$!";
    }
}

find(\&process_source, $src_dir); # or die "$? $!";

$dbh->disconnect;

print "$src_dir\n";

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
    print "$0 tmpdir path\n $0 /tmp/tmp.TZwUdevEPd";
    exit ;
}

