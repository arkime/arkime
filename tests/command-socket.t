#!/usr/bin/env perl
#
# SPDX-License-Identifier: Apache-2.0
#
# Tests for the command socket add-file/add-dir async notification feature.
# Starts capture with --command-socket --command-wait --flush, connects via
# Unix domain socket, and validates:
# - add-file without --notify (backward compatible, no async notification)
# - add-file --notify (async file-done/file-error notifications)
# - add-dir --notify (file-done for each file in directory)
# - file-status tracking after completion
# - error handling for nonexistent files

use lib ".";
use strict;
use Test::More tests => 22;
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
    open(STDOUT, '>', "/tmp/capture-test1-$$.log");
    open(STDERR, '>&', STDOUT);
    exec($capture,
        "-c", $config,
        "-n", "test",
        "--command-socket", $sockpath,
        "--command-wait",
        "--flush",
        "--dryrun",
        "--debug",
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
    skip "capture did not start, skipping socket tests", 16 unless $ready;

    my $sock = IO::Socket::UNIX->new(
        Peer => $sockpath,
        Type => SOCK_STREAM,
    );
    ok(defined $sock, "connected to command socket");

    skip "could not connect to socket", 15 unless defined $sock;

    $sock->autoflush(1);
    my $sel = IO::Select->new($sock);

    # Verify capture is ready -- send file-status and wait for response
    print $sock "file-status\n";
    my $init = read_line($sock, $sel, 10);
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

    # Read the async completion notification
    my $notify = read_line($sock, $sel, 30);
    like($notify, qr/^file-done filename=.*dns-udp\.pcap/, "file-done notification with filename");

    # --- Test 3: add-dir WITH --notify (file-done for each file) ---
    print $sock "add-dir --notify $pcapdir\n";
    my $dir_ack = read_line($sock, $sel, 10);
    is($dir_ack, "Added directory\n", "add-dir --notify returns plain ack");

    # Collect all file-done notifications from the directory
    my @dir_notifications;
    for (my $i = 0; $i < 2; $i++) {
        my $n = read_line($sock, $sel, 30);
        if ($n && $n =~ /^file-done filename=/) {
            push @dir_notifications, $n;
        }
    }
    is(scalar @dir_notifications, 2, "add-dir --notify produces file-done for each file");

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

    # --- Test 6: exit after --notify doesn't hang ---
    # Regression test: schemeActions held a ref to the client, preventing exit
    # from closing the socket. Send add-file --notify, wait for completion,
    # then exit. If the ref leak is present, the socket won't close.
    print $sock "add-file --notify $pcap\n";
    read_line($sock, $sel, 10);    # ack
    read_line($sock, $sel, 30);    # file-done
    print $sock "exit\n";
    # Wait up to 5 seconds for the socket to close (become readable with EOF).
    # If the ref leak is present, the socket stays open and can_read times out.
    my $closed = 0;
    if ($sel->can_read(5)) {
        my $n = $sock->sysread(my $buf, 1);
        $closed = 1 if (!defined $n || $n == 0);
    }
    ok($closed, "exit closes connection after --notify file completes");
    $sock->close();

    # Reconnect to verify capture is still healthy
    my $sock2 = IO::Socket::UNIX->new(
        Peer => $sockpath,
        Type => SOCK_STREAM,
    );
    ok(defined $sock2, "reconnected after exit");

    skip "could not reconnect", 1 unless defined $sock2;

    $sock2->autoflush(1);
    my $sel2 = IO::Select->new($sock2);
    print $sock2 "file-status\n";
    my $status4 = read_line($sock2, $sel2, 5);
    is($status4, "file-status done\n", "capture still responsive after exit");

    # Shutdown
    print $sock2 "shutdown\n";
    $sock2->close();
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

# --- Test 7: --notify + -op race regression (no --flush) ---
# When capture runs WITHOUT --flush, packet processing for a just-loaded file
# may still be in flight when reader-scheme.c frees the schemeActions slot in
# the --notify completion path. Packet threads then run arkime_field_ops_run
# against freed memory and crash inside arkime_field_string_add.
#
# Repro: launch capture WITHOUT --flush, queue add-file --notify -op repeatedly
# against a multi-packet pcap, then verify the process is still alive.
my $sockpath2 = "$tmpdir/arkime-test2.sock";
my $racepcap  = "$cwd/pcap/modbus.pcap";

SKIP: {
    skip "modbus.pcap missing, skipping race test", 2 unless -f $racepcap;

    # Create many symlinks under unique names so each add-file uses a distinct
    # URI (forcing fresh readerPos rotation and bypassing page-cache effects).
    my $racedir = "$tmpdir/racepcaps";
    mkdir($racedir);
    my $iterations = 40;
    for (my $i = 0; $i < $iterations; $i++) {
        symlink($racepcap, "$racedir/race-$i.pcap");
    }

    my $logfile = "/tmp/capture-racetest-$$.log";
    # diag("capture stdout/stderr -> $logfile");

    my $pid2 = fork();
    die "fork failed: $!" unless defined $pid2;
    if ($pid2 == 0) {
        # Redirect stdout/stderr to a file so we can inspect last output on crash
        open(STDOUT, '>', $logfile) or die "open $logfile: $!";
        open(STDERR, '>&', STDOUT) or die "dup STDERR: $!";
        # Note: no --flush; this is what exposes the race
        exec($capture,
            "-c", $config,
            "-n", "test",
            "--command-socket", $sockpath2,
            "--command-wait",
            "--dryrun",
            "--debug",
        );
        die "exec failed: $!";
    }

    my $ready2 = 0;
    for (my $i = 0; $i < 100; $i++) {
        if (-S $sockpath2) { $ready2 = 1; last; }
        select(undef, undef, undef, 0.1);
    }

    skip "race-test capture did not start", 2 unless $ready2;

    my $rsock = IO::Socket::UNIX->new(Peer => $sockpath2, Type => SOCK_STREAM);
    skip "could not connect to race-test socket", 2 unless defined $rsock;
    $rsock->autoflush(1);
    my $rsel = IO::Select->new($rsock);

    # Queue many distinct add-file --notify -op requests back-to-back to
    # maximize the chance that packets from file N are still in flight in
    # packet threads when reader-scheme frees actions[N].
    for (my $i = 0; $i < $iterations; $i++) {
        print $rsock "add-file --notify -op tags=racetest:$i $racedir/race-$i.pcap\n";
    }

    # Drain responses for up to ~60s and count file-done notifications.
    my $done = 0;
    my $deadline = time() + 60;
    my $last_progress = time();
    my $last_done = 0;
    while (time() < $deadline && $done < $iterations) {
        my $line = read_line($rsock, $rsel, $deadline - time());
        last unless defined $line && length($line);
        for my $l (split /\n/, $line) {
            $done++ if ($l =~ /^file-done /);
        }
        if ($done > $last_done) {
            $last_done = $done;
            $last_progress = time();
        }
        # If 8s pass with no new file-done, grab a stack sample for diagnosis.
        if (time() - $last_progress > 8) {
            my $sample_file = "/tmp/capture-stall-$pid2.txt";
            system("sample $pid2 3 -file $sample_file >/dev/null 2>&1 &");
            diag("Stall detected after $done/$iterations file-done; sampling $pid2 -> $sample_file");
            last;
        }
    }

    # The key assertion: process is still alive (didn't crash from UAF).
    # Use waitpid+WNOHANG instead of kill(0) — kill(0) returns true for
    # zombies, so a crashed-but-not-reaped capture would falsely pass.
    my $reaped = waitpid($pid2, WNOHANG);
    my $alive  = ($reaped == 0);
    my $why    = "";
    if (!$alive) {
        my $status = $?;
        my $sig    = $status & 127;
        my $exit   = $status >> 8;
        $why = $sig ? " (died with signal $sig)" : " (exited with code $exit)";
        # Dump last lines of capture log into prove output for diagnosis
        if (open(my $lh, '<', $logfile)) {
            my @lines = <$lh>;
            close $lh;
            my $tail = scalar(@lines) > 50 ? join('', @lines[-50..-1]) : join('', @lines);
            diag("--- last 50 lines of $logfile ---\n$tail--- end ---");
        }
    }
    ok($alive, "capture still alive after $iterations concurrent --notify + -op requests$why");

    is($done, $iterations, "received file-done for all $iterations requests");

    # Cleanup
    if ($alive) {
        print $rsock "shutdown\n";
    }
    $rsock->close();

    my $exited2 = 0;
    for (my $i = 0; $i < 100; $i++) {
        my $ret = waitpid($pid2, WNOHANG);
        if ($ret == $pid2) { $exited2 = 1; last; }
        select(undef, undef, undef, 0.1);
    }
    if (!$exited2) {
        kill('KILL', $pid2);
        waitpid($pid2, 0);
    }
    unlink($sockpath2) if (-e $sockpath2);
}
