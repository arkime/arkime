use Test::More tests => 24;
use strict;

# esproxy-sensors entries that would leave esProxy auth fail open must stop it
# at startup. Sensors without host read their pinned hostname from es first.

my $ini = "/tmp/arkime-esproxy-config-$$.ini";
END { unlink $ini if $ini; }

sub writeIni {
    my ($sensor, $es) = @_;
    $es //= "http://127.0.0.1:9200";
    open(my $f, '>', $ini) or die "can't write $ini";
    print $f "[default]\nelasticsearch=$es\nprefix=tests\nesProxyPort=17299\n\n[esproxy-sensors]\n$sensor\n";
    close($f);
}

foreach my $test (
    ["bad=pass:", "empty pass"],
    ["bad=ip:", "empty ip"],
    ["bad=foo:bar", "neither pass nor ip"],
    ["bad=pass:true", "boolean pass"],
    ["bad=pass:x;ip:false", "boolean ip"],
) {
    my ($sensor, $name) = @$test;
    writeIni($sensor);
    my $out = `node ../viewer/esProxy.js -c $ini -n bad 2>&1`;
    is ($? >> 8, 1, "$name exits");
    like ($out, qr/ERROR - esproxy-sensors 'bad' must set a non empty 'pass' and\/or 'ip'/, "$name error");
}

# a blank host would turn the stats hostname check off
foreach my $test (
    ["bad=pass:x;host:", "empty host"],
    ["bad=pass:x;host:true", "boolean host"],
) {
    my ($sensor, $name) = @$test;
    writeIni($sensor);
    my $out = `node ../viewer/esProxy.js -c $ini -n bad 2>&1`;
    is ($? >> 8, 1, "$name exits");
    like ($out, qr/ERROR - esproxy-sensors 'bad' must set a non empty 'host' when set/, "$name error");
}

foreach my $test (
    ["good=pass:x", "pass only"],
    ["good=ip:127.0.0.1", "ip only"],
    ["good=pass:x;host:sensor1.example.com", "pass and host"],
) {
    my ($sensor, $name) = @$test;
    writeIni($sensor);
    my $pid = open(my $out, '-|', "node ../viewer/esProxy.js -c $ini -n good 2>&1") or die "can't run esProxy";
    my $listening = 0;
    eval {
        local $SIG{ALRM} = sub { die "timeout\n" };
        alarm 20;
        while (my $line = <$out>) {
            if ($line =~ /listening on/) { $listening = 1; last; }
        }
        alarm 0;
    };
    kill 'TERM', $pid;
    close($out);
    ok ($listening, "$name starts");
}

# Run esProxy until a line matches or it exits, returns the exit code (undef if
# still running) and everything it printed
sub runUntil {
    my ($re) = @_;
    my $pid = open(my $out, '-|', "node ../viewer/esProxy.js -c $ini -n good 2>&1") or die "can't run esProxy";
    my ($all, $exited) = ('', 0);
    eval {
        local $SIG{ALRM} = sub { die "timeout\n" };
        alarm 20;
        while (my $line = <$out>) {
            $all .= $line;
            last if ($line =~ $re);
        }
        $exited = 1 if (!kill(0, $pid) || eof($out));
        alarm 0;
    };
    kill 'TERM', $pid;
    close($out);
    return ($exited ? $? >> 8 : undef, $all);
}

# Pinning sensors without host needs es at startup, a bad url or a response that
# won't change exits instead of never listening
foreach my $test (
    ["notaurl", qr/invalid elasticsearch url/, "bad elasticsearch url"],
    ["http://127.0.0.1:9200/esproxy-no-such-path", qr/status 400/, "es 4xx response"],
) {
    my ($es, $re, $name) = @$test;
    writeIni("good=pass:x", $es);
    my $out = `node ../viewer/esProxy.js -c $ini -n good 2>&1`;
    is ($? >> 8, 1, "$name exits");
    like ($out, $re, "$name error");
}

# es not reachable yet is retried, not fatal
writeIni("good=pass:x", "http://127.0.0.1:1");
my ($code, $out) = runUntil(qr/retrying/);
like ($out, qr/loading stats hostnames to pin esproxy-sensors, retrying/, "unreachable es retried");
unlike ($out, qr/listening on/, "unreachable es not listening");

# A sensor with host set doesn't need es at startup
writeIni("good=pass:x;host:sensor1.example.com", "http://127.0.0.1:1");
($code, $out) = runUntil(qr/listening on/);
like ($out, qr/listening on/, "sensor with host starts without es");
