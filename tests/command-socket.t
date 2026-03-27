#!/usr/bin/env perl
#
# SPDX-License-Identifier: Apache-2.0
#
# Tests for the command socket add-file async notification feature.
# Starts capture with --command-socket --command-wait --flush, connects via
# Unix domain socket, sends add-file, and validates both the immediate ack
# (with correlation ID) and the async file-done notification.

use lib ".";
use strict;
use Test::More tests => 10;
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
    skip "capture did not start, skipping socket tests", 7 unless $ready;

    # Connect to the command socket
    my $sock = IO::Socket::UNIX->new(
        Peer => $sockpath,
        Type => SOCK_STREAM,
    );
    ok(defined $sock, "connected to command socket");

    skip "could not connect to socket", 6 unless defined $sock;

    $sock->autoflush(1);
    my $sel = IO::Select->new($sock);

    # Test 1: Send add-file command
    print $sock "add-file $pcap\n";

    # Read the immediate ack - should contain correlation ID
    my $ack = "";
    if ($sel->can_read(5)) {
        $sock->sysread($ack, 4096);
    }
    like($ack, qr/^Added file id=(\d+)\n/, "immediate ack contains correlation ID");

    # Extract the ID from the ack
    my ($ack_id) = ($ack =~ /id=(\d+)/);
    ok(defined $ack_id && $ack_id > 0, "correlation ID is a positive integer (got: " . ($ack_id // "undef") . ")");

    # Read the async completion notification (may take a few seconds)
    my $notify = "";
    if ($sel->can_read(30)) {
        $sock->sysread($notify, 4096);
    }
    like($notify, qr/^file-done id=\d+/, "received file-done notification");

    # Validate the notification contains the same ID as the ack
    my ($notify_id) = ($notify =~ /id=(\d+)/);
    is($notify_id, $ack_id, "notification ID matches ack ID");

    # Validate the notification contains expected fields
    like($notify, qr/filename=.*dns-udp\.pcap/, "notification contains filename");

    # Test error case: add a non-existent file
    print $sock "add-file /nonexistent/file.pcap\n";

    my $err_ack = "";
    if ($sel->can_read(5)) {
        $sock->sysread($err_ack, 4096);
    }

    # Read all data (ack + async error notification may come together or separately)
    my $err_all = $err_ack;
    if ($sel->can_read(10)) {
        my $more = "";
        $sock->sysread($more, 4096);
        $err_all .= $more;
    }
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
