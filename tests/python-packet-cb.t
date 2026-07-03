# Test python packet callbacks return values (capture/python.c arkime_python_packet_cb)
# A callback returning a non-int (None) must not turn into ArkimePacketRC -1,
# which dropped the packet and did an out of bounds packetStats write.
use lib ".";
use ArkimeTest;
use Test::More tests => 3;
use Test::Differences;
use Data::Dumper;
use JSON;
use strict;

my $cmd = '../capture/capture -c config.test.ini -n test --regressionTests --tests -o disablePython=false -o plugins=pythonpacketcb.py -r pcap/python-packet-cb.pcap 2>&1 1>/dev/null | ./tests.pl --fix';
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

# diag Dumper($out->{sessions3}->[0]->{body});

is(scalar @{$out->{sessions3}}, 1, "one session");
is($out->{sessions3}->[0]->{body}->{network}->{packets}, 4, "all 4 packets processed");
is($out->{sessions3}->[0]->{body}->{source}->{packets}, 4, "all 4 src packets");
