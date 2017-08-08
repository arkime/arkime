use Test::More tests => 70;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $pwd = getcwd() . "/pcap";

# bigendian pcap file tests
    my $json = viewerGet("/spiview.json?date=-1&facets=1&spi=a1,a2,pr,fileand&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    my $mjson = multiGet("/spiview.json?date=-1&facets=1&spi=a1,a2,pr,fileand&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    my $djson = multiGet("/spiview.json?startTime=1332734457&stopTime=1389743152&facets=1&spi=a1,a2,pr,fileand&expression=" . uri_escape("file=$pwd/bigendian.pcap"));

    eq_or_diff($json->{map}, from_json('{"src": {}, "dst":{}}'), "map bigendian");
    eq_or_diff($json->{protocols}, from_json('{"icmp": 1}'), "protocols bigendian");
    eq_or_diff($json->{graph}, from_json('{"xmin":null,"pa2Histo":[[1335956400000,0]],"pa1Histo":[[1335956400000,2]],"xmax":null,"db2Histo":[[1335956400000,0]],"interval":3600,"lpHisto":[[1335956400000,1]],"db1Histo":[[1335956400000,0]]}'), "graph bigendian");
    eq_or_diff($djson->{graph}, from_json('{"xmin":1332734457000,"pa2Histo":[[1335956400000,0]],"pa1Histo":[[1335956400000,2]],"db2Histo":[[1335956400000,0]],"interval":3600,"xmax":1389743152000,"lpHisto":[[1335956400000,1]],"db1Histo":[[1335956400000,0]]}'), "date graph bigendian");
    eq_or_diff($json->{spi}->{a1}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":3232280992}]}'), "bigendian a1");
    eq_or_diff($json->{spi}->{a2}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":171969329}]}'), "bigendian a2");
    eq_or_diff($json->{spi}->{pr}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":"icmp"}]}'), "bigendian pr");
    eq_or_diff($json->{spi}->{fileand}, from_json(qq({"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":"$pwd/bigendian.pcap"}]})), "bigendian fileand");

    is ($json->{health}->{number_of_data_nodes}, 1, "Correct health number_of_data_nodes bigendian");
    is ($mjson->{health}->{number_of_data_nodes}, 2, "Correct health number_of_data_nodes multi bigendian");

    delete $json->{health};
    delete $mjson->{health};
    delete $djson->{health};
    $djson->{graph}->{xmax} = undef;
    $djson->{graph}->{xmin} = undef;
    eq_or_diff($json, $mjson, "single doesn't match multi", { context => 3 });
    eq_or_diff($json, $djson, "single doesn't match date", { context => 3 });

# bigendian pcap file tests no facets
    $json = viewerGet("/spiview.json?date=-1&spi=a1,a2,pr&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    $mjson = multiGet("/spiview.json?date=-1&spi=a1,a2,pr&expression=" . uri_escape("file=$pwd/bigendian.pcap"));

    is (!exists $json->{map}, 1, "map bigendian no facets");
    is (!exists $json->{graph}, 1, "graph bigendian no facets");
    eq_or_diff($json->{spi}->{a1}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":3232280992}]}'), "bigendian a1 no facets");
    eq_or_diff($json->{spi}->{a2}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":171969329}]}'), "bigendian a2 no facets");
    eq_or_diff($json->{spi}->{pr}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":"icmp"}]}'), "bigendian pr no facets");

    is ($json->{health}->{number_of_data_nodes}, 1, "Correct health number_of_data_nodes bigendian no facets");
    is ($mjson->{health}->{number_of_data_nodes}, 2, "Correct health number_of_data_nodes multi bigendian no facets");

    delete $json->{health};
    delete $mjson->{health};
    eq_or_diff($json, $mjson, "single doesn't match multi", { context => 3 });

# Check facets short
    $json = viewerGet("/spiview.json?startTime=1386004308&stopTime=1386004400&facets=1&spi=a1,a2,pr,fileand&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    $mjson = multiGet("/spiview.json?startTime=1386004308&stopTime=1386004400&facets=1&spi=a1,a2,pr,fileand&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"src":{"USA": 3}, "dst":{"USA": 3}}'), "map short");
    eq_or_diff($json->{protocols}, from_json('{"http": 3, "socks": 3, "tcp": 3}'), "protocols short");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1386004309000", 1], ["1386004312000", 1], [1386004317000, 1]]'), "lpHisto short");
    eq_or_diff($json->{graph}->{pa1Histo}, from_json('[["1386004309000", 8], ["1386004312000", 8], [1386004317000, 10]]'), "pa1Histo short");
    eq_or_diff($json->{graph}->{pa2Histo}, from_json('[["1386004309000", 6], ["1386004312000", 7], [1386004317000, 7]]'), "pa2Histo short");
    eq_or_diff($json->{graph}->{db1Histo}, from_json('[["1386004309000", 155], ["1386004312000", 171], [1386004317000, 160]]'), "db1Histo short");
    eq_or_diff($json->{graph}->{db2Histo}, from_json('[["1386004309000", 1599], ["1386004312000", 1599], [1386004317000, 1603]]'), "db2Histo short");
    is ($json->{recordsFiltered}, 3, "records short");
    is ($json->{graph}->{interval}, 1, "correct interval short");
    is ($json->{graph}->{xmax}, 1386004400000, "correct xmax short");
    is ($json->{graph}->{xmin}, 1386004308000, "correct xmin short");
    eq_or_diff($json->{spi}->{a1}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":3, "key":179608761}]}'), "short a1");
    eq_or_diff($json->{spi}->{a2}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":3, "key":179608825}]}'), "short a2");
    eq_or_diff($json->{spi}->{pr}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":3, "key":"tcp"}]}'), "short pr");
    eq_or_diff($json->{spi}->{fileand}, from_json(qq({"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":3, "key":"$pwd/socks-http-example.pcap"}]})), "bigendian fileand");

    delete $json->{health};
    delete $mjson->{health};
    eq_or_diff($json, $mjson, "single doesn't match multi", { context => 3 });

# Check facets medium
    $json = viewerGet("/spiview.json?startTime=1386004308&stopTime=1386349908&facets=1&spi=a1,a2,pr&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    $mjson = multiGet("/spiview.json?startTime=1386004308&stopTime=1386349908&facets=1&spi=a1,a2,pr&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"src":{"USA": 3}, "dst":{"USA": 3}}'), "map medium");
    eq_or_diff($json->{protocols}, from_json('{"http": 3, "socks": 3, "tcp": 3}'), "protocols medium");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1386004260000", 3]]'), "lpHisto medium");
    eq_or_diff($json->{graph}->{pa1Histo}, from_json('[["1386004260000", 26]]'), "pa1Histo medium");
    eq_or_diff($json->{graph}->{pa2Histo}, from_json('[["1386004260000", 20]]'), "pa1Histo medium");
    eq_or_diff($json->{graph}->{db1Histo}, from_json('[["1386004260000", 486]]'), "db2Histo medium");
    eq_or_diff($json->{graph}->{db2Histo}, from_json('[["1386004260000", 4801]]'), "db2Histo medium");
    is ($json->{recordsFiltered}, 3, "records medium");
    is ($json->{graph}->{interval}, 60, "correct interval medium");
    is ($json->{graph}->{xmax}, 1386349908000, "correct xmax medium");
    is ($json->{graph}->{xmin}, 1386004308000, "correct xmin medium");
    eq_or_diff($json->{spi}->{a1}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0,
            "buckets":[{"doc_count":3, "key":179608761}]}'), "medium a1");
    eq_or_diff($json->{spi}->{a2}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0,
            "buckets":[{"doc_count":3, "key":179608825}]}'), "medium a2");
    eq_or_diff($json->{spi}->{pr}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0,
            "buckets":[{"doc_count":3, "key":"tcp"}]}'), "medium pr");

    delete $json->{health};
    delete $mjson->{health};
    eq_or_diff($json, $mjson, "single doesn't match multi", { context => 3 });

# Check facets ALL
    $json = viewerGet("/spiview.json?date=-1&facets=1&spi=a1,a2,pr,fileand,ta:3,hh1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    $mjson = multiGet("/spiview.json?date=-1&facets=1&spi=a1,a2,pr,fileand,ta:3,hh1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    $djson = viewerGet("/spiview.json?startTime=1332734457&stopTime=1482563001&facets=1&spi=a1,a2,pr,fileand,ta:3,hh1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    # Sort alpha since counts are the same and could come back in random order
    @{$json->{spi}->{hh1}->{buckets}} = sort({$a->{key} cmp $b->{key}} @{$json->{spi}->{hh1}->{buckets}});
    @{$mjson->{spi}->{hh1}->{buckets}} = sort({$a->{key} cmp $b->{key}} @{$mjson->{spi}->{hh1}->{buckets}});
    @{$djson->{spi}->{hh1}->{buckets}} = sort({$a->{key} cmp $b->{key}} @{$djson->{spi}->{hh1}->{buckets}});

    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3, "CAN": 1}, "src":{"USA": 3, "RUS":1}}'), "map ALL");
    eq_or_diff($json->{protocols}, from_json('{"tcp": 5, "http": 3, "socks": 3, "bittorrent": 2, "icmp": 1}'), "protocols ALL");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1], [1482552000000,1]]'), "lpHisto ALL");
    eq_or_diff($json->{graph}->{pa1Histo}, from_json('[["1335956400000", 2], ["1386003600000", 26], [1387742400000, 3], [1482552000000,3]]'), "pa1Histo ALL");
    eq_or_diff($json->{graph}->{pa2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 20], [1387742400000, 1], [1482552000000,1]]'), "pa2Histo ALL");
    eq_or_diff($json->{graph}->{db1Histo}, from_json('[["1335956400000", 0], ["1386003600000", 486], [1387742400000, 68], [1482552000000,68]]'), "db1Histo ALL");
    eq_or_diff($json->{graph}->{db2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 4801], [1387742400000, 0], [1482552000000,0]]'), "db2Histo ALL");
    is($djson->{graph}->{xmin}, 1332734457000, "date graph ALL xmin");
    is($djson->{graph}->{xmax}, 1482563001000, "date graph ALL xmax");
    is ($json->{recordsFiltered}, 6, "records ALL");
    is ($json->{graph}->{interval}, 3600, "correct interval ALL");

    eq_or_diff($json->{spi}->{a1}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0,
            "buckets":[{"doc_count":3, "key":179608761},{"doc_count":1, "key":167772161}, {"doc_count":1, "key":168430090}, {"doc_count":1, "key":3232280992}]}'), "ALL a1");
    eq_or_diff($json->{spi}->{a2}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0,
            "buckets":[{"doc_count":3, "key":179608825}, {"doc_count":1, "key":167772162}, {"doc_count":1, "key":168495883}, {"doc_count":1, "key":171969329}]}'), "ALL a2");
    eq_or_diff($json->{spi}->{pr}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0,
            "buckets":[{"doc_count":5, "key":"tcp"}, {"doc_count":1, "key":"icmp"}]}'), "ALL pr");
    eq_or_diff($json->{spi}->{fileand}, from_json(qq({"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":3, "key":"$pwd/socks-http-example.pcap"}, {"doc_count":2, "key":"$pwd/bt-tcp.pcap"},{"doc_count":1, "key":"$pwd/bigendian.pcap"}]})), "bigendian fileand");

    my @buckets = sort {$a->{key} cmp $b->{key}} @{$json->{spi}->{ta}->{buckets}};
    $json->{spi}->{ta}->{buckets} = \@buckets;

    my @mbuckets = sort {$a->{key} cmp $b->{key}} @{$mjson->{spi}->{ta}->{buckets}};
    $mjson->{spi}->{ta}->{buckets} = \@mbuckets;

    my @dbuckets = sort {$a->{key} cmp $b->{key}} @{$djson->{spi}->{ta}->{buckets}};
    $djson->{spi}->{ta}->{buckets} = \@dbuckets;

    eq_or_diff($json->{spi}->{ta}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 14,
            "buckets":[{"doc_count":3, "key":"byhost2"},{"doc_count":3, "key":"hosttaggertest1"},{"doc_count":3, "key":"hosttaggertest2"}]}'), "ALL ta");

    eq_or_diff($json->{spi}->{hh1}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0,
            "buckets":[{"doc_count":3, "key":"accept"},{"doc_count":3, "key":"host"}, {"doc_count":3, "key":"user-agent"}]}'), "ALL hh1");

    delete $json->{health};
    delete $mjson->{health};
    delete $djson->{health};
    $djson->{graph}->{xmax} = undef;
    $djson->{graph}->{xmin} = undef;
    eq_or_diff($mjson, $json, "single doesn't match multi", { context => 3 });
    eq_or_diff($djson, $json, "single doesn't match date", { context => 3 });
