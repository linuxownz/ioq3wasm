#!/usr/bin/perl

use strict;
use warnings;

use IO::Socket::UNIX;
use IO::Select;

sub usage {
    die "Usage $0 ~/q3a//baseq3/sockets/<UUID>.socket";
}

sub do_log {
    print shift;
}

sub open_connection_to_server {
    my $sockpath = shift;
    my $error    = undef;
    my $socket   = undef;

    # kill 0, $pid; to see if alive before creating? TODO
    do_log "open_connection_to_server: connecting to UNIX socket $sockpath\n";

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

    return ( $socket, $error );
}

my $sockpath = $ARGV[0] || usage;
my $command  = $ARGV[1] || usage;

printf "connecting to $sockpath sending command $command\n";

my $reply = '';
my ( $sock, $error ) = open_connection_to_server($sockpath);

if ( $sock ) {
    $sock->send( "$command\n" );
    $sock->flush;

    my $select = IO::Select->new( $sock );

    $! = 0;
    if ( $select->can_read ( 5 ) ) {
        # do_log "can read";
        # add test function test_write.. to the C code to return a bunch of data
        my $data;
        while ( $sock->recv( $data, 4096, MSG_DONTWAIT ) ) {
            # do_log "recv " . ( length $data ) . "'$data'" ;
            $reply .= $data if $data;
            $data = undef;

            sleep 0.15;
        }
    }

    print "read error: $!" if $! and $! ne 'Resource temporarily unavailable';
    print "read complete\n";

    $select->remove ( $sock );
    $select = undef;

    $sock->close;
} elsif ( $error ) {
    die $error;
}

print $reply;

