use Test::More tests => 70;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $pwd = "*/pcap";
my $fpwd = getcwd() . "/pcap";

# bigendian pcap file tests
    my $json = viewerGet("/spiview.json?date=-1&facets=1&spi=srcIp,dstIp,ipProtocol,fileand&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    my $mjson = multiGet("/spiview.json?date=-1&facets=1&spi=srcIp,dstIp,ipProtocol,fileand&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    my $djson = multiGet("/spiview.json?startTime=1332734457&stopTime=1389743152&facets=1&spi=srcIp,dstIp,ipProtocol,fileand&expression=" . uri_escape("file=$pwd/bigendian.pcap"));

    eq_or_diff($json->{map}, from_json('{"src": {}, "dst":{}}'), "map bigendian");
    eq_or_diff($json->{protocols}, from_json('{"icmp": 1}'), "protocols bigendian");
    eq_or_diff($json->{graph}, from_json('{"xmin":null,"pa2Histo":[[1335956400000,0]],"pa1Histo":[[1335956400000,2]],"xmax":null,"db2Histo":[[1335956400000,0]],"interval":3600,"lpHisto":[[1335956400000,1]],"db1Histo":[[1335956400000,0]]}'), "graph bigendian");
    eq_or_diff($djson->{graph}, from_json('{"xmin":1332734457000,"pa2Histo":[[1335956400000,0]],"pa1Histo":[[1335956400000,2]],"db2Histo":[[1335956400000,0]],"interval":3600,"xmax":1389743152000,"lpHisto":[[1335956400000,1]],"db1Histo":[[1335956400000,0]]}'), "date graph bigendian");
    eq_or_diff($json->{spi}->{srcIp}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":"192.168.177.160"}]}'), "bigendian srcIp");
    eq_or_diff($json->{spi}->{dstIp}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":"10.64.11.49"}]}'), "bigendian dstIp");
    eq_or_diff($json->{spi}->{ipProtocol}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":"icmp"}]}'), "bigendian ipProtocol");
    eq_or_diff($json->{spi}->{fileand}, from_json(qq({"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":"$fpwd/bigendian.pcap"}]})), "bigendian fileand");

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
    $json = viewerGet("/spiview.json?date=-1&spi=srcIp,dstIp,ipProtocol&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    $mjson = multiGet("/spiview.json?date=-1&spi=srcIp,dstIp,ipProtocol&expression=" . uri_escape("file=$pwd/bigendian.pcap"));

    is (!exists $json->{map}, 1, "map bigendian no facets");
    is (!exists $json->{graph}, 1, "graph bigendian no facets");
    eq_or_diff($json->{spi}->{srcIp}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":"192.168.177.160"}]}'), "bigendian srcIp no facets");
    eq_or_diff($json->{spi}->{dstIp}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":"10.64.11.49"}]}'), "bigendian dstIp no facets");
    eq_or_diff($json->{spi}->{ipProtocol}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":"icmp"}]}'), "bigendian ipProtocol no facets");

    is ($json->{health}->{number_of_data_nodes}, 1, "Correct health number_of_data_nodes bigendian no facets");
    is ($mjson->{health}->{number_of_data_nodes}, 2, "Correct health number_of_data_nodes multi bigendian no facets");

    delete $json->{health};
    delete $mjson->{health};
    eq_or_diff($json, $mjson, "single doesn't match multi", { context => 3 });

# Check facets short
    $json = viewerGet("/spiview.json?startTime=1386004308&stopTime=1386004400&facets=1&spi=srcIp,dstIp,ipProtocol,fileand&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    $mjson = multiGet("/spiview.json?startTime=1386004308&stopTime=1386004400&facets=1&spi=srcIp,dstIp,ipProtocol,fileand&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"src":{"US": 3}, "dst":{"US": 3}}'), "map short");
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
    eq_or_diff($json->{spi}->{srcIp}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":3, "key":"10.180.156.185"}]}'), "short srcIp");
    eq_or_diff($json->{spi}->{dstIp}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":3, "key":"10.180.156.249"}]}'), "short dstIp");
    eq_or_diff($json->{spi}->{ipProtocol}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":3, "key":"tcp"}]}'), "short ipProtocol");
    eq_or_diff($json->{spi}->{fileand}, from_json(qq({"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":3, "key":"$fpwd/socks-http-example.pcap"}]})), "bigendian fileand");

    delete $json->{health};
    delete $mjson->{health};
    eq_or_diff($json, $mjson, "single doesn't match multi", { context => 3 });

# Check facets medium
    $json = viewerGet("/spiview.json?startTime=1386004308&stopTime=1386349908&facets=1&spi=srcIp,dstIp,ipProtocol&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    $mjson = multiGet("/spiview.json?startTime=1386004308&stopTime=1386349908&facets=1&spi=srcIp,dstIp,ipProtocol&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json->{map}, from_json('{"src":{"US": 3}, "dst":{"US": 3}}'), "map medium");
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
    eq_or_diff($json->{spi}->{srcIp}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0,
            "buckets":[{"doc_count":3, "key":"10.180.156.185"}]}'), "medium srcIp");
    eq_or_diff($json->{spi}->{dstIp}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0,
            "buckets":[{"doc_count":3, "key":"10.180.156.249"}]}'), "medium dstIp");
    eq_or_diff($json->{spi}->{ipProtocol}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0,
            "buckets":[{"doc_count":3, "key":"tcp"}]}'), "medium ipProtocol");

    delete $json->{health};
    delete $mjson->{health};
    eq_or_diff($json, $mjson, "single doesn't match multi", { context => 3 });

# Check facets ALL
    $json = viewerGet("/spiview.json?date=-1&facets=1&spi=srcIp,dstIp,ipProtocol,fileand,tags:5,http.requestHeader&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    $mjson = multiGet("/spiview.json?date=-1&facets=1&spi=srcIp,dstIp,ipProtocol,fileand,tags:5,http.requestHeader&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    $djson = viewerGet("/spiview.json?startTime=1332734457&stopTime=1482563001&facets=1&spi=srcIp,dstIp,ipProtocol,fileand,tags:5,http.requestHeader&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    # Sort alpha since counts are the same and could come back in random order
    @{$json->{spi}->{"http.requestHeader"}->{buckets}} = sort({$a->{key} cmp $b->{key}} @{$json->{spi}->{"http.requestHeader"}->{buckets}});
    @{$mjson->{spi}->{"http.requestHeader"}->{buckets}} = sort({$a->{key} cmp $b->{key}} @{$mjson->{spi}->{"http.requestHeader"}->{buckets}});
    @{$djson->{spi}->{"http.requestHeader"}->{buckets}} = sort({$a->{key} cmp $b->{key}} @{$djson->{spi}->{"http.requestHeader"}->{buckets}});

    eq_or_diff($json->{map}, from_json('{"dst":{"US": 3, "CA": 1}, "src":{"US": 3, "RU":1}}'), "map ALL");
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

    eq_or_diff($json->{spi}->{srcIp}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0,
            "buckets":[{"doc_count":3, "key":"10.180.156.185"},{"doc_count":1, "key":"10.0.0.1"}, {"doc_count":1, "key":"10.10.10.10"}, {"doc_count":1, "key":"192.168.177.160"}]}'), "ALL srcIp");
    eq_or_diff($json->{spi}->{dstIp}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0,
            "buckets":[{"doc_count":3, "key":"10.180.156.249"}, {"doc_count":1, "key":"10.0.0.2"}, {"doc_count":1, "key":"10.11.11.11"}, {"doc_count":1, "key":"10.64.11.49"}]}'), "ALL dstIp");
    eq_or_diff($json->{spi}->{ipProtocol}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0,
            "buckets":[{"doc_count":5, "key":"tcp"}, {"doc_count":1, "key":"icmp"}]}'), "ALL ipProtocol");
    eq_or_diff($json->{spi}->{fileand}, from_json(qq({"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":3, "key":"$fpwd/socks-http-example.pcap"}, {"doc_count":2, "key":"$fpwd/bt-tcp.pcap"},{"doc_count":1, "key":"$fpwd/bigendian.pcap"}]})), "bigendian fileand");

    my @buckets = sort {$a->{key} cmp $b->{key}} @{$json->{spi}->{tags}->{buckets}};
    $json->{spi}->{tags}->{buckets} = \@buckets;

    my @mbuckets = sort {$a->{key} cmp $b->{key}} @{$mjson->{spi}->{tags}->{buckets}};
    $mjson->{spi}->{tags}->{buckets} = \@mbuckets;

    my @dbuckets = sort {$a->{key} cmp $b->{key}} @{$djson->{spi}->{tags}->{buckets}};
    $djson->{spi}->{tags}->{buckets} = \@dbuckets;

    eq_or_diff($json->{spi}->{tags}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 8,
            "buckets":[{"doc_count":3, "key":"byhost2"},{"doc_count":3, "key":"domainwise"},{"doc_count":3, "key":"hosttaggertest1"},{"doc_count":3, "key":"hosttaggertest2"},{"doc_count":3, "key":"wisebyhost2"}]}'), "ALL ta");

    SKIP: {
        skip "Upgrade test", 1 if ($ENV{MOLOCH_REINDEX_TEST}); # reindex doesn't have http.requestHeader 
        eq_or_diff($json->{spi}->{"http.requestHeader"}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0,
                "buckets":[{"doc_count":3, "key":"accept"},{"doc_count":3, "key":"host"}, {"doc_count":3, "key":"user-agent"}]}'), "ALL http.requestHeader");
    }

    delete $json->{health};
    delete $mjson->{health};
    delete $djson->{health};
    $djson->{graph}->{xmax} = undef;
    $djson->{graph}->{xmin} = undef;
    eq_or_diff($mjson, $json, "single doesn't match multi", { context => 3 });
    eq_or_diff($djson, $json, "single doesn't match date", { context => 3 });
