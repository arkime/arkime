#!/usr/bin/env perl
#
# SPDX-License-Identifier: Apache-2.0
#
# Tests for the command socket add-file async notification feature.
# Starts capture with --command-socket --command-wait --flush, connects via
# Unix domain socket, and validates:
# - add-file without --notify (backward compatible, no async notification)
# - add-file --notify (correlation ID in ack + async file-done/file-error)
# - file-status command (polling for outstanding items)

use lib ".";
use strict;
use Test::More tests => 12;
use IO::Socket::UNIX;
use IO::Select;
use POSIX ":sys_wait_h";
use Cwd;
use File::Temp qw(tempdir);

my $cwd = getcwd();
my $tmpdir = tempdir(CLEANUP => 1);
my $sockpath = "$tmpdir/arkime-test.sock";
my $capture = "../capture/capture";
my $config = "config.test.ini";
my $pcap = "$cwd/pcap/dns-udp.pcap";

# Helper: read a full line from socket with timeout
sub read_line {
    my ($sock, $sel, $timeout) = @_;
    $timeout //= 10;
    my $buf = "";
    my $deadline = time() + $timeout;
    while (time() < $deadline) {
        if ($sel->can_read($deadline - time())) {
            my $chunk = "";
            my $n = $sock->sysread($chunk, 4096);
            return undef if (!defined $n || $n == 0);
            $buf .= $chunk;
            # Return once we have at least one complete line
            return $buf if ($buf =~ /\n/);
        }
    }
    return $buf;
}

# Helper: drain all available data from socket with timeout
sub read_all {
    my ($sock, $sel, $timeout) = @_;
    $timeout //= 10;
    my $buf = "";
    my $deadline = time() + $timeout;
    while (time() < $deadline) {
        last unless $sel->can_read($deadline - time());
        my $chunk = "";
        my $n = $sock->sysread($chunk, 4096);
        last if (!defined $n || $n == 0);
        $buf .= $chunk;
    }
    return $buf;
}

# Verify prerequisites
ok(-x $capture, "capture binary exists and is executable");
ok(-f $pcap, "test pcap file exists");

# Start capture in command socket mode as a background process
my $pid = fork();
die "fork failed: $!" unless defined $pid;

if ($pid == 0) {
    # Child: exec capture
    exec($capture,
        "-c", $config,
        "-n", "test",
        "--command-socket", $sockpath,
        "--command-wait",
        "--flush",
        "--dryrun",
    );
    die "exec failed: $!";
}

# Parent: wait for the socket file to appear (up to 10 seconds)
my $ready = 0;
for (my $i = 0; $i < 100; $i++) {
    if (-S $sockpath) {
        $ready = 1;
        last;
    }
    select(undef, undef, undef, 0.1);  # sleep 100ms
}
ok($ready, "command socket appeared at $sockpath");

SKIP: {
    skip "capture did not start, skipping socket tests", 10 unless $ready;

    # Connect to the command socket
    my $sock = IO::Socket::UNIX->new(
        Peer => $sockpath,
        Type => SOCK_STREAM,
    );
    ok(defined $sock, "connected to command socket");

    skip "could not connect to socket", 9 unless defined $sock;

    $sock->autoflush(1);
    my $sel = IO::Select->new($sock);

    # Verify capture is ready by sending a version command
    print $sock "version\n";
    my $ver = read_line($sock, $sel, 5);
    ok(defined $ver && length($ver) > 0, "capture is responsive");

    # --- Test backward compatibility: add-file WITHOUT --notify ---
    print $sock "add-file $pcap\n";
    my $compat_ack = read_line($sock, $sel, 10);
    is($compat_ack, "Added file\n", "add-file without --notify returns plain ack");

    # Wait for processing then verify no async notification arrives
    ok(!$sel->can_read(3), "no async notification without --notify");

    # --- Test add-file WITH --notify ---
    print $sock "add-file --notify $pcap\n";
    my $ack = read_line($sock, $sel, 10);
    like($ack, qr/^Added file id=(\d+)\n/, "add-file --notify ack contains correlation ID");

    my ($ack_id) = ($ack =~ /id=(\d+)/);

    # Read the async completion notification
    my $notify = read_line($sock, $sel, 30);
    like($notify, qr/^file-done id=\Q$ack_id\E\b/, "file-done notification has matching ID");
    like($notify, qr/filename=.*dns-udp\.pcap/, "notification contains filename");

    # --- Test file-status command (should be empty after completion) ---
    print $sock "file-status\n";
    my $status = read_line($sock, $sel, 5);
    is($status, "file-status done\n", "file-status shows no outstanding items after completion");

    # --- Test error case: add-file --notify with non-existent file ---
    print $sock "add-file --notify /nonexistent/file.pcap\n";
    # Read ack + async error (may come in one or two reads)
    my $err_all = read_all($sock, $sel, 10);
    like($err_all, qr/file-error id=\d+/, "received file-error for nonexistent file");

    # Shutdown capture
    print $sock "shutdown\n";
    $sock->close();
}

# Wait for capture to exit (up to 10 seconds)
my $exited = 0;
for (my $i = 0; $i < 100; $i++) {
    my $ret = waitpid($pid, WNOHANG);
    if ($ret == $pid) {
        $exited = 1;
        last;
    }
    select(undef, undef, undef, 0.1);
}

if (!$exited) {
    kill('TERM', $pid);
    waitpid($pid, 0);
}

# Cleanup socket file if still present
unlink($sockpath) if (-e $sockpath);
