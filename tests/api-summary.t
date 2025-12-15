use Test::More tests => 85;
use Cwd;
use URI::Escape;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

# my $pwd = "*/";

# Default fields to request in summary (comma-separated string)
my $defaultFields = 'ip,ip.dst:port,protocols,tags,ip.src,ip.dst,port.dst,port.src,host.http,dns.query.host';

# Helper function to get a field by name from the fields array
sub getField {
    my ($summary, $fieldName) = @_;
    foreach my $field (@{$summary->{fields}}) {
        return $field if $field->{field} eq $fieldName;
    }
    return undef;
}

# Helper function to test both single and multi viewer responses
sub getSummary {
    my ($url, $fields) = @_;
    $fields //= $defaultFields;  # Use default fields if not provided

    # Build POST body with fields as comma-separated string
    my $postData = to_json({ fields => $fields });

    my $json = viewerPost($url, $postData);
    my $mjson = multiPost($url, $postData);

    eq_or_diff($mjson, $json, "single doesn't match multi for $url", { context => 3 });

    # Normalize into single JSON
    my $njson = $json->[0];
    $njson->{fields} = [];
    foreach my $field (@{$json}) {
        push @{$njson->{fields}}, $field if exists $field->{field};
    }

    return $njson;
}

# Test validation - missing fields parameter
my $invalidJson = viewerPost("/api/sessions/summary?date=-1", to_json({}));
ok(exists $invalidJson->{error}, "missing fields parameter returns error");
like($invalidJson->{error}, qr/fields/i, "error message mentions fields");

# Test validation - invalid fields type (array instead of string)
$invalidJson = viewerPost("/api/sessions/summary?date=-1", to_json({ fields => ['ip', 'protocols'] }));
ok(exists $invalidJson->{error}, "invalid fields type returns error");

# Test validation - empty fields string
$invalidJson = viewerPost("/api/sessions/summary?date=-1", to_json({ fields => '' }));
ok(exists $invalidJson->{error}, "empty fields string returns error");

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

# Test fields array exists and has correct structure
ok(exists $summary->{fields}, "fields array exists");
ok(ref($summary->{fields}) eq 'ARRAY', "fields is an array");
cmp_ok(scalar(@{$summary->{fields}}), '>', 0, "fields array is not empty");

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

# Test field structure
if (@{$summary->{fields}} > 0) {
    my $field = $summary->{fields}->[0];
    ok(exists $field->{field}, "field has field property");
    ok(exists $field->{data}, "field has data property");
    ok(exists $field->{viewMode}, "field has viewMode property");
    ok(exists $field->{metricType}, "field has metricType property");
    ok(ref($field->{data}) eq 'ARRAY', "field data is an array");

    # Test data item structure
    if (@{$field->{data}} > 0) {
        my $item = $field->{data}->[0];
        ok(exists $item->{item}, "data item has item field");
        ok(exists $item->{sessions}, "data item has sessions field");
        ok(exists $item->{bytes}, "data item has bytes field");
        ok(exists $item->{packets}, "data item has packets field");
        cmp_ok($item->{sessions}, '>', 0, "data item sessions count is positive");
    }
}

# Test that expected fields exist in the fields array
my @expectedFields = ('ip', 'ip.dst:port', 'protocols', 'tags', 'ip.src', 'ip.dst', 'port.dst', 'port.src', 'host.http', 'dns.query.host');
foreach my $expectedField (@expectedFields) {
    my $field = getField($summary, $expectedField);
    ok(defined $field, "field '$expectedField' exists in fields array");
}

# Test with expression filter - bigendian.pcap only
$summary = getSummary("/api/sessions/summary?date=-1&expression=" . uri_escape("file=*/bigendian.pcap"));
is($summary->{sessions}, 1, "bigendian.pcap has 1 session");
cmp_ok($summary->{packets}, '>', 0, "bigendian.pcap has packets");

# Test with length parameter (topNum)
$summary = getSummary("/api/sessions/summary?date=-1&length=5");
my $protocolsField = getField($summary, 'protocols');
my $ipField = getField($summary, 'ip');
ok(defined $protocolsField && ref($protocolsField->{data}) eq 'ARRAY', "protocols field with length=5");
ok(defined $ipField && ref($ipField->{data}) eq 'ARRAY', "ip field with length=5");
cmp_ok(scalar(@{$protocolsField->{data}}), '<=', 5, "protocols data limited to 5 items") if defined $protocolsField;
cmp_ok(scalar(@{$ipField->{data}}), '<=', 5, "ip data limited to 5 items") if defined $ipField;

$summary = getSummary("/api/sessions/summary?date=-1&length=10");
$protocolsField = getField($summary, 'protocols');
$ipField = getField($summary, 'ip');
cmp_ok(scalar(@{$protocolsField->{data}}), '<=', 10, "protocols data limited to 10 items") if defined $protocolsField;
cmp_ok(scalar(@{$ipField->{data}}), '<=', 10, "ip data limited to 10 items") if defined $ipField;

# Test DNS queries
$summary = getSummary("/api/sessions/summary?date=-1&expression=" . uri_escape("file=*/dns-https.pcap"));
my $dnsField = getField($summary, 'dns.query.host');
ok(defined $dnsField && ref($dnsField->{data}) eq 'ARRAY', "dns.query.host field is an array");
if (defined $dnsField && @{$dnsField->{data}} > 0) {
    my $dns = $dnsField->{data}->[0];
    ok(exists $dns->{item}, "DNS query has item field");
    ok(exists $dns->{sessions}, "DNS query has sessions field");
    ok($dns->{item} ne "", "DNS query item is not empty");
}

# Test HTTP hosts
$summary = getSummary("/api/sessions/summary?date=-1&expression=" . uri_escape("file=*/socks-http-example.pcap"));
my $httpField = getField($summary, 'host.http');
ok(defined $httpField && ref($httpField->{data}) eq 'ARRAY', "host.http field is an array");

# Test TCP/UDP destination ports (port.dst covers both)
$summary = getSummary("/api/sessions/summary?date=-1&expression=" . uri_escape("file=*/*-tcp*.pcap"));
my $portDstField = getField($summary, 'port.dst');
ok(defined $portDstField && ref($portDstField->{data}) eq 'ARRAY', "port.dst field is an array");
if (defined $portDstField && @{$portDstField->{data}} > 0) {
    my $port = $portDstField->{data}->[0];
    ok(exists $port->{item}, "port has item field");
    ok(exists $port->{sessions}, "port has sessions field");
    ok(exists $port->{bytes}, "port has bytes field");
    ok(exists $port->{packets}, "port has packets field");
}

# Test destination IP:Port combinations
$summary = getSummary("/api/sessions/summary?date=-1");
my $ipPortField = getField($summary, 'ip.dst:port');
ok(defined $ipPortField && ref($ipPortField->{data}) eq 'ARRAY', "ip.dst:port field is an array");
if (defined $ipPortField && @{$ipPortField->{data}} > 0) {
    my $ipPort = $ipPortField->{data}->[0];
    ok(exists $ipPort->{item}, "IP:Port has item field");
    ok($ipPort->{item} =~ /[.:]\d+/, "IP:Port item ends with port number");
}

# Test with time range
$summary = getSummary("/api/sessions/summary?date=-1&startTime=1386004308&stopTime=1386004400");
ok(exists $summary->{sessions}, "summary with time range has sessions");
cmp_ok($summary->{sessions}, '>=', 0, "summary with time range sessions >= 0");

# Test tags (if any exist in test data)
$summary = getSummary("/api/sessions/summary?date=-1");
my $tagsField = getField($summary, 'tags');
ok(defined $tagsField && ref($tagsField->{data}) eq 'ARRAY', "tags field is an array");

# Test empty result
$summary = getSummary("/api/sessions/summary?date=-1&expression=" . uri_escape("ip.src==1.2.3.4"));
is($summary->{sessions}, 0, "no sessions for non-existent IP");
is($summary->{bytes}, 0, "no bytes for non-existent IP");
is($summary->{packets}, 0, "no packets for non-existent IP");

# Test length=1 (minimum topNum)
$summary = getSummary("/api/sessions/summary?date=-1&length=1");
$protocolsField = getField($summary, 'protocols');
$ipField = getField($summary, 'ip');
cmp_ok(scalar(@{$protocolsField->{data}}), '<=', 1, "protocols data limited to 1 item") if defined $protocolsField;
cmp_ok(scalar(@{$ipField->{data}}), '<=', 1, "ip data limited to 1 item") if defined $ipField;

# Test length=100 (large topNum)
$summary = getSummary("/api/sessions/summary?date=-1&length=100");
$protocolsField = getField($summary, 'protocols');
$ipField = getField($summary, 'ip');
ok(defined $protocolsField && ref($protocolsField->{data}) eq 'ARRAY', "protocols field with length=100");
ok(defined $ipField && ref($ipField->{data}) eq 'ARRAY', "ip field with length=100");
