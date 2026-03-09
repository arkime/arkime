#!/usr/bin/env perl
#
# SPDX-License-Identifier: Apache-2.0
#
# Mini Redis server for testing. Stores everything in memory.
# Non-blocking I/O with per-connection buffering to prevent one slow
# client from starving others.
# Usage: perl mini-redis.pl [--debug] <port>

use strict;
use warnings;
use IO::Socket::INET;
use POSIX qw(strftime);
use IO::Select;
use Fcntl qw(F_GETFL F_SETFL O_NONBLOCK);

my $debug = 0;
if (@ARGV && $ARGV[0] eq '--debug') {
    $debug = 1;
    shift @ARGV;
}

my $port = $ARGV[0] || 6379;

# Storage: $db{$dbnum}{$key} = { value => ..., expire => ... }
my %db;
my %conn_db;   # fileno => db number
my %conn_buf;  # fileno => read buffer

my $server = IO::Socket::INET->new(
    LocalPort => $port,
    Proto     => 'tcp',
    Listen    => 128,
    ReuseAddr => 1,
) or die "Cannot start server on port $port: $!\n";

print "mini-redis listening on port $port\n" if $debug;

my $sel = IO::Select->new($server);

while (1) {
    my @ready = $sel->can_read(1);

    # Expire keys lazily
    for my $dbn (keys %db) {
        for my $k (keys %{$db{$dbn}}) {
            if (defined $db{$dbn}{$k}{expire} && time() >= $db{$dbn}{$k}{expire}) {
                delete $db{$dbn}{$k};
            }
        }
    }

    for my $fh (@ready) {
        if ($fh == $server) {
            my $client = $server->accept();
            if ($client) {
                $client->autoflush(1);
                binmode($client);
                # Set non-blocking so reads never stall the event loop
                my $flags = fcntl($client, F_GETFL, 0);
                fcntl($client, F_SETFL, $flags | O_NONBLOCK);
                $conn_buf{fileno($client)} = '';
                $sel->add($client);
            }
            next;
        }

        # Read whatever is available into the per-connection buffer
        my $fn = fileno($fh);
        my $data;
        my $n = sysread($fh, $data, 65536);
        if (!defined $n) {
            next if $!{EAGAIN} || $!{EWOULDBLOCK};
            # Real error — disconnect
            delete $conn_db{$fn};
            delete $conn_buf{$fn};
            $sel->remove($fh);
            $fh->close();
            next;
        }
        if ($n == 0) {
            # EOF
            delete $conn_db{$fn};
            delete $conn_buf{$fn};
            $sel->remove($fh);
            $fh->close();
            next;
        }
        $conn_buf{$fn} .= $data;

        # Process as many complete commands as possible from the buffer
        while (1) {
            my ($cmd, $consumed) = parse_command($conn_buf{$fn});
            last unless defined $cmd;
            substr($conn_buf{$fn}, 0, $consumed, '');

            my @args = @$cmd;
            my $name = uc($args[0] // '');
            my $cdb = $conn_db{$fn} // 0;

            my $ts = strftime("%H:%M:%S", localtime);
            print "$ts REDIS: $name " . join(' ', map { defined $_ ? $_ : '(nil)' } @args[1..$#args]) . " [db=$cdb]\n" if $debug;

            if ($name eq 'PING') {
                send_simple($fh, 'PONG');
            } elsif ($name eq 'QUIT') {
                send_simple($fh, 'OK');
                delete $conn_db{$fn};
                delete $conn_buf{$fn};
                $sel->remove($fh);
                $fh->close();
                last;
            } elsif ($name eq 'SELECT') {
                my $dbn = $args[1] // 0;
                $conn_db{$fn} = int($dbn);
                send_simple($fh, 'OK');
            } elsif ($name eq 'SET') {
                my $key = $args[1];
                my $val = $args[2];
                if (!defined $key || !defined $val) {
                    send_error($fh, "ERR wrong number of arguments for 'set' command");
                    next;
                }
                my $entry = { value => $val };
                my $i = 3;
                while ($i < @args) {
                    my $opt = uc($args[$i] // '');
                    if ($opt eq 'EX' && defined $args[$i+1]) {
                        $entry->{expire} = time() + int($args[$i+1]);
                        $i += 2;
                    } elsif ($opt eq 'PX' && defined $args[$i+1]) {
                        $entry->{expire} = time() + int($args[$i+1]) / 1000;
                        $i += 2;
                    } else {
                        $i++;
                    }
                }
                $db{$cdb}{$key} = $entry;
                send_simple($fh, 'OK');
            } elsif ($name eq 'SETEX') {
                my $key = $args[1];
                my $seconds = $args[2];
                my $val = $args[3];
                if (!defined $key || !defined $seconds || !defined $val) {
                    send_error($fh, "ERR wrong number of arguments for 'setex' command");
                    next;
                }
                $db{$cdb}{$key} = { value => $val, expire => time() + int($seconds) };
                send_simple($fh, 'OK');
            } elsif ($name eq 'GET') {
                my $key = $args[1];
                if (!defined $key) {
                    send_error($fh, "ERR wrong number of arguments for 'get' command");
                    next;
                }
                my $entry = $db{$cdb}{$key};
                if ($entry && defined $entry->{expire} && time() >= $entry->{expire}) {
                    delete $db{$cdb}{$key};
                    $entry = undef;
                }
                if ($entry) {
                    send_bulk($fh, $entry->{value});
                } else {
                    send_null($fh);
                }
            } elsif ($name eq 'DEL') {
                my $count = 0;
                for my $i (1..$#args) {
                    if (delete $db{$cdb}{$args[$i]}) {
                        $count++;
                    }
                }
                send_integer($fh, $count);
            } elsif ($name eq 'EXISTS') {
                my $key = $args[1];
                my $entry = $db{$cdb}{$key};
                if ($entry && defined $entry->{expire} && time() >= $entry->{expire}) {
                    delete $db{$cdb}{$key};
                    $entry = undef;
                }
                send_integer($fh, $entry ? 1 : 0);
            } elsif ($name eq 'COMMAND') {
                send_raw($fh, "*0\r\n");
            } elsif ($name eq 'CLIENT') {
                send_simple($fh, 'OK');
            } elsif ($name eq 'INFO') {
                send_bulk($fh, "# Server\r\nmini_redis:1.0\r\n");
            } elsif ($name eq 'DBSIZE') {
                my $size = scalar keys %{$db{$cdb} // {}};
                send_integer($fh, $size);
            } elsif ($name eq 'FLUSHDB') {
                delete $db{$cdb};
                send_simple($fh, 'OK');
            } elsif ($name eq 'KEYS') {
                my $pattern = $args[1] // '*';
                my $re = $pattern;
                $re =~ s/\./\\./g;
                $re =~ s/\*/.*/g;
                $re =~ s/\?/./g;
                $re = qr/^$re$/;
                my @matching;
                for my $k (keys %{$db{$cdb} // {}}) {
                    my $entry = $db{$cdb}{$k};
                    if ($entry && defined $entry->{expire} && time() >= $entry->{expire}) {
                        delete $db{$cdb}{$k};
                        next;
                    }
                    push @matching, $k if $k =~ $re;
                }
                send_raw($fh, "*" . scalar(@matching) . "\r\n");
                for my $k (@matching) {
                    send_bulk($fh, $k);
                }
            } elsif ($name eq 'SETNX') {
                my $key = $args[1];
                my $val = $args[2];
                if (!defined $key || !defined $val) {
                    send_error($fh, "ERR wrong number of arguments for 'setnx' command");
                    next;
                }
                my $entry = $db{$cdb}{$key};
                if ($entry && (!defined $entry->{expire} || time() < $entry->{expire})) {
                    send_integer($fh, 0);
                } else {
                    $db{$cdb}{$key} = { value => $val };
                    send_integer($fh, 1);
                }
            } elsif ($name eq 'SHUTDOWN') {
                send_simple($fh, 'OK');
                exit(0);
            } else {
                send_error($fh, "ERR unknown command '$name'");
            }
        }
    }
}

# Try to parse one complete RESP command from a buffer string.
# Returns ($cmd_arrayref, $bytes_consumed) or (undef, 0) if incomplete.
sub parse_command {
    my ($buf) = @_;
    my $pos = 0;
    my $len = length($buf);

    return (undef, 0) if $len == 0;

    if (substr($buf, 0, 1) eq '*') {
        # RESP array
        my $eol = index($buf, "\r\n", $pos);
        return (undef, 0) if $eol < 0;
        my $count = int(substr($buf, 1, $eol - 1));
        $pos = $eol + 2;

        my @args;
        for (1..$count) {
            return (undef, 0) if $pos >= $len;
            if (substr($buf, $pos, 1) eq '$') {
                my $eol2 = index($buf, "\r\n", $pos);
                return (undef, 0) if $eol2 < 0;
                my $slen = int(substr($buf, $pos + 1, $eol2 - $pos - 1));
                $pos = $eol2 + 2;
                if ($slen < 0) {
                    push @args, undef;
                } else {
                    # Need slen bytes + \r\n
                    return (undef, 0) if $pos + $slen + 2 > $len;
                    push @args, substr($buf, $pos, $slen);
                    $pos += $slen + 2;
                }
            } else {
                # Inline element within array (shouldn't happen in proper RESP)
                my $eol2 = index($buf, "\r\n", $pos);
                return (undef, 0) if $eol2 < 0;
                push @args, substr($buf, $pos, $eol2 - $pos);
                $pos = $eol2 + 2;
            }
        }
        return (\@args, $pos);
    } else {
        # Inline command
        my $eol = index($buf, "\r\n", $pos);
        return (undef, 0) if $eol < 0;
        # Also accept bare \n
        if ($eol < 0) {
            $eol = index($buf, "\n", $pos);
            return (undef, 0) if $eol < 0;
        }
        my $line = substr($buf, $pos, $eol - $pos);
        $pos = $eol + 2;
        $pos = $eol + 1 if substr($buf, $eol, 2) ne "\r\n";
        my @parts = split(/\s+/, $line);
        return (undef, 0) unless @parts;
        return (\@parts, $pos);
    }
}

# RESP response helpers
sub send_simple {
    my ($fh, $msg) = @_;
    send_raw($fh, "+$msg\r\n");
}

sub send_error {
    my ($fh, $msg) = @_;
    send_raw($fh, "-$msg\r\n");
}

sub send_integer {
    my ($fh, $val) = @_;
    send_raw($fh, ":$val\r\n");
}

sub send_bulk {
    my ($fh, $data) = @_;
    if (!defined $data) {
        send_null($fh);
        return;
    }
    my $len = length($data);
    send_raw($fh, "\$$len\r\n$data\r\n");
}

sub send_null {
    my ($fh) = @_;
    send_raw($fh, "\$-1\r\n");
}

sub send_raw {
    my ($fh, $data) = @_;
    print $fh $data;
}
