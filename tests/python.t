# Test python
use lib ".";
use ArkimeTest;
use Test::More tests => 2;
use Test::Differences;
use Data::Dumper;
use JSON;
use strict;


my $cmd = '../capture/capture -c config.test.ini -n test --regressionTests --tests -o plugins=pythontest.py -r pcap/aerospike.pcap 2>&1 1>/dev/null | ./tests.pl --fix';
# diag $cmd;

my $input = `$cmd`;
# diag $input;

$input =~ s/^-----.*//ms;

my $out = from_json($input, {relaxed => 1});
#diag Dumper($out->{sessions3}->[0]->{body});

is($out->{sessions3}->[0]->{body}->{test}->{python}->[0], "my value");
is($out->{sessions3}->[0]->{body}->{http}->{statuscode}->[0], 12345);
