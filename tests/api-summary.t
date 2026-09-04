use Test::More tests => 122;
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
my $expectedDownloadBytes = 24 + $summary->{bytes} + 16 * $summary->{packets};
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
        ok(exists $item->{value}, "data item has value field (metric basis)");
        is($item->{value}, $item->{sessions}, "default metric value equals session count");
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

################################################################################
# widgets[] request shape (the modular dashboard's native path)

# Helper: POST a body to both single and multi viewers, compare, and return the
# raw chunk array (stats chunk first unless noStats, one chunk per widget in
# request order, then the {} terminator)
sub postSummary {
    my ($url, $body) = @_;
    my $postData = to_json($body);
    my $json = viewerPost($url, $postData);
    my $mjson = multiPost($url, $postData);
    eq_or_diff($mjson, $json, "single doesn't match multi for $url widgets body", { context => 3 });
    return $json;
}

# Helper: find a widget chunk by its id
sub getChunk {
    my ($chunks, $id) = @_;
    foreach my $chunk (@{$chunks}) {
        return $chunk if exists $chunk->{id} && $chunk->{id} eq $id;
    }
    return undef;
}

# Basic widgets request - per-widget id/length are honored and echoed
my $chunks = postSummary("/api/sessions/summary?date=-1", {
    widgets => [
        { id => 'wProto', field => 'protocols', length => 5 },
        { id => 'wSrc', field => 'ip.src' }
    ]
});
is(scalar(@{$chunks}), 4, "widgets response has stats + 2 widget chunks + terminator");
ok(exists $chunks->[0]->{sessions} && !exists $chunks->[0]->{field}, "first chunk is the stats chunk");
is(scalar(keys %{$chunks->[-1]}), 0, "response ends with the empty terminator chunk");
my $protoChunk = getChunk($chunks, 'wProto');
ok(defined $protoChunk, "protocols widget chunk echoes its id");
is($protoChunk->{field}, 'protocols', "protocols widget chunk echoes its field");
is($protoChunk->{length}, 5, "protocols widget chunk echoes its length");
cmp_ok(scalar(@{$protoChunk->{data}}), '<=', 5, "protocols widget data limited to 5 items");
cmp_ok(scalar(@{$protoChunk->{data}}), '>', 0, "protocols widget data is not empty");
my $srcChunk = getChunk($chunks, 'wSrc');
ok(defined $srcChunk && ref($srcChunk->{data}) eq 'ARRAY', "ip.src widget chunk has data");

# Empty widgets array - stats-only response
$chunks = postSummary("/api/sessions/summary?date=-1", { widgets => [] });
is(scalar(@{$chunks}), 2, "empty widgets array yields stats + terminator only");
ok(exists $chunks->[0]->{sessions}, "empty widgets array still returns stats");

# noStats - skip the stats chunk (incremental single-widget fetches)
$chunks = postSummary("/api/sessions/summary?date=-1", {
    noStats => \1,
    widgets => [ { id => 'w0', field => 'protocols' } ]
});
is(scalar(@{$chunks}), 2, "noStats response has widget chunk + terminator only");
is($chunks->[0]->{id}, 'w0', "noStats response starts with the widget chunk");
ok(!exists $chunks->[0]->{firstPacket}, "noStats response has no stats chunk");

# Per-widget local expression - ANDed with the global search, other widgets and
# the stats chunk are unaffected
$chunks = postSummary("/api/sessions/summary?date=-1", {
    widgets => [
        { id => 'wAll', field => 'protocols' },
        { id => 'wLocal', field => 'protocols', expression => 'file=*/bigendian.pcap' }
    ]
});
my $allChunk = getChunk($chunks, 'wAll');
my $localChunk = getChunk($chunks, 'wLocal');
is($localChunk->{expression}, 'file=*/bigendian.pcap', "local expression is echoed on the widget chunk");
eq_or_diff([map { { item => $_->{item}, sessions => $_->{sessions} } } @{$localChunk->{data}}],
    [{ item => 'icmp', sessions => 1 }], "local expression filters the widget to bigendian's icmp session");
cmp_ok($chunks->[0]->{sessions}, '>', 1, "stats chunk is unaffected by a widget-local expression");
cmp_ok(scalar(@{$allChunk->{data}}), '>', scalar(@{$localChunk->{data}}), "unfiltered widget sees more protocols than the filtered one");

# Multi-metric widget - metrics[] sums per bucket, sortMetric orders the Top-N
$chunks = postSummary("/api/sessions/summary?date=-1", {
    widgets => [ { id => 'wMetrics', field => 'ip.src', length => 5, metrics => ['bytes', 'packets'], sortMetric => 'bytes' } ]
});
my $mChunk = getChunk($chunks, 'wMetrics');
ok(defined $mChunk && @{$mChunk->{data}} > 0, "multi-metric widget has data");
my ($valuesMatch, $sessionsMatch, $packetsMatch, $sorted, $prev) = (1, 1, 1, 1, undef);
foreach my $item (@{$mChunk->{data}}) {
    $valuesMatch = 0 if $item->{value} != $item->{metricValues}->{bytes} || $item->{value} != $item->{bytes};
    $sessionsMatch = 0 if $item->{metricValues}->{sessions} != $item->{sessions};
    $packetsMatch = 0 if $item->{metricValues}->{packets} != $item->{packets};
    $sorted = 0 if defined $prev && $item->{value} > $prev;
    $prev = $item->{value};
}
ok($valuesMatch, "value is the bytes sort metric and matches the bytes agg");
ok($sessionsMatch, "metricValues.sessions matches the session count");
ok($packetsMatch, "metricValues.packets matches the packets agg");
ok($sorted, "multi-metric data is ranked by the sort metric descending");

# Bottom-N by metric - order=asc ranks ascending by the sort metric
$chunks = postSummary("/api/sessions/summary?date=-1", {
    widgets => [ { id => 'wBottom', field => 'ip.src', length => 5, order => 'asc', metrics => ['bytes'], sortMetric => 'bytes' } ]
});
my $bChunk = getChunk($chunks, 'wBottom');
ok(defined $bChunk && @{$bChunk->{data}} > 0, "bottom-N metric widget has data");
($sorted, $prev) = (1, undef);
foreach my $item (@{$bChunk->{data}}) {
    $sorted = 0 if defined $prev && $item->{value} < $prev;
    $prev = $item->{value};
}
ok($sorted, "bottom-N metric data is ranked ascending");

# Unknown field - per-widget error chunk, later widgets still return data
$chunks = postSummary("/api/sessions/summary?date=-1", {
    widgets => [
        { id => 'wBad', field => 'no.such.field' },
        { id => 'wGood', field => 'protocols' }
    ]
});
my $badChunk = getChunk($chunks, 'wBad');
my $goodChunk = getChunk($chunks, 'wGood');
ok(defined $badChunk && exists $badChunk->{error}, "unknown widget field returns a per-widget error chunk");
like($badChunk->{error}, qr/Unknown field/, "unknown field error names the problem");
ok(defined $goodChunk && @{$goodChunk->{data}} > 0, "widgets after an unknown-field widget still return data");
