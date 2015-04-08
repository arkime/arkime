use Test::More tests => 86;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $pwd = getcwd() . "/pcap";

# bigendian pcap file tests
    my $json = viewerGet("/spiview.json?date=-1&facets=1&spi=a1,a2,pr&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    eq_or_diff($json->{map}, from_json('{"src": {}, "dst":{}}'), "map bigendian");
    eq_or_diff($json->{graph}, from_json('{"lpHisto": [[1335956400000, 1]], "dbHisto": [[1335956400000, 0]], "paHisto": [[1335956400000, 2]], "xmin": null, "xmax": null, "interval":3600}'), "graph bigendian");
    eq_or_diff($json->{spi}->{a1}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":1, "terms":[{"count":1, "term":3232280992}]}'), "bigendian a1");
    eq_or_diff($json->{spi}->{a2}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":1, "terms":[{"count":1, "term":171969329}]}'), "bigendian a2");
    eq_or_diff($json->{spi}->{pr}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":1, "terms":[{"count":1, "term":"icmp"}]}'), "bigendian pr");
    is ($json->{health}->{number_of_data_nodes}, 1, "Correct health number_of_data_nodes bigendian");

# multi bigendian pcap file tests
    my $json = multiGet("/spiview.json?date=-1&facets=1&spi=a1,a2,pr&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    eq_or_diff($json->{map}, from_json('{"src": {}, "dst":{}}'), "multi map bigendian");
    eq_or_diff($json->{graph}, from_json('{"lpHisto": [[1335956400000, 1]], "dbHisto": [[1335956400000, 0]], "paHisto": [[1335956400000, 2]], "xmin": null, "xmax": null, "interval":3600}'), "multi graph bigendian");
    eq_or_diff($json->{spi}->{a1}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":1, "terms":[{"count":1, "term":3232280992}]}'), "multi bigendian a1");
    eq_or_diff($json->{spi}->{a2}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":1, "terms":[{"count":1, "term":171969329}]}'), "multi bigendian a2");
    eq_or_diff($json->{spi}->{pr}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":1, "terms":[{"count":1, "term":"icmp"}]}'), "multi bigendian pr");
    is ($json->{health}->{number_of_data_nodes}, 2, "multi Correct health number_of_data_nodes bigendian");

# bigendian pcap file tests no facets
    my $json = viewerGet("/spiview.json?date=-1&spi=a1,a2,pr&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    is (!exists $json->{map}, 1, "map bigendian no facets");
    is (!exists $json->{graph}, 1, "graph bigendian no facets");
    eq_or_diff($json->{spi}->{a1}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":1, "terms":[{"count":1, "term":3232280992}]}'), "bigendian a1 no facets");
    eq_or_diff($json->{spi}->{a2}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":1, "terms":[{"count":1, "term":171969329}]}'), "bigendian a2 no facets");
    eq_or_diff($json->{spi}->{pr}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":1, "terms":[{"count":1, "term":"icmp"}]}'), "bigendian pr no facets");
    is ($json->{health}->{number_of_data_nodes}, 1, "Correct health number_of_data_nodes bigendian no facets");

# multi bigendian pcap file tests no facets
    my $json = multiGet("/spiview.json?date=-1&spi=a1,a2,pr&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    is (!exists $json->{map}, 1, "multi map bigendian no facets");
    is (!exists $json->{graph}, 1, "multi graph bigendian no facets");
    eq_or_diff($json->{spi}->{a1}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":1, "terms":[{"count":1, "term":3232280992}]}'), "multi bigendian a1 no facets");
    eq_or_diff($json->{spi}->{a2}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":1, "terms":[{"count":1, "term":171969329}]}'), "multi bigendian a2 no facets");
    eq_or_diff($json->{spi}->{pr}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":1, "terms":[{"count":1, "term":"icmp"}]}'), "multi bigendian pr no facets");
    is ($json->{health}->{number_of_data_nodes}, 2, "multi Correct health number_of_data_nodes bigendian no facets");


# Check facets short
    $json = viewerGet("/spiview.json?startTime=1386004308&stopTime=1386004400&facets=1&spi=a1,a2,pr&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"src":{"USA": 3}, "dst":{"USA": 3}}'), "map short");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1386004309000", 1], ["1386004312000", 1], [1386004317000, 1]]'), "lpHisto short");
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1386004309000", 14], ["1386004312000", 15], [1386004317000, 17]]'), "paHisto short");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1386004309000", 1754], ["1386004312000", 1770], [1386004317000, 1763]]'), "dbHisto short");
    is ($json->{iTotalDisplayRecords}, 3, "records short");
    is ($json->{graph}->{interval}, 1, "correct interval short");
    is ($json->{graph}->{xmax}, 1386004400000, "correct xmax short");
    is ($json->{graph}->{xmin}, 1386004308000, "correct xmin short");
    eq_or_diff($json->{spi}->{a1}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":3, "terms":[{"count":3, "term":179608761}]}'), "short a1");
    eq_or_diff($json->{spi}->{a2}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":3, "terms":[{"count":3, "term":179608825}]}'), "short a2");
    eq_or_diff($json->{spi}->{pr}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":3, "terms":[{"count":3, "term":"tcp"}]}'), "short pr");

# multi Check facets short
    $json = multiGet("/spiview.json?startTime=1386004308&stopTime=1386004400&facets=1&spi=a1,a2,pr&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"src":{"USA": 3}, "dst":{"USA": 3}}'), "multi map short");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1386004309000", 1], ["1386004312000", 1], [1386004317000, 1]]'), "multi lpHisto short");
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1386004309000", 14], ["1386004312000", 15], [1386004317000, 17]]'), "multi paHisto short");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1386004309000", 1754], ["1386004312000", 1770], [1386004317000, 1763]]'), "multi dbHisto short");
    is ($json->{iTotalDisplayRecords}, 3, "multi records short");
    is ($json->{graph}->{interval}, 1, "multi correct interval short");
    is ($json->{graph}->{xmax}, 1386004400000, "multi correct xmax short");
    is ($json->{graph}->{xmin}, 1386004308000, "multi correct xmin short");
    eq_or_diff($json->{spi}->{a1}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":3, "terms":[{"count":3, "term":179608761}]}'), "multi short a1");
    eq_or_diff($json->{spi}->{a2}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":3, "terms":[{"count":3, "term":179608825}]}'), "multi short a2");
    eq_or_diff($json->{spi}->{pr}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":3, "terms":[{"count":3, "term":"tcp"}]}'), "multi short pr");


# Check facets medium
    $json = viewerGet("/spiview.json?startTime=1386004308&stopTime=1386349908&facets=1&spi=a1,a2,pr&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"src":{"USA": 3}, "dst":{"USA": 3}}'), "map medium");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1386004260000", 3]]'), "lpHisto medium");
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1386004260000", 46]]'), "paHisto medium");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1386004260000", 5287]]'), "dbHisto medium");
    is ($json->{iTotalDisplayRecords}, 3, "records medium");
    is ($json->{graph}->{interval}, 60, "correct interval medium");
    is ($json->{graph}->{xmax}, 1386349908000, "correct xmax medium");
    is ($json->{graph}->{xmin}, 1386004308000, "correct xmin medium");
    eq_or_diff($json->{spi}->{a1}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":3, 
            "terms":[{"count":3, "term":179608761}]}'), "medium a1");
    eq_or_diff($json->{spi}->{a2}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":3, 
            "terms":[{"count":3, "term":179608825}]}'), "medium a2");
    eq_or_diff($json->{spi}->{pr}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":3, 
            "terms":[{"count":3, "term":"tcp"}]}'), "medium pr");

# multi Check facets medium
    $json = multiGet("/spiview.json?startTime=1386004308&stopTime=1386349908&facets=1&spi=a1,a2,pr&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"src":{"USA": 3}, "dst":{"USA": 3}}'), "multi map medium");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1386004260000", 3]]'), "multi lpHisto medium");
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1386004260000", 46]]'), "multi paHisto medium");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1386004260000", 5287]]'), "multi dbHisto medium");
    is ($json->{iTotalDisplayRecords}, 3, "multi records medium");
    is ($json->{graph}->{interval}, 60, "multi correct interval medium");
    is ($json->{graph}->{xmax}, 1386349908000, "multi correct xmax medium");
    is ($json->{graph}->{xmin}, 1386004308000, "multi correct xmin medium");
    eq_or_diff($json->{spi}->{a1}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":3, 
            "terms":[{"count":3, "term":179608761}]}'), "multi medium a1");
    eq_or_diff($json->{spi}->{a2}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":3, 
            "terms":[{"count":3, "term":179608825}]}'), "multi medium a2");
    eq_or_diff($json->{spi}->{pr}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":3, 
            "terms":[{"count":3, "term":"tcp"}]}'), "multi medium pr");


# Check facets ALL 
    $json = viewerGet("/spiview.json?date=-1&facets=1&spi=a1,a2,pr&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3, "CAN": 1}, "src":{"USA": 3, "RUS":1}}'), "map ALL");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1]]'), "lpHisto ALL");
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1335956400000", 2], ["1386003600000", 46], [1387742400000, 4]]'), "paHisto ALL");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1335956400000", 0], ["1386003600000", 5287], [1387742400000, 68]]'), "dbHisto ALL");
    is ($json->{iTotalDisplayRecords}, 5, "records ALL");
    is ($json->{graph}->{interval}, 3600, "correct interval ALL");

    eq_or_diff($json->{spi}->{a1}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":5, 
            "terms":[{"count":3, "term":179608761},{"count":1, "term":3232280992}, {"count":1, "term":167772161}]}'), "ALL a1");
    eq_or_diff($json->{spi}->{a2}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":5, 
            "terms":[{"count":3, "term":179608825}, {"count":1, "term":171969329}, {"count":1, "term":167772162}]}'), "ALL a2");
    eq_or_diff($json->{spi}->{pr}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":5, 
            "terms":[{"count":4, "term":"tcp"}, {"count":1, "term":"icmp"}]}'), "ALL pr");

# multi Check facets ALL 
    $json = multiGet("/spiview.json?date=-1&facets=1&spi=a1,a2,pr&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3, "CAN": 1}, "src":{"USA": 3, "RUS":1}}'), "multi map ALL");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1]]'), "multi lpHisto ALL");
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1335956400000", 2], ["1386003600000", 46], [1387742400000, 4]]'), "multi paHisto ALL");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1335956400000", 0], ["1386003600000", 5287], [1387742400000, 68]]'), "multi dbHisto ALL");
    is ($json->{iTotalDisplayRecords}, 5, "multi records ALL");
    is ($json->{graph}->{interval}, 3600, "multi correct interval ALL");

    eq_or_diff($json->{spi}->{a1}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":5, 
            "terms":[{"count":3, "term":179608761},{"count":1, "term":167772161}, {"count":1, "term":3232280992}]}'), "multi ALL a1");
    eq_or_diff($json->{spi}->{a2}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":5, 
            "terms":[{"count":3, "term":179608825}, {"count":1, "term":167772162}, {"count":1, "term":171969329}]}'), "multi ALL a2");
    eq_or_diff($json->{spi}->{pr}, from_json('{"other": 0, "missing": 0, "_type":"terms", "total":5, 
            "terms":[{"count":4, "term":"tcp"}, {"count":1, "term":"icmp"}]}'), "multi ALL pr");
