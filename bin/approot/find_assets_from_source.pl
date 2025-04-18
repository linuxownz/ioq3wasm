#!/usr/bin/perl
#
# Working

use strict;
use warnings;

use Data::Dumper;
use File::Find;
use File::Basename;
use File::Slurper 'read_text';

# RegisterModel\|RegisterSound\|RegisterShader\|RegisterSkin
my $re = qr/Register(Model|Sound|SoundAsync|Shader|ShaderNoMip|Skin).*\(\s*"(.*?)(?:\.(\w\w\w))?"/;

my $old = $";
$" = ",";

my @wanted_dirs = qw( cgame client renderergl2 );
my @src_dirs    = glob("~/Projects/IOQuake3/ioq3wasm/code/{@wanted_dirs}/");
# die Dumper @src_dirs;

$" = $old;

my $use_ogg = $ENV{INCLUDE_OGG} || 0;

my @wanted_exts=qw( c h );

my @sound_exts = qw(wav ogg);
my @text_exts  = qw(bmp png jpg jpeg tga);
my @model_exts = qw(md3);

my @exts = (
    @sound_exts,
    @text_exts,
    @model_exts
);

sub categorize_ext {
    my $ext = shift;

    grep /$ext/, @text_exts  and return "texture";
    grep /$ext/, @sound_exts and return "sound";
    grep /$ext/, @model_exts and return "model";

    return "UNKNOWN";
}

sub process_source {
    my $dirname  = $File::Find::name;
    my $filename = basename($dirname);

    if ( $filename =~ /\w+\.(\w{1}$)/ ) {
        my $ext = $1;

        if ( ! grep $ext, @wanted_exts ) {
            print STDERR "Skipping $filename\n";
            return;
        }
        # die "$filename with good ext $ext";

        die "$! $@" unless open my $file, '<', $dirname ;
        while ( my $content = <$file> ) {
            chomp $content;

            if ( $content =~ /$re/ ) {
                my $regtype   = $1;
                my $assetname = $2;
                my $assetext  = $3;

                if ( $assetname =~ /\%/ ) {
                    print STDERR "asset name with % $assetname.  skipping\n";
                    next;
                }

                # print "'$regtype' '$assetname' '$assetext'\n";
                # next if ! $assetext; # blocked gfx/2d/numbers

                if ( $assetext ) {
                    if ( $assetext =~ /wav/ && $use_ogg ) {
                        $assetext = 'ogg';
                    } elsif ( $assetext =~ /ogg/ && ! $use_ogg ) {
                        $assetext = 'wav';
                    } elsif ( $assetext =~ /bmp|jpg|jpeg|tga/ ) {
                        $assetext = 'png';
                    }

                    # print Dumper $regtype, $assetname, $assetext, $content;
                    print "$assetname.$assetext\n";

                } else {
                    print "shader?$assetname\n";
                }
            }
        }
    } else {
        print STDERR "! $filename\n";
    }
}

find(\&process_source, @src_dirs); # or die "$? $!";

