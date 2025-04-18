#!/usr/bin/perl
#
# Working

use strict;
use warnings;

use Data::Dumper;
use File::Find;
use File::Basename;

my $src_dir = ( "$ARGV[0]/models/" );
# die Dumper $src_dir;

my $re = qr/.*\.(wav|ogg|bmp|png|jpe?g|tga|md3)/;

sub process_source {
    my $dirname  = $File::Find::name;
    my $filename = basename($dirname);

    if ( $filename =~ /\w+\.skin$/ ) {

        # print "opening $dirname\n";
        open my $file, '<', $dirname or die "$! $@";
        while ( my $content = <$file> ) {
            chomp $content;
            next if $content =~ /^\s*$/;

            # print "$content\n";
            my ( undef, $asset ) = split /\s*,\s*/, $content;

            if ( $asset and $asset =~ /$re/ ) {

                my $assetext  = $1;

                # next if $assetname =~ /\%/;

                die "OGG?????" if $assetext eq 'ogg';

                if ( $assetext && $assetext =~ /bmp|jpg|jpeg|tga/ ) {
                    $assetext = 'png';
                }

                # print Dumper $regtype, $assetname, $assetext, $content;

                print "$asset";
                #print ".$assetext" if $assetext;
                # print "($regtype)" if $regtype;
                print "\n";
            }
        }
    }
}

find(\&process_source, $src_dir); # or die "$? $!";

