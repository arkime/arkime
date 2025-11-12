use Test::More tests => 82;
use Cwd;
use URI::Escape;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

# my $pwd = "*/";


# Helper function to test both single and multi viewer responses
sub getSummary {
    my ($url) = @_;

    my $json = viewerGet($url);
    my $mjson = multiGet($url);

    # Delete fields that might differ between single and multi
    delete $json->{recordsTotal};
    delete $mjson->{recordsTotal};

    eq_or_diff($mjson, $json, "single doesn't match multi for $url", { context => 3 });

    return $json;
}

# Basic summary test - all PCAP files
my $summary = getSummary("/api/sessions/summary?date=-1");

# Test basic response structure
ok(exists $summary->{firstPacket}, "firstPacket exists");
ok(exists $summary->{lastPacket}, "lastPacket exists");
ok(exists $summary->{sessions}, "sessions exists");
ok(exists $summary->{bytes}, "bytes exists");
ok(exists $summary->{dataBytes}, "dataBytes exists");
ok(exists $summary->{packets}, "packets exists");
ok(exists $summary->{downloadBytes}, "downloadBytes exists");

# Test aggregation arrays exist
ok(exists $summary->{tags}, "tags exists");
ok(exists $summary->{protocols}, "protocols exists");
ok(exists $summary->{uniqueIp}, "uniqueIp exists");
ok(exists $summary->{uniqueSrcIp}, "uniqueSrcIp exists");
ok(exists $summary->{uniqueDstIp}, "uniqueDstIp exists");
ok(exists $summary->{uniqueDstIpPort}, "uniqueDstIpPort exists");
ok(exists $summary->{uniqueTcpDstPorts}, "uniqueTcpDstPorts exists");
ok(exists $summary->{uniqueUdpDstPorts}, "uniqueUdpDstPorts exists");
ok(exists $summary->{dnsQueryHost}, "dnsQueryHost exists");
ok(exists $summary->{httpHost}, "httpHost exists");

# Test numeric values are reasonable
cmp_ok($summary->{sessions}, '>', 0, "sessions count is positive");
cmp_ok($summary->{bytes}, '>', 0, "bytes count is positive");
cmp_ok($summary->{packets}, '>', 0, "packets count is positive");
cmp_ok($summary->{firstPacket}, '>', 0, "firstPacket timestamp is positive");
cmp_ok($summary->{lastPacket}, '>', 0, "lastPacket timestamp is positive");
cmp_ok($summary->{lastPacket}, '>=', $summary->{firstPacket}, "lastPacket >= firstPacket");

# Test downloadBytes calculation
my $expectedDownloadBytes = 20 + $summary->{bytes} + 16 * $summary->{packets};
is($summary->{downloadBytes}, $expectedDownloadBytes, "downloadBytes calculated correctly");

# Test protocols array structure
ok(ref($summary->{protocols}) eq 'ARRAY', "protocols is an array");
if (@{$summary->{protocols}} > 0) {
    my $proto = $summary->{protocols}->[0];
    ok(exists $proto->{item}, "protocol has item field");
    ok(exists $proto->{sessions}, "protocol has sessions field");
    ok(exists $proto->{bytes}, "protocol has bytes field");
    ok(exists $proto->{packets}, "protocol has packets field");
    cmp_ok($proto->{sessions}, '>', 0, "protocol sessions count is positive");
}

# Test IP addresses array structure
ok(ref($summary->{uniqueIp}) eq 'ARRAY', "uniqueIp is an array");
ok(ref($summary->{uniqueSrcIp}) eq 'ARRAY', "uniqueSrcIp is an array");
ok(ref($summary->{uniqueDstIp}) eq 'ARRAY', "uniqueDstIp is an array");

if (@{$summary->{uniqueIp}} > 0) {
    my $ip = $summary->{uniqueIp}->[0];
    ok(exists $ip->{item}, "IP has item field");
    ok(exists $ip->{sessions}, "IP has sessions field");
    ok(exists $ip->{bytes}, "IP has bytes field");
    ok(exists $ip->{packets}, "IP has packets field");
}

# Test with expression filter - bigendian.pcap only
$summary = getSummary("/api/sessions/summary?date=-1&expression=" . uri_escape("file=*/bigendian.pcap"));
is($summary->{sessions}, 1, "bigendian.pcap has 1 session");
cmp_ok($summary->{packets}, '>', 0, "bigendian.pcap has packets");

# Test with length parameter (topNum)
$summary = getSummary("/api/sessions/summary?date=-1&length=5");
ok(ref($summary->{protocols}) eq 'ARRAY', "protocols array with length=5");
cmp_ok(scalar(@{$summary->{protocols}}), '<=', 5, "protocols limited to 5 items");
cmp_ok(scalar(@{$summary->{uniqueIp}}), '<=', 5, "uniqueIp limited to 5 items");

$summary = getSummary("/api/sessions/summary?date=-1&length=10");
cmp_ok(scalar(@{$summary->{protocols}}), '<=', 10, "protocols limited to 10 items");
cmp_ok(scalar(@{$summary->{uniqueIp}}), '<=', 10, "uniqueIp limited to 10 items");

# Test DNS queries
$summary = getSummary("/api/sessions/summary?date=-1&expression=" . uri_escape("file=*/dns-https.pcap"));
ok(ref($summary->{dnsQueryHost}) eq 'ARRAY', "dnsQueryHost is an array");
if (@{$summary->{dnsQueryHost}} > 0) {
    my $dns = $summary->{dnsQueryHost}->[0];
    ok(exists $dns->{item}, "DNS query has item field");
    ok(exists $dns->{sessions}, "DNS query has sessions field");
    ok($dns->{item} ne "", "DNS query item is not empty");
}

# Test HTTP hosts
$summary = getSummary("/api/sessions/summary?date=-1&expression=" . uri_escape("file=*/socks-http-example.pcap"));
ok(ref($summary->{httpHost}) eq 'ARRAY', "httpHost is an array");

# Test TCP ports
$summary = getSummary("/api/sessions/summary?date=-1&expression=" . uri_escape("file=*/*-tcp*.pcap"));
ok(ref($summary->{uniqueTcpDstPorts}) eq 'ARRAY', "uniqueTcpDstPorts is an array");
if (@{$summary->{uniqueTcpDstPorts}} > 0) {
    my $port = $summary->{uniqueTcpDstPorts}->[0];
    ok(exists $port->{item}, "TCP port has item field");
    ok(exists $port->{sessions}, "TCP port has sessions field");
    ok(exists $port->{bytes}, "TCP port has bytes field");
    ok(exists $port->{packets}, "TCP port has packets field");
}

# Test UDP ports
$summary = getSummary("/api/sessions/summary?date=-1&expression=" . uri_escape("file=*/dns-udp.pcap"));
ok(ref($summary->{uniqueUdpDstPorts}) eq 'ARRAY', "uniqueUdpDstPorts is an array");

# Test destination IP:Port combinations
$summary = getSummary("/api/sessions/summary?date=-1");
ok(ref($summary->{uniqueDstIpPort}) eq 'ARRAY', "uniqueDstIpPort is an array");
if (@{$summary->{uniqueDstIpPort}} > 0) {
    my $ipPort = $summary->{uniqueDstIpPort}->[0];
    ok(exists $ipPort->{item}, "IP:Port has item field");
    ok($ipPort->{item} =~ /_/, "IP:Port item contains underscore separator");
}

# Test with time range
$summary = getSummary("/api/sessions/summary?date=-1&startTime=1386004308&stopTime=1386004400");
ok(exists $summary->{sessions}, "summary with time range has sessions");
cmp_ok($summary->{sessions}, '>=', 0, "summary with time range sessions >= 0");

# Test tags (if any exist in test data)
$summary = getSummary("/api/sessions/summary?date=-1");
ok(ref($summary->{tags}) eq 'ARRAY', "tags is an array");

# Test empty result
$summary = getSummary("/api/sessions/summary?date=-1&expression=" . uri_escape("ip.src==1.2.3.4"));
is($summary->{sessions}, 0, "no sessions for non-existent IP");
is($summary->{bytes}, 0, "no bytes for non-existent IP");
is($summary->{packets}, 0, "no packets for non-existent IP");

# Test length=1 (minimum topNum)
$summary = getSummary("/api/sessions/summary?date=-1&length=1");
cmp_ok(scalar(@{$summary->{protocols}}), '<=', 1, "protocols limited to 1 item");
cmp_ok(scalar(@{$summary->{uniqueIp}}), '<=', 1, "uniqueIp limited to 1 item");

# Test length=100 (large topNum)
$summary = getSummary("/api/sessions/summary?date=-1&length=100");
ok(ref($summary->{protocols}) eq 'ARRAY', "protocols array with length=100");
ok(ref($summary->{uniqueIp}) eq 'ARRAY', "uniqueIp array with length=100");
