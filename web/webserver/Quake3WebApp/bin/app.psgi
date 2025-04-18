#!/usr/bin/perl

use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin/../lib";


# use this block if you don't need middleware, and only have a single target Dancer app to run here
use Quake3WebApp;

Quake3WebApp->to_app;

=begin comment
# use this block if you want to include middleware such as Plack::Middleware::Deflater

use Quake3WebApp;
use Plack::Builder;

builder {
    enable 'Deflater';
    Quake3WebApp->to_app;
}

=end comment

=cut

=begin comment
# use this block if you want to mount several applications on different path

use Quake3WebApp;
use Quake3WebApp_admin;

use Plack::Builder;

builder {
    mount '/'      => Quake3WebApp->to_app;
    mount '/admin'      => Quake3WebApp_admin->to_app;
}

=end comment

=cut

