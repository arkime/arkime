#!/usr/bin/env perl
#
# SPDX-License-Identifier: Apache-2.0
#
# Tests for the command socket add-file/add-dir async notification feature.
# Starts capture with --command-socket --command-wait --flush, connects via
# Unix domain socket, and validates:
# - add-file without --notify (backward compatible, no async notification)
# - add-file --notify (async file-done/file-error with per-file IDs)
# - add-dir --notify (each file gets its own unique ID)
# - file-status tracking after completion
# - error handling for nonexistent files

use lib ".";
use strict;
use Test::More tests => 18;
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

# Create a temp directory with symlinks to two pcap files for add-dir tests
my $pcapdir = "$tmpdir/pcaps";
mkdir($pcapdir);
symlink("$cwd/pcap/dns-udp.pcap", "$pcapdir/dns-udp.pcap");
symlink("$cwd/pcap/dns-mx.pcap", "$pcapdir/dns-mx.pcap");

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
ok(-d $pcapdir, "test pcap directory exists");

# Start capture in command socket mode as a background process
my $pid = fork();
die "fork failed: $!" unless defined $pid;

if ($pid == 0) {
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

# Wait for the socket file to appear (up to 10 seconds)
my $ready = 0;
for (my $i = 0; $i < 100; $i++) {
    if (-S $sockpath) {
        $ready = 1;
        last;
    }
    select(undef, undef, undef, 0.1);
}
ok($ready, "command socket appeared");

SKIP: {
    skip "capture did not start, skipping socket tests", 14 unless $ready;

    my $sock = IO::Socket::UNIX->new(
        Peer => $sockpath,
        Type => SOCK_STREAM,
    );
    ok(defined $sock, "connected to command socket");

    skip "could not connect to socket", 13 unless defined $sock;

    $sock->autoflush(1);
    my $sel = IO::Select->new($sock);

    # Verify capture is ready -- send file-status and wait for response
    print $sock "file-status\n";
    my $init = read_all($sock, $sel, 2);
    ok(defined $init && $init =~ /file-status done/, "capture is responsive");

    # --- Test 1: add-file WITHOUT --notify (backward compat) ---
    print $sock "add-file $pcap\n";
    my $compat_ack = read_line($sock, $sel, 10);
    is($compat_ack, "Added file\n", "add-file without --notify returns plain ack");

    # Wait for processing, verify no async notification
    ok(!$sel->can_read(3), "no async notification without --notify");

    # Verify file-status is empty after completion
    print $sock "file-status\n";
    my $status1 = read_line($sock, $sel, 5);
    is($status1, "file-status done\n", "file-status empty after non-notify file completes");

    # --- Test 2: add-file WITH --notify ---
    print $sock "add-file --notify $pcap\n";
    my $ack = read_line($sock, $sel, 10);
    is($ack, "Added file\n", "add-file --notify returns plain ack");

    # Read the async completion notification (contains per-file ID)
    my $notify = read_line($sock, $sel, 30);
    like($notify, qr/^file-done id=\d+ filename=.*dns-udp\.pcap/, "file-done notification with per-file ID and filename");

    # --- Test 3: add-dir WITH --notify (each file gets unique ID) ---
    print $sock "add-dir --notify $pcapdir\n";
    my $dir_ack = read_line($sock, $sel, 10);
    is($dir_ack, "Added directory\n", "add-dir --notify returns plain ack");

    # Collect all file-done notifications from the directory
    my @dir_notifications;
    my %dir_ids;
    for (my $i = 0; $i < 2; $i++) {
        my $n = read_line($sock, $sel, 30);
        if ($n && $n =~ /^file-done id=(\d+)/) {
            push @dir_notifications, $n;
            $dir_ids{$1} = 1;
        }
    }
    is(scalar @dir_notifications, 2, "add-dir --notify produces file-done for each file");
    is(scalar keys %dir_ids, 2, "each file in directory has a unique notification ID");

    # Drain any remaining notifications from the directory
    read_all($sock, $sel, 2);

    # --- Test 4: add-dir WITHOUT --notify (no notifications, status clears) ---
    print $sock "add-dir $pcapdir\n";
    my $dir_ack2 = read_line($sock, $sel, 10);
    is($dir_ack2, "Added directory\n", "add-dir without --notify returns plain ack");

    # Wait for processing, verify no async notification
    ok(!$sel->can_read(3), "no async notification for add-dir without --notify");

    # Verify file-status is empty after completion
    print $sock "file-status\n";
    my $status3 = read_line($sock, $sel, 5);
    is($status3, "file-status done\n", "file-status empty after non-notify directory completes");

    # --- Test 5: error case ---
    print $sock "add-file --notify /nonexistent/file.pcap\n";
    my $err_all = read_all($sock, $sel, 10);
    like($err_all, qr/file-error/, "received file-error for nonexistent file");

    # Shutdown
    print $sock "shutdown\n";
    $sock->close();
}

# Wait for capture to exit
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

unlink($sockpath) if (-e $sockpath);
