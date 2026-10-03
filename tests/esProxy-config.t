use Test::More tests => 12;
use strict;

# esproxy-sensors entries that would leave esProxy auth fail open must stop it
# at startup. Exits before connecting, so this needs no cluster.

my $ini = "/tmp/arkime-esproxy-config-$$.ini";
END { unlink $ini if $ini; }

sub writeIni {
    my ($sensor) = @_;
    open(my $f, '>', $ini) or die "can't write $ini";
    print $f "[default]\nelasticsearch=http://127.0.0.1:9200\nprefix=tests\nesProxyPort=17299\n\n[esproxy-sensors]\n$sensor\n";
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

foreach my $test (
    ["good=pass:x", "pass only"],
    ["good=ip:127.0.0.1", "ip only"],
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
