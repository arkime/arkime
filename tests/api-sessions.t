use Test::More tests => 54;
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
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1386004309000", 14], ["1386004312000", 15], [1386004317000, 17]]'), "paHisto short");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1386004309000", 1754], ["1386004312000", 1770], [1386004317000, 1763]]'), "dbHisto short");
    is ($json->{recordsFiltered}, 3, "records short");
    is ($json->{graph}->{interval}, 1, "correct interval short");
    is ($json->{graph}->{xmax}, 1386004400000, "correct xmax short");
    is ($json->{graph}->{xmin}, 1386004308000, "correct xmin short");

# multi Check facets short
    $json = multiGet("/sessions.json?startTime=1386004308&stopTime=1386004400&facets=1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3}, "src":{"USA": 3}}'), "multi map short");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1386004309000", 1], ["1386004312000", 1], [1386004317000, 1]]'), "multi lpHisto short");
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1386004309000", 14], ["1386004312000", 15], [1386004317000, 17]]'), "multi paHisto short");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1386004309000", 1754], ["1386004312000", 1770], [1386004317000, 1763]]'), "multi dbHisto short");
    is ($json->{recordsFiltered}, 3, "multi records short");
    is ($json->{graph}->{interval}, 1, "multi correct interval short");
    is ($json->{graph}->{xmax}, 1386004400000, "multi correct xmax short");
    is ($json->{graph}->{xmin}, 1386004308000, "multi correct xmin short");

# Check facets medium
    $json = viewerGet("/sessions.json?startTime=1386004308&stopTime=1386349908&facets=1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3}, "src":{"USA": 3}}'), "map medium");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1386004260000", 3]]'), "lpHisto medium");
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1386004260000", 46]]'), "paHisto medium");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1386004260000", 5287]]'), "dbHisto medium");
    is ($json->{recordsFiltered}, 3, "records medium");
    is ($json->{graph}->{interval}, 60, "correct interval medium");
    is ($json->{graph}->{xmax}, 1386349908000, "correct xmax medium");
    is ($json->{graph}->{xmin}, 1386004308000, "correct xmin medium");

# mutli Check facets medium
    $json = multiGet("/sessions.json?startTime=1386004308&stopTime=1386349908&facets=1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3}, "src":{"USA": 3}}'), "multi map medium");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1386004260000", 3]]'), "multi lpHisto medium");
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1386004260000", 46]]'), "multi paHisto medium");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1386004260000", 5287]]'), "multi dbHisto medium");
    is ($json->{recordsFiltered}, 3, "multi records medium");
    is ($json->{graph}->{interval}, 60, "multi correct interval medium");
    is ($json->{graph}->{xmax}, 1386349908000, "multi correct xmax medium");
    is ($json->{graph}->{xmin}, 1386004308000, "multi correct xmin medium");

# Check facets ALL 
    $json = viewerGet("/sessions.json?date=-1&facets=1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3, "CAN": 1}, "src":{"USA": 3, "RUS":1}}'), "map ALL");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1], [1482552000000,1]]'), "lpHisto ALL");
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1335956400000", 2], ["1386003600000", 46], [1387742400000, 4], [1482552000000,4]]'), "paHisto ALL");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1335956400000", 0], ["1386003600000", 5287], [1387742400000, 68], [1482552000000,68]]'), "dbHisto ALL");
    is ($json->{recordsFiltered}, 6, "records ALL");
    is ($json->{graph}->{interval}, 3600, "correct interval ALL");

# multi Check facets ALL 
    $json = multiGet("/sessions.json?date=-1&facets=1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3, "CAN": 1}, "src":{"USA": 3, "RUS":1}}'), "multi map ALL");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1], [1482552000000,1]]'), "multi lpHisto ALL");
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1335956400000", 2], ["1386003600000", 46], [1387742400000, 4], [1482552000000,4]]'), "multi paHisto ALL");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1335956400000", 0], ["1386003600000", 5287], [1387742400000, 68], [1482552000000,68]]'), "multi dbHisto ALL");
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
