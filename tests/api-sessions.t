use Test::More tests => 83;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $pwd = "*/pcap";

sub testMulti {
    my ($json, $mjson, $url) = @_;

    # recordsTotal might be different
    delete $json->{recordsTotal};
    delete $mjson->{recordsTotal};

    for (my $i=0; $i < scalar(@{$mjson->{data}}); $i++) { delete $mjson->{data}->[$i]->{cluster}; }

    eq_or_diff($mjson, $json, "single doesn't match multi for $url", { context => 3 });

    return $json
}

sub get {
    my ($url) = @_;

    my $json = viewerGet($url);
    my $mjson = multiGet($url);

    $json = testMulti($json, $mjson, $url);

    return $json;
}

sub post {
    my ($url, $content) = @_;

    my $json = viewerPost($url, $content);
    my $mjson = multiPost($url, $content);

    $json = testMulti($json, $mjson, $url);

    return $json;
}

sub getBinary {
my ($url) = @_;

#    diag "http://$MolochTest::host:8123$url";
    my $response = $MolochTest::userAgent->get("http://$MolochTest::host:8123$url");
    my $mresponse = $MolochTest::userAgent->get("http://$MolochTest::host:8125$url");

    eq_or_diff($mresponse->content, $response->content, "single doesn't match multi for $url", { context => 3 });

    return $response
}


# bigendian pcap file tests
    my $json = get("/sessions.json?length=1000&date=-1&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    is ($json->{recordsFiltered}, 1, "bigendian recordsFiltered");
    is (scalar @{$json->{data}}, 1);

    my $id = $json->{data}->[0]->{id};
    my $response = getBinary("/test/raw/" . $id . "?type=src");
    is (unpack("H*", $response->content), "08000afb43a800004fa11b290002538d08090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363708004bcb43ca00004fa11b2d0008129108090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f3031323334353637", "Correct bigendian tcpdump data");

    # Start at second element
    my $json = get("/sessions.json?length=1000&start=2&date=-1&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    is ($json->{recordsFiltered}, 1, "bigendian recordsFiltered");
    is (scalar @{$json->{data}}, 0);

    # Force a scroll
    $json = get("/sessions.json?length=20000&date=-1&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    is ($json->{recordsFiltered}, 1);
    is (scalar @{$json->{data}}, 1);

    # Force a scroll and start at second element
    $json = get("/sessions.json?length=20000&start=2&date=-1&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    is ($json->{recordsFiltered}, 1);
    is (scalar @{$json->{data}}, 0);

# Check facets short
    $json = get("/sessions.json?map=true&startTime=1386004308&stopTime=1386004400&facets=1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"dst":{"US": 3}, "src":{"US": 3}, "xffGeo":{}}'), "map short");
    eq_or_diff($json->{graph}->{sessionsHisto}, from_json('[["1386004309000", 1], ["1386004312000", 1], [1386004317000, 1]]'), "sessionsHisto short");
    eq_or_diff($json->{graph}->{"source.packetsHisto"}, from_json('[["1386004309000", 8], ["1386004312000", 8], [1386004317000, 10]]'), "source.packetsHisto short");
    eq_or_diff($json->{graph}->{"destination.packetsHisto"}, from_json('[["1386004309000", 6], ["1386004312000", 7], [1386004317000, 7]]'), "destination.packetsHisto short");
    eq_or_diff($json->{graph}->{"client.bytesHisto"}, from_json('[["1386004309000", 155], ["1386004312000", 171], [1386004317000, 160]]'), "client.bytesHisto short");
    eq_or_diff($json->{graph}->{"server.bytesHisto"}, from_json('[["1386004309000", 1599], ["1386004312000", 1599], [1386004317000, 1603]]'), "server.bytesHisto short");
    is ($json->{recordsFiltered}, 3, "records short");
    is ($json->{graph}->{interval}, 1, "correct interval short");
    is ($json->{graph}->{xmax}, 1386004400000, "correct xmax short");
    is ($json->{graph}->{xmin}, 1386004308000, "correct xmin short");

# Check facets medium
    $json = get("/sessions.json?map=true&startTime=1386004308&stopTime=1386349908&facets=1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"dst":{"US": 3}, "src":{"US": 3}, "xffGeo":{}}'), "map medium");
    eq_or_diff($json->{graph}->{sessionsHisto}, from_json('[["1386004260000", 3]]'), "sessionsHisto medium");
    eq_or_diff($json->{graph}->{"source.packetsHisto"}, from_json('[["1386004260000", 26]]'), "source.packetsHisto medium");
    eq_or_diff($json->{graph}->{"destination.packetsHisto"}, from_json('[["1386004260000", 20]]'), "destination.packetsHisto medium");
    eq_or_diff($json->{graph}->{"client.bytesHisto"}, from_json('[["1386004260000", 486]]'), "client.bytesHisto medium");
    eq_or_diff($json->{graph}->{"server.bytesHisto"}, from_json('[["1386004260000", 4801]]'), "server.bytesHisto medium");
    is ($json->{recordsFiltered}, 3, "records medium");
    is ($json->{graph}->{interval}, 60, "correct interval medium");
    is ($json->{graph}->{xmax}, 1386349908000, "correct xmax medium");
    is ($json->{graph}->{xmin}, 1386004308000, "correct xmin medium");

# Check facets ALL
    $json = get("/sessions.json?map=true&date=-1&forceAggregations=true&facets=1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"dst":{"US": 3, "CA": 1}, "src":{"US": 3, "RU":1}, "xffGeo":{}}'), "map ALL");
    eq_or_diff($json->{graph}->{sessionsHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1], [1482552000000,1]]'), "sessionsHisto ALL");
    eq_or_diff($json->{graph}->{"source.packetsHisto"}, from_json('[["1335956400000", 2], ["1386003600000", 26], [1387742400000, 3], [1482552000000,3]]'), "source.packetsHisto ALL");
    eq_or_diff($json->{graph}->{"destination.packetsHisto"}, from_json('[["1335956400000", 0], ["1386003600000", 20], [1387742400000, 1], [1482552000000,1]]'), "destination.packetsHisto ALL");
    eq_or_diff($json->{graph}->{"client.bytesHisto"}, from_json('[["1335956400000", 128], ["1386003600000", 486], [1387742400000, 68], [1482552000000,68]]'), "client.bytesHisto ALL");
    eq_or_diff($json->{graph}->{"server.bytesHisto"}, from_json('[["1335956400000", 0], ["1386003600000", 4801], [1387742400000, 0], [1482552000000,0]]'), "server.bytesHisto ALL");
    is ($json->{recordsFiltered}, 6, "records ALL");
    is ($json->{graph}->{interval}, 3600, "correct interval ALL");

# Check ip.protocol=blah (GET and POST)
    my $json = get("/sessions.json?date=-1&&spi=ipsrc&expression=" . uri_escape("file=$pwd/bigendian.pcap&&ip.protocol==blah"));
    is($json->{error}, "Unknown protocol string blah", "ip.protocol==blah");
    my $json = post("/api/sessions", '{"date":-1, "spi":"ipsrc", "expression":"file=' . $pwd . '/bigendian.pcap&&ip.protocol==blah"}');
    is($json->{error}, "Unknown protocol string blah", "ip.protocol==blah");

# csv
    my $csv = getBinary("/sessions.csv?date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap"))->content;
    $csv =~ s/\r//g;
    eq_or_diff ($csv, 'IP Protocol, Start Time, Stop Time, Src IP, Src Port, Src Country, Dst IP, Dst Port, Dst Country, Bytes, Data bytes, Packets, Arkime Node
tcp,1386004309468,1386004309478,10.180.156.185,53533,US,10.180.156.249,1080,US,2698,1754,14,test
tcp,1386004312331,1386004312384,10.180.156.185,53534,US,10.180.156.249,1080,US,2780,1770,15,test
tcp,1386004317979,1386004317989,10.180.156.185,53535,US,10.180.156.249,1080,US,2905,1763,17,test
', "CSV Expression");

    my $idQuery = get("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap"));
    #    diag "http://$MolochTest::host:8123/sessions.csv?date=-1&ids=" . $idQuery->{data}->[0]->{id};
    $csv = $MolochTest::userAgent->get("http://$MolochTest::host:8123/sessions.csv?date=-1&ids=" . $idQuery->{data}->[0]->{id})->content;
    $csv =~ s/\r//g;
    eq_or_diff ($csv,
'IP Protocol, Start Time, Stop Time, Src IP, Src Port, Src Country, Dst IP, Dst Port, Dst Country, Bytes, Data bytes, Packets, Arkime Node
tcp,1386004309468,1386004309478,10.180.156.185,53533,US,10.180.156.249,1080,US,2698,1754,14,test
', "CSV Ids");

    my $csv = getBinary("/sessions.csv?fields=firstPacket,lastPacket,source.ip,source.geo.country_iso_code,destination.ip,destination.geo.country_iso_code,network.packets,node,tcpflags.rst,tcpflags.psh,socks.ASN&date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap"))->content;
    $csv =~ s/\r//g;
    eq_or_diff ($csv, 'Start Time, Stop Time, Src IP, Src Country, Dst IP, Dst Country, Packets, Arkime Node, TCP Flag RST, TCP Flag PSH,  ASN
1386004309468,1386004309478,10.180.156.185,US,10.180.156.249,US,14,test,0,4,"AS15133 MCI Communications Services, Inc. d/b/a Verizon Business"
1386004312331,1386004312384,10.180.156.185,US,10.180.156.249,US,15,test,0,4,
1386004317979,1386004317989,10.180.156.185,US,10.180.156.249,US,17,test,0,6,"AS15133 MCI Communications Services, Inc. d/b/a Verizon Business"
', "CSV Expression");


# bigendian pcap fs tests
    my $json = get("/sessions.json?date=-1&fields=fileId&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    ok ($json->{data}->[0]->{fileId}->[0] =~ /bigendian.pcap/, "correct fs");

# bigendian pcap fs tests 2 fields
    my $json = get("/sessions.json?date=-1&fields=tls&fields=fileId&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    ok ($json->{data}->[0]->{fileId}->[0] =~ /bigendian.pcap/, "correct fs");

# no map data
    $json = get("/sessions.json?startTime=1386004308&stopTime=1386004400&facets=1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($json->{map}, from_json('{}'), "no map data");

# no aggregation data
    $json = get("/sessions.json?date=-1&facets=1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($json->{map}, from_json('{}'), "no map data");
    eq_or_diff($json->{graph}->{xmax}, undef, "empty graph data");

# Check file != blah.pcap
    my $json = get("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    is ($json->{recordsFiltered}, 6, "file ==");
    my $json = post("/api/sessions", '{"date": -1, "expression": "file!=' . $pwd . '/bigendian.pcap&&file=' . $pwd . '/socks-http-example.pcap|file=' . $pwd . '/bt-tcp.pcap"}');
    is ($json->{recordsFiltered}, 5, "file !=");

# Check file == EXISTS!
    my $json = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("file==EXISTS!&&file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    is ($json->{recordsFiltered}, 6, "file == EXISTS!");

# buildquery should return a query and indices for GET and POST
    $json = viewerGet("/api/buildquery");
    ok (exists $json->{esquery}, "buildquery returns esquery");
    ok (exists $json->{indices}, "buildquery returns indices");
    $json = viewerPost("/api/buildquery");
    ok (exists $json->{esquery}, "buildquery returns esquery");
    ok (exists $json->{indices}, "buildquery returns indices");

# should be able to download single pcap
    $response = getBinary("/api/session/test/" . $id . "/pcap");
    is (unpack("H*", $response->content), "a1b2c3d40002000400000000000000000000ffff000000014fa11b2900025436000000620000006200005e0001b10021280529ba08004500005430a70000ff010348c0a8b1a00a400b3108000afb43a800004fa11b290002538d08090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f30313233343536374fa11b2d00081331000000620000006200005e0001b10021280529ba08004500005430a80000ff010347c0a8b1a00a400b3108004bcb43ca00004fa11b2d0008129108090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f3031323334353637", "can download pcap");

# should get error if not able to download pcap
    $response = getBinary("/api/session/test/nonexistingid/pcap");
    is (unpack("H*", $response->content), "", "shouldn't find pcap");
    is ($response->{_rc}, "500", "can't find pcap returns 500");

# should be able to download multiple sessions pcap using list of ids
    $response = getBinary("/api/sessions/pcap/sessions.pcap?date=-1&segments=no&ids=". $id);
    is (unpack("H*", $response->content), "a1b2c3d40002000400000000000000000000ffff000000014fa11b2900025436000000620000006200005e0001b10021280529ba08004500005430a70000ff010348c0a8b1a00a400b3108000afb43a800004fa11b290002538d08090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f30313233343536374fa11b2d00081331000000620000006200005e0001b10021280529ba08004500005430a80000ff010347c0a8b1a00a400b3108004bcb43ca00004fa11b2d0008129108090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f3031323334353637", "can download pcap using list of ids");

# should get error if get pcap can't find sessions from list of ids
    $json = viewerGet("/api/sessions/pcap/sessions.pcap?date=-1&segments=no&ids=nonexistingid");
    is ($json->{text}, "no sessions found", "can't download pcap because sessions can't be found with list of ids");

# should be able to download multiple sessions pcap using query
    $response = getBinary("/api/sessions/pcap/sessions.pcap?length=10000&date=-1&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    is (unpack("H*", $response->content), "a1b2c3d40002000400000000000000000000ffff000000014fa11b2900025436000000620000006200005e0001b10021280529ba08004500005430a70000ff010348c0a8b1a00a400b3108000afb43a800004fa11b290002538d08090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f30313233343536374fa11b2d00081331000000620000006200005e0001b10021280529ba08004500005430a80000ff010347c0a8b1a00a400b3108004bcb43ca00004fa11b2d0008129108090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f3031323334353637", "can download pcap using query");
