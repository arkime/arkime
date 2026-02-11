#!/usr/bin/env perl
#
# SPDX-License-Identifier: Apache-2.0
#
# Mini Redis server for testing. Stores everything in memory.
# Usage: perl mini-redis.pl <port>

use strict;
use warnings;
use IO::Socket::INET;
use IO::Select;

my $port = $ARGV[0] || 6379;

# Storage: $db{$dbnum}{$key} = { value => ..., expire => ... }
my %db;
my %conn_db;  # fileno => db number

my $server = IO::Socket::INET->new(
    LocalPort => $port,
    Proto     => 'tcp',
    Listen    => 5,
    ReuseAddr => 1,
) or die "Cannot start server on port $port: $!\n";

print "mini-redis listening on port $port\n";

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
                $sel->add($client);
            }
            next;
        }

        my $cmd = parse_command($fh);
        if (!$cmd) {
            delete $conn_db{fileno($fh)};
            $sel->remove($fh);
            $fh->close();
            next;
        }

        my @args = @$cmd;
        my $name = uc($args[0] // '');

        my $cdb = $conn_db{fileno($fh)} // 0;

        print "REDIS: $name " . join(' ', map { defined $_ ? $_ : '(nil)' } @args[1..$#args]) . " [db=$cdb]\n";

        if ($name eq 'PING') {
            send_simple($fh, 'PONG');
        } elsif ($name eq 'QUIT') {
            send_simple($fh, 'OK');
            $sel->remove($fh);
            $fh->close();
            next;
        } elsif ($name eq 'SELECT') {
            my $dbn = $args[1] // 0;
            $conn_db{fileno($fh)} = int($dbn);
            send_simple($fh, 'OK');
        } elsif ($name eq 'SET') {
            my $key = $args[1];
            my $val = $args[2];
            if (!defined $key || !defined $val) {
                send_error($fh, "ERR wrong number of arguments for 'set' command");
                next;
            }
            my $entry = { value => $val };
            # Parse optional EX/PX/EXAT/PXAT
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
            # Redis clients send COMMAND DOCS on connect; just return empty array
            send_raw($fh, "*0\r\n");
        } elsif ($name eq 'INFO') {
            send_bulk($fh, "# Server\r\nmini_redis:1.0\r\n");
        } elsif ($name eq 'DBSIZE') {
            my $size = scalar keys %{$db{$cdb} // {}};
            send_integer($fh, $size);
        } elsif ($name eq 'FLUSHDB') {
            delete $db{$cdb};
            send_simple($fh, 'OK');
        } elsif ($name eq 'SHUTDOWN') {
            send_simple($fh, 'OK');
            exit(0);
        } else {
            send_error($fh, "ERR unknown command '$name'");
        }
    }
}

# Parse RESP protocol command
sub parse_command {
    my ($fh) = @_;
    my $line = read_line($fh);
    return undef unless defined $line;

    if ($line =~ /^\*(\d+)/) {
        # RESP array
        my $count = $1;
        my @args;
        for (1..$count) {
            my $hdr = read_line($fh);
            return undef unless defined $hdr;
            if ($hdr =~ /^\$(-?\d+)/) {
                my $len = $1;
                if ($len < 0) {
                    push @args, undef;
                } else {
                    my $data;
                    my $read = 0;
                    while ($read < $len) {
                        my $chunk;
                        my $n = $fh->read($chunk, $len - $read);
                        return undef unless $n;
                        $data .= $chunk;
                        $read += $n;
                    }
                    # consume trailing \r\n
                    read_line($fh);
                    push @args, $data;
                }
            } else {
                # Inline value
                push @args, $hdr;
            }
        }
        return \@args;
    } elsif ($line =~ /\S/) {
        # Inline command
        my @parts = split(/\s+/, $line);
        return \@parts if @parts;
    }
    return undef;
}

sub read_line {
    my ($fh) = @_;
    my $line = '';
    while (1) {
        my $ch;
        my $n = $fh->read($ch, 1);
        return undef if !$n;
        $line .= $ch;
        if ($line =~ /\r?\n$/) {
            $line =~ s/\r?\n$//;
            return $line;
        }
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
