# Test python
use lib ".";
use ArkimeTest;
use Test::More tests => 5;
use Test::Differences;
use Data::Dumper;
use JSON;
use strict;

sub runCapture {
    my ($plugin, $pcap) = @_;

    my $cmd = "../capture/capture -c config.test.ini -n test --regressionTests --tests -o disablePython=false -o plugins=$plugin -r $pcap 2>&1 1>/dev/null | ./tests.pl --fix";
    # diag $cmd;

    my $input = `$cmd`;
    # diag $input;

    my $out;

    eval {
        $out = from_json($input, {relaxed => 1});
        1;
    } or do {
        my $e = $@;
        print "JSON Parse Error: $e\n";
        print $input, "\n";
        exit 1;
    };
    return $out;
}

### Session callbacks
my $out = runCapture("pythontest.py", "pcap/aerospike.pcap");

#diag Dumper($out->{sessions3}->[0]->{body});

is($out->{sessions3}->[0]->{body}->{test}->{python}->[0], "my value");
is($out->{sessions3}->[0]->{body}->{http}->{statuscode}->[0], 12345);

### Packet callbacks - a callback returning a non-int (None) must not turn
### into ArkimePacketRC -1, which dropped the packet and did an out of
### bounds packetStats write.
$out = runCapture("pythonpacketcb.py", "pcap/python-packet-cb.pcap");

is(scalar @{$out->{sessions3}}, 1, "one session");
is($out->{sessions3}->[0]->{body}->{network}->{packets}, 4, "all 4 packets processed");
is($out->{sessions3}->[0]->{body}->{source}->{packets}, 4, "all 4 src packets");
