use Test::More tests => 66;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $pwd = getcwd() . "/pcap";

# bigendian pcap file tests
    my $json = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    is ($json->{recordsFiltered}, 1, "bigendian recordsFiltered");
    my $response = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/raw/" . $json->{data}->[0]->{id} . "?type=src");
    is (unpack("H*", $response->content), "4fa11b290002538d08090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f30313233343536374fa11b2d0008129108090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f3031323334353637", "Correct bigendian tcpdump data");

# multi bigendian pcap file tests
    my $json = multiGet("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    is ($json->{recordsFiltered}, 1, "multi bigendian recordsFiltered");
    my $response = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/raw/" . $json->{data}->[0]->{id} . "?type=src");
    is (unpack("H*", $response->content), "4fa11b290002538d08090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f30313233343536374fa11b2d0008129108090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f3031323334353637", "multi Correct bigendian tcpdump data");

# Check facets short
    $json = viewerGet("/sessions.json?startTime=1386004308&stopTime=1386004400&facets=1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3}, "src":{"USA": 3}}'), "map short");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1386004309000", 1], ["1386004312000", 1], [1386004317000, 1]]'), "lpHisto short");
    eq_or_diff($json->{graph}->{pa1Histo}, from_json('[["1386004309000", 8], ["1386004312000", 8], [1386004317000, 10]]'), "pa1Histo short");
    eq_or_diff($json->{graph}->{pa2Histo}, from_json('[["1386004309000", 6], ["1386004312000", 7], [1386004317000, 7]]'), "pa2Histo short");
    eq_or_diff($json->{graph}->{db1Histo}, from_json('[["1386004309000", 155], ["1386004312000", 171], [1386004317000, 160]]'), "db1Histo short");
    eq_or_diff($json->{graph}->{db2Histo}, from_json('[["1386004309000", 1599], ["1386004312000", 1599], [1386004317000, 1603]]'), "db2Histo short");
    is ($json->{recordsFiltered}, 3, "records short");
    is ($json->{graph}->{interval}, 1, "correct interval short");
    is ($json->{graph}->{xmax}, 1386004400000, "correct xmax short");
    is ($json->{graph}->{xmin}, 1386004308000, "correct xmin short");

# multi Check facets short
    $json = multiGet("/sessions.json?startTime=1386004308&stopTime=1386004400&facets=1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3}, "src":{"USA": 3}}'), "multi map short");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1386004309000", 1], ["1386004312000", 1], [1386004317000, 1]]'), "multi lpHisto short");
    eq_or_diff($json->{graph}->{pa1Histo}, from_json('[["1386004309000", 8], ["1386004312000", 8], [1386004317000, 10]]'), "multi pa1Histo short");
    eq_or_diff($json->{graph}->{pa2Histo}, from_json('[["1386004309000", 6], ["1386004312000", 7], [1386004317000, 7]]'), "multi pa2Histo short");
    eq_or_diff($json->{graph}->{db1Histo}, from_json('[["1386004309000", 155], ["1386004312000", 171], [1386004317000, 160]]'), "multi db1Histo short");
    eq_or_diff($json->{graph}->{db2Histo}, from_json('[["1386004309000", 1599], ["1386004312000", 1599], [1386004317000, 1603]]'), "multi db2Histo short");
    is ($json->{recordsFiltered}, 3, "multi records short");
    is ($json->{graph}->{interval}, 1, "multi correct interval short");
    is ($json->{graph}->{xmax}, 1386004400000, "multi correct xmax short");
    is ($json->{graph}->{xmin}, 1386004308000, "multi correct xmin short");

# Check facets medium
    $json = viewerGet("/sessions.json?startTime=1386004308&stopTime=1386349908&facets=1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3}, "src":{"USA": 3}}'), "map medium");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1386004260000", 3]]'), "lpHisto medium");
    eq_or_diff($json->{graph}->{pa1Histo}, from_json('[["1386004260000", 26]]'), "pa1Histo medium");
    eq_or_diff($json->{graph}->{pa2Histo}, from_json('[["1386004260000", 20]]'), "pa2Histo medium");
    eq_or_diff($json->{graph}->{db1Histo}, from_json('[["1386004260000", 486]]'), "db1Histo medium");
    eq_or_diff($json->{graph}->{db2Histo}, from_json('[["1386004260000", 4801]]'), "db2Histo medium");
    is ($json->{recordsFiltered}, 3, "records medium");
    is ($json->{graph}->{interval}, 60, "correct interval medium");
    is ($json->{graph}->{xmax}, 1386349908000, "correct xmax medium");
    is ($json->{graph}->{xmin}, 1386004308000, "correct xmin medium");

# mutli Check facets medium
    $json = multiGet("/sessions.json?startTime=1386004308&stopTime=1386349908&facets=1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3}, "src":{"USA": 3}}'), "multi map medium");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1386004260000", 3]]'), "multi lpHisto medium");
    eq_or_diff($json->{graph}->{pa1Histo}, from_json('[["1386004260000", 26]]'), "multi pa1Histo medium");
    eq_or_diff($json->{graph}->{pa2Histo}, from_json('[["1386004260000", 20]]'), "multi pa2Histo medium");
    eq_or_diff($json->{graph}->{db1Histo}, from_json('[["1386004260000", 486]]'), "multi db1Histo medium");
    eq_or_diff($json->{graph}->{db2Histo}, from_json('[["1386004260000", 4801]]'), "multi db2Histo medium");
    is ($json->{recordsFiltered}, 3, "multi records medium");
    is ($json->{graph}->{interval}, 60, "multi correct interval medium");
    is ($json->{graph}->{xmax}, 1386349908000, "multi correct xmax medium");
    is ($json->{graph}->{xmin}, 1386004308000, "multi correct xmin medium");

# Check facets ALL 
    $json = viewerGet("/sessions.json?date=-1&facets=1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3, "CAN": 1}, "src":{"USA": 3, "RUS":1}}'), "map ALL");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1], [1482552000000,1]]'), "lpHisto ALL");
    eq_or_diff($json->{graph}->{pa1Histo}, from_json('[["1335956400000", 2], ["1386003600000", 26], [1387742400000, 3], [1482552000000,3]]'), "pa1Histo ALL");
    eq_or_diff($json->{graph}->{pa2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 20], [1387742400000, 1], [1482552000000,1]]'), "pa2Histo ALL");
    eq_or_diff($json->{graph}->{db1Histo}, from_json('[["1335956400000", 0], ["1386003600000", 486], [1387742400000, 68], [1482552000000,68]]'), "db1Histo ALL");
    eq_or_diff($json->{graph}->{db2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 4801], [1387742400000, 0], [1482552000000,0]]'), "db2Histo ALL");
    is ($json->{recordsFiltered}, 6, "records ALL");
    is ($json->{graph}->{interval}, 3600, "correct interval ALL");

# multi Check facets ALL 
    $json = multiGet("/sessions.json?date=-1&facets=1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3, "CAN": 1}, "src":{"USA": 3, "RUS":1}}'), "multi map ALL");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1], [1482552000000,1]]'), "multi lpHisto ALL");
    eq_or_diff($json->{graph}->{pa1Histo}, from_json('[["1335956400000", 2], ["1386003600000", 26], [1387742400000, 3], [1482552000000,3]]'), "multi pa1Histo ALL");
    eq_or_diff($json->{graph}->{pa2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 20], [1387742400000, 1], [1482552000000,1]]'), "multi pa2Histo ALL");
    eq_or_diff($json->{graph}->{db1Histo}, from_json('[["1335956400000", 0], ["1386003600000", 486], [1387742400000, 68], [1482552000000,68]]'), "multi db1Histo ALL");
    eq_or_diff($json->{graph}->{db2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 4801], [1387742400000, 0], [1482552000000,0]]'), "multi db2Histo ALL");
    is ($json->{recordsFiltered}, 6, "multi records ALL");
    is ($json->{graph}->{interval}, 3600, "multi correct interval ALL");

# Check ip.protocol=blah
    my $json = viewerGet("/sessions.json?date=-1&&spi=a1&expression=" . uri_escape("file=$pwd/bigendian.pcap&&ip.protocol==blah"));
    is($json->{bsqErr}, "Unknown protocol string blah", "ip.protocol==blah");

# multi Check ip.protocol=blah
    my $json = multiGet("/sessions.json?date=-1&&spi=a1&expression=" . uri_escape("file=$pwd/bigendian.pcap&&ip.protocol==blah"));
    is($json->{bsqErr}, "Unknown protocol string blah", "multi ip.protocol==blah");

# csv
    my $csv = $MolochTest::userAgent->get("http://$MolochTest::host:8123/sessions.csv?date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap"))->content;
    $csv =~ s/\r//g;
    eq_or_diff ($csv, 'IP Protocol, Start Time, Stop Time, Src IP, Src Port, Src Country, Dst IP, Dst Port, Dst Country, Bytes, Data bytes, Packets, Moloch Node
tcp,1386004309,1386004309,10.180.156.185,53533,USA,10.180.156.249,1080,USA,2698,1754,14,test
tcp,1386004312,1386004312,10.180.156.185,53534,USA,10.180.156.249,1080,USA,2780,1770,15,test
tcp,1386004317,1386004317,10.180.156.185,53535,USA,10.180.156.249,1080,USA,2905,1763,17,test
', "CSV Expression");
   
    my $idQuery = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap"));
    $csv = $MolochTest::userAgent->get("http://$MolochTest::host:8123/sessions.csv?date=-1&ids=" . $idQuery->{data}->[0]->{id})->content;
    $csv =~ s/\r//g;
    eq_or_diff ($csv, 
'IP Protocol, Start Time, Stop Time, Src IP, Src Port, Src Country, Dst IP, Dst Port, Dst Country, Bytes, Data bytes, Packets, Moloch Node
tcp,1386004309,1386004309,10.180.156.185,53533,USA,10.180.156.249,1080,USA,2698,1754,14,test
', "CSV Ids");

# bigendian pcap fs tests
    my $json = viewerGet("/sessions.json?date=-1&fields=fs&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    ok ($json->{data}->[0]->{fs}->[0] =~ /bigendian.pcap/, "correct fs");

# bigendian pcap fs tests 2 fields
    my $json = viewerGet("/sessions.json?date=-1&fields=tls&fields=fs&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    ok ($json->{data}->[0]->{fs}->[0] =~ /bigendian.pcap/, "correct fs");
