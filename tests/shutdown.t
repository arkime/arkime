#!/usr/bin/env perl
#
# SPDX-License-Identifier: Apache-2.0
#
# Shutdown tests for capture. Each case starts capture, interrupts it, and checks that
# it stops promptly and says what it skipped:
# - SIGINT mid file with the libpcap-file and scheme readers while a pcap trickles in on stdin
# - shutdown over the command socket with files still queued, every queued file is notified
# - SIGINT with the tzsp reader on a quiet link, the receive loop has to notice quitting

use lib ".";
use strict;
use warnings;
use Test::More tests => 20;
use IO::Socket::UNIX;
use IO::Select;
use POSIX ":sys_wait_h";
use Cwd;
use File::Temp qw(tempdir);

my $cwd = getcwd();
my $tmpdir = tempdir(CLEANUP => 1);
my $capture = "../capture/capture";
my $config = "config.test.ini";
my $bigpcap = "$cwd/pcap/arkime_synthetic.pcap";
my $pcapA = "$cwd/pcap/dns-udp.pcap";
my $pcapB = "$cwd/pcap/dns-mx.pcap";

ok(-x $capture, "capture binary exists and is executable");
ok(-f $bigpcap && -f $pcapA && -f $pcapB, "test pcap files exist");

# Poll for a child to exit, 1 if it did within timeout seconds
sub wait_exit {
    my ($pid, $timeout) = @_;
    my $deadline = time() + $timeout;
    while (time() < $deadline) {
        return 1 if (waitpid($pid, WNOHANG) == $pid);
        select(undef, undef, undef, 0.1);
    }
    return 0;
}

# A child that didn't stop on its own must not outlive the test
sub reap {
    my ($pid) = @_;
    kill('KILL', $pid);
    waitpid($pid, 0);
}

sub slurp {
    my ($file) = @_;
    local $/;
    open(my $fh, '<', $file) or return "";
    my $data = <$fh>;
    close($fh);
    return $data;
}

# Read everything from a socket until the peer closes it or timeout seconds pass.
# Returns (data, closed)
sub read_until_closed {
    my ($sock, $timeout) = @_;
    my $sel = IO::Select->new($sock);
    my $buf = "";
    my $deadline = time() + $timeout;
    while (time() < $deadline) {
        last unless $sel->can_read($deadline - time());
        my $chunk = "";
        my $n = $sock->sysread($chunk, 4096);
        return ($buf, 1) if (!defined $n || $n == 0);
        $buf .= $chunk;
    }
    return ($buf, 0);
}

################################################################################
# Mid file interrupt, libpcap-file and scheme readers.
# The pcap is trickled in over stdin so the interrupt lands while the file is still being
# read, and the trickle keeps going after the interrupt so the reader's blocking read
# returns and it sees config.quitting.
################################################################################
for my $mode (["--libpcap", "-o", "offlineDispatchAfter=10"], ["--scheme"]) {
    my $name = $mode->[0];
    my $log = "$tmpdir/mid-file$name.log";

    pipe(my $rd, my $wr) or die "pipe: $!";
    my $pid = fork();
    die "fork failed: $!" unless defined $pid;
    if ($pid == 0) {
        close($wr);
        open(STDIN, '<&', $rd) or die "dup stdin: $!";
        open(STDOUT, '>', $log) or die "open $log: $!";
        open(STDERR, '>&', STDOUT);
        # -r - needs --copy, with --dryrun nothing is written
        exec($capture, "-c", $config, "-n", "test", "--copy", "--dryrun", "--debug", @$mode, "-r", "-");
        die "exec failed: $!";
    }
    close($rd);
    $wr->autoflush(1);

    open(my $in, '<', $bigpcap) or die "open $bigpcap: $!";
    binmode($in);
    local $SIG{PIPE} = 'IGNORE';
    # Startup can take a few seconds under the sanitizers, so wait for the reader to log that
    # it opened stdin before timing the interrupt, otherwise the quit lands during init and
    # there is no file to skip. 1k every 50ms gives about 17 seconds of runway.
    my $started = 0;
    my $startedAt = 0;
    my $sentInt = 0;
    my $exited = 0;
    my $buf;
    while (read($in, $buf, 1024)) {
        last unless defined syswrite($wr, $buf); # capture went away
        select(undef, undef, undef, 0.05);
        if (!$started && slurp($log) =~ /Processing -/) {
            $started = 1;
            $startedAt = time();
        }
        if ($started && !$sentInt && time() - $startedAt >= 1) {
            kill('INT', $pid);
            $sentInt = 1;
        }
        if ($sentInt && waitpid($pid, WNOHANG) == $pid) {
            $exited = 1;
            last;
        }
    }
    close($in);
    diag("$name: reader never logged Processing before the pcap ran out") unless $started;
    kill('INT', $pid) unless $sentInt;
    $exited ||= wait_exit($pid, 10);
    close($wr);

    ok($exited, "$name: capture exits within 10s of SIGINT mid file");
    reap($pid) unless $exited;

    my $out = slurp($log);
    like($out, qr/Quitting, skipping rest of -/, "$name: logged the skipped file");
    unlike($out, qr/recorded as processed/, "$name: a copied file is not reported as recorded");
}

################################################################################
# Shutdown with files still queued on the command socket.
# Every queued file must get a notification, not just EOF, and capture must exit.
################################################################################
{
    my $sockpath = "$tmpdir/shutdown.sock";
    my $log = "$tmpdir/command-socket.log";

    my $pid = fork();
    die "fork failed: $!" unless defined $pid;
    if ($pid == 0) {
        open(STDOUT, '>', $log) or die "open $log: $!";
        open(STDERR, '>&', STDOUT);
        exec($capture, "-c", $config, "-n", "test", "--command-socket", $sockpath, "--command-wait", "--flush", "--dryrun", "--debug");
        die "exec failed: $!";
    }

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
        skip "capture did not start", 6 unless $ready;

        my $sock = IO::Socket::UNIX->new(Peer => $sockpath, Type => SOCK_STREAM);
        ok(defined $sock, "connected to command socket");
        skip "could not connect to socket", 5 unless defined $sock;
        $sock->autoflush(1);

        # One write so all three adds are queued before shutdown is seen. --flush makes the big
        # file take a while, so the two small ones are still queued when quitting starts.
        print $sock "add-file --notify $bigpcap\nadd-file --notify $pcapA\nadd-file --notify $pcapB\nshutdown\n";

        my ($data, $closed) = read_until_closed($sock, 30);
        $sock->close();

        my @acks = ($data =~ /^Added file$/mg);
        is(scalar @acks, 3, "all three add-file commands were acked");

        for my $pcap ($bigpcap, $pcapA, $pcapB) {
            my ($base) = $pcap =~ m{([^/]+)$};
            like($data, qr/^file-(done|error) filename=.*\Q$base\E/m, "$base was notified before exit");
        }
        like($data, qr/^file-error filename=.*dns-mx\.pcap/m, "last queued file got file-error from the quit drain");
        ok($closed, "capture closed the command socket on exit");
    }

    my $exited = wait_exit($pid, 10);
    ok($exited, "capture exits after shutdown with queued files");
    reap($pid) unless $exited;
    unlink($sockpath) if (-e $sockpath);
}

################################################################################
# Quiet tzsp link. Nothing ever arrives, the receive must time out so the loop
# sees quitting and the reader thread reaches its exit hook.
################################################################################
{
    my $log = "$tmpdir/tzsp.log";
    my $port = 40000 + ($$ % 20000);

    my $pid = fork();
    die "fork failed: $!" unless defined $pid;
    if ($pid == 0) {
        open(STDOUT, '>', $log) or die "open $log: $!";
        open(STDERR, '>&', STDOUT);
        exec($capture, "-c", $config, "-n", "test", "--dryrun", "--debug", "-o", "pcapReadMethod=tzsp", "-o", "tzspPort=$port");
        die "exec failed: $!";
    }

    # Give it time to get through init and start the reader thread
    my $started = 0;
    for (my $i = 0; $i < 100; $i++) {
        if (waitpid($pid, WNOHANG) == $pid) {
            last;
        }
        if (slurp($log) =~ /maxDbField/) {
            $started = 1;
            last;
        }
        select(undef, undef, undef, 0.1);
    }
    ok($started, "tzsp capture started");

    kill('INT', $pid);
    my $exited = wait_exit($pid, 10);
    ok($exited, "tzsp capture exits within 10s of SIGINT with no traffic");
    reap($pid) unless $exited;

    my $out = slurp($log);
    unlike($out, qr/FATAL|ERROR - Reader didn't call/, "tzsp capture started without a config error");
}
