use Test::More tests => 73;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $pwd = "*/pcap";

# bigendian pcap file tests
    my $json = viewerGet("/spiview.json?map=true&date=-1&facets=1&spi=srcIp,dstIp,ipProtocol,fileand&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    my $pjson = viewerPost("/api/spiview", '{"map":true, "date":-1, "facets":1, "spi":"srcIp,dstIp,ipProtocol,fileand", "expression":"file=' . $pwd . '/bigendian.pcap"}');
    my $mjson = multiGet("/spiview.json?map=true&date=-1&facets=1&spi=srcIp,dstIp,ipProtocol,fileand&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    my $djson = multiGet("/spiview.json?map=true&startTime=1332734457&stopTime=1389743152&facets=1&spi=srcIp,dstIp,ipProtocol,fileand&expression=" . uri_escape("file=$pwd/bigendian.pcap"));

    eq_or_diff($json, $pjson, "GET and POST versions of spiview endpoint are not the same");
    eq_or_diff($json->{map}, from_json('{"src": {}, "dst":{}, "xffGeo":{}}'), "map bigendian");
    eq_or_diff($json->{protocols}, from_json('{"icmp": 1}'), "protocols bigendian");
    eq_or_diff($json->{graph}, from_json('{"sessionsHisto":[[1335956400000,1]],"dstBytesHisto":[[1335956400000,0]],"srcPacketsHisto":[[1335956400000,2]],"srcBytesHisto":[[1335956400000,196]],"xmax":null,"srcDataBytesHisto":[[1335956400000,128]],"interval":3600,"xmin":null,"dstDataBytesHisto":[[1335956400000,0]],"dstPacketsHisto":[[1335956400000,0]],"sessionsTotal":1,"totBytesTotal":196,"totDataBytesTotal":128,"totPacketsTotal":2}'), "graph bigendian");
    eq_or_diff($djson->{graph}, from_json('{"srcDataBytesHisto":[[1335956400000,128]],"xmax":1389743152000,"srcBytesHisto":[[1335956400000,196]],"dstDataBytesHisto":[[1335956400000,0]],"interval":3600,"xmin":1332734457000,"dstPacketsHisto":[[1335956400000,0]],"dstBytesHisto":[[1335956400000,0]],"sessionsHisto":[[1335956400000,1]],"srcPacketsHisto":[[1335956400000,2]],"sessionsTotal":1,"totBytesTotal":196,"totDataBytesTotal":128,"totPacketsTotal":2}'), "date graph bigendian");
    eq_or_diff($json->{spi}->{srcIp}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":"192.168.177.160"}]}'), "bigendian srcIp");
    eq_or_diff($json->{spi}->{dstIp}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":"10.64.11.49"}]}'), "bigendian dstIp");
    eq_or_diff($json->{spi}->{ipProtocol}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":"icmp"}]}'), "bigendian ipProtocol");
    eq_or_diff($json->{spi}->{fileand}, from_json(qq({"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":"/DIR/tests/pcap/bigendian.pcap"}]})), "bigendian fileand");

    $djson->{graph}->{xmax} = undef;
    $djson->{graph}->{xmin} = undef;
    eq_or_diff($json, $mjson, "single doesn't match multi", { context => 3 });
    eq_or_diff($json, $djson, "single doesn't match date", { context => 3 });

# bigendian pcap file tests no facets
    $json = viewerGet("/spiview.json?date=-1&spi=srcIp,dstIp,ipProtocol&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    $pjson = viewerPost("/api/spiview", '{"date":-1, "spi":"srcIp,dstIp,ipProtocol", "expression":"file=' . $pwd . '/bigendian.pcap"}');
    $mjson = multiGet("/spiview.json?date=-1&spi=srcIp,dstIp,ipProtocol&expression=" . uri_escape("file=$pwd/bigendian.pcap"));

    eq_or_diff($json, $pjson, "GET and POST versions of spiview endpoint are not the same");
    is (!exists $json->{map}, 1, "map bigendian no facets");
    is (!exists $json->{graph}, 1, "graph bigendian no facets");
    eq_or_diff($json->{spi}->{srcIp}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":"192.168.177.160"}]}'), "bigendian srcIp no facets");
    eq_or_diff($json->{spi}->{dstIp}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":"10.64.11.49"}]}'), "bigendian dstIp no facets");
    eq_or_diff($json->{spi}->{ipProtocol}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":1, "key":"icmp"}]}'), "bigendian ipProtocol no facets");

    eq_or_diff($json, $mjson, "single doesn't match multi", { context => 3 });

# Check facets short
    $json = viewerGet("/spiview.json?map=true&startTime=1386004308&stopTime=1386004400&facets=1&spi=srcIp,dstIp,ipProtocol,fileand&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    $pjson = viewerPost("/api/spiview", '{"map":true, "startTime":1386004308, "stopTime":1386004400, "facets":1, "spi":"srcIp,dstIp,ipProtocol,fileand", "expression":"file=' . $pwd . '/bigendian.pcap|file=' . $pwd .'/socks-http-example.pcap|file=' . $pwd .'/bt-tcp.pcap"}');
    $mjson = multiGet("/spiview.json?map=true&startTime=1386004308&stopTime=1386004400&facets=1&spi=srcIp,dstIp,ipProtocol,fileand&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json, $pjson, "GET and POST versions of spiview endpoint are not the same");
    eq_or_diff($json->{map}, from_json('{"src":{"US": 3}, "dst":{"US": 3}, "xffGeo":{}}'), "map short");
    eq_or_diff($json->{protocols}, from_json('{"http": 3, "socks": 3, "tcp": 3}'), "protocols short");
    eq_or_diff($json->{graph}->{sessionsHisto}, from_json('[["1386004309000", 1], ["1386004312000", 1], [1386004317000, 1]]'), "sessionsHisto short");
    eq_or_diff($json->{graph}->{"source.packetsHisto"}, from_json('[["1386004309000", 8], ["1386004312000", 8], [1386004317000, 10]]'), "srcPacketsHisto short");
    eq_or_diff($json->{graph}->{"destination.packetsHisto"}, from_json('[["1386004309000", 6], ["1386004312000", 7], [1386004317000, 7]]'), "dstPacketsHisto short");
    eq_or_diff($json->{graph}->{"client.bytesHisto"}, from_json('[["1386004309000", 155], ["1386004312000", 171], [1386004317000, 160]]'), "srcDataBytesHisto short");
    eq_or_diff($json->{graph}->{"server.bytesHisto"}, from_json('[["1386004309000", 1599], ["1386004312000", 1599], [1386004317000, 1603]]'), "dstDataBytesHisto short");
    is ($json->{recordsFiltered}, 3, "records short");
    is ($json->{graph}->{interval}, 1, "correct interval short");
    is ($json->{graph}->{xmax}, 1386004400000, "correct xmax short");
    is ($json->{graph}->{xmin}, 1386004308000, "correct xmin short");
    eq_or_diff($json->{spi}->{srcIp}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":3, "key":"10.180.156.185"}]}'), "short srcIp");
    eq_or_diff($json->{spi}->{dstIp}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":3, "key":"10.180.156.249"}]}'), "short dstIp");
    eq_or_diff($json->{spi}->{ipProtocol}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":3, "key":"tcp"}]}'), "short ipProtocol");
    eq_or_diff($json->{spi}->{fileand}, from_json(qq({"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":3, "key":"/DIR/tests/pcap/socks-http-example.pcap"}]})), "bigendian fileand");

    eq_or_diff($json, $mjson, "single doesn't match multi", { context => 3 });

# Check facets medium
    $json = viewerGet("/spiview.json?map=true&startTime=1386004308&stopTime=1386349908&facets=1&spi=srcIp,dstIp,ipProtocol&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    $pjson = viewerPost("/api/spiview", '{"map":true, "startTime":1386004308, "stopTime":1386349908, "facets":1, "spi":"srcIp,dstIp,ipProtocol", "expression":"file=' . $pwd . '/bigendian.pcap|file=' . $pwd .'/socks-http-example.pcap|file=' . $pwd .'/bt-tcp.pcap"}');
    $mjson = multiGet("/spiview.json?map=true&startTime=1386004308&stopTime=1386349908&facets=1&spi=srcIp,dstIp,ipProtocol&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    eq_or_diff($json, $pjson, "GET and POST versions of spiview endpoint are not the same");
    eq_or_diff($json->{map}, from_json('{"src":{"US": 3}, "dst":{"US": 3}, "xffGeo":{}}'), "map medium");
    eq_or_diff($json->{protocols}, from_json('{"http": 3, "socks": 3, "tcp": 3}'), "protocols medium");
    eq_or_diff($json->{graph}->{sessionsHisto}, from_json('[["1386004260000", 3]]'), "sessionsHisto medium");
    eq_or_diff($json->{graph}->{"source.packetsHisto"}, from_json('[["1386004260000", 26]]'), "srcPacketsHisto medium");
    eq_or_diff($json->{graph}->{"destination.packetsHisto"}, from_json('[["1386004260000", 20]]'), "srcPacketsHisto medium");
    eq_or_diff($json->{graph}->{"client.bytesHisto"}, from_json('[["1386004260000", 486]]'), "dstDataBytesHisto medium");
    eq_or_diff($json->{graph}->{"server.bytesHisto"}, from_json('[["1386004260000", 4801]]'), "dstDataBytesHisto medium");
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

    eq_or_diff($json, $mjson, "single doesn't match multi", { context => 3 });

# Check facets ALL
    $json = viewerGet("/spiview.json?map=true&date=-1&facets=1&spi=srcIp,dstIp,ipProtocol,fileand,tags:5,http.requestHeader&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    $pjson = viewerPost("/api/spiview", '{"map":true, "date":-1, "facets":1, "spi":"srcIp,dstIp,ipProtocol,fileand,tags:5,http.requestHeader", "expression":"file=' . $pwd . '/bigendian.pcap|file=' . $pwd .'/socks-http-example.pcap|file=' . $pwd .'/bt-tcp.pcap"}');
    $mjson = multiGet("/spiview.json?map=true&date=-1&facets=1&spi=srcIp,dstIp,ipProtocol,fileand,tags:5,http.requestHeader&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    $djson = viewerGet("/spiview.json?map=true&startTime=1332734457&stopTime=1482563001&facets=1&spi=srcIp,dstIp,ipProtocol,fileand,tags:5,http.requestHeader&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));

    # Sort alpha since counts are the same and could come back in random order
    @{$json->{spi}->{"http.requestHeader"}->{buckets}} = sort({$a->{key} cmp $b->{key}} @{$json->{spi}->{"http.requestHeader"}->{buckets}});
    @{$pjson->{spi}->{"http.requestHeader"}->{buckets}} = sort({$a->{key} cmp $b->{key}} @{$pjson->{spi}->{"http.requestHeader"}->{buckets}});
    @{$mjson->{spi}->{"http.requestHeader"}->{buckets}} = sort({$a->{key} cmp $b->{key}} @{$mjson->{spi}->{"http.requestHeader"}->{buckets}});
    @{$djson->{spi}->{"http.requestHeader"}->{buckets}} = sort({$a->{key} cmp $b->{key}} @{$djson->{spi}->{"http.requestHeader"}->{buckets}});

    eq_or_diff($json, $pjson, "GET and POST versions of spiview endpoint are not the same");
    eq_or_diff($json->{map}, from_json('{"dst":{"US": 3, "CA": 1}, "src":{"US": 3, "RU":1}, "xffGeo":{}}'), "map ALL");
    eq_or_diff($json->{protocols}, from_json('{"tcp": 5, "http": 3, "socks": 3, "bittorrent": 2, "icmp": 1}'), "protocols ALL");
    eq_or_diff($json->{graph}->{sessionsHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1], [1482552000000,1]]'), "sessionsHisto ALL");
    eq_or_diff($json->{graph}->{"source.packetsHisto"}, from_json('[["1335956400000", 2], ["1386003600000", 26], [1387742400000, 3], [1482552000000,3]]'), "srcPacketsHisto ALL");
    eq_or_diff($json->{graph}->{"destination.packetsHisto"}, from_json('[["1335956400000", 0], ["1386003600000", 20], [1387742400000, 1], [1482552000000,1]]'), "dstPacketsHisto ALL");
    eq_or_diff($json->{graph}->{"client.bytesHisto"}, from_json('[["1335956400000", 128], ["1386003600000", 486], [1387742400000, 68], [1482552000000,68]]'), "srcDataBytesHisto ALL");
    eq_or_diff($json->{graph}->{"server.bytesHisto"}, from_json('[["1335956400000", 0], ["1386003600000", 4801], [1387742400000, 0], [1482552000000,0]]'), "dstDataBytesHisto ALL");
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
    eq_or_diff($json->{spi}->{fileand}, from_json(qq({"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0, "buckets":[{"doc_count":3, "key":"/DIR/tests/pcap/socks-http-example.pcap"}, {"doc_count":2, "key":"/DIR/tests/pcap/bt-tcp.pcap"},{"doc_count":1, "key":"/DIR/tests/pcap/bigendian.pcap"}]})), "bigendian fileand");

    my @buckets = sort {$a->{key} cmp $b->{key}} @{$json->{spi}->{tags}->{buckets}};
    $json->{spi}->{tags}->{buckets} = \@buckets;

    my @mbuckets = sort {$a->{key} cmp $b->{key}} @{$mjson->{spi}->{tags}->{buckets}};
    $mjson->{spi}->{tags}->{buckets} = \@mbuckets;

    my @dbuckets = sort {$a->{key} cmp $b->{key}} @{$djson->{spi}->{tags}->{buckets}};
    $djson->{spi}->{tags}->{buckets} = \@dbuckets;

    eq_or_diff($json->{spi}->{tags}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 8,
            "buckets":[{"doc_count":3, "key":"byhost2"},{"doc_count":3, "key":"domainwise"},{"doc_count":3, "key":"hosttaggertest1"},{"doc_count":3, "key":"hosttaggertest2"},{"doc_count":3, "key":"wisebyhost2"}]}'), "ALL ta");

    eq_or_diff($json->{spi}->{"http.requestHeader"}, from_json('{"doc_count_error_upper_bound": 0, "sum_other_doc_count": 0,
            "buckets":[{"doc_count":3, "key":"accept"},{"doc_count":3, "key":"host"}, {"doc_count":3, "key":"user-agent"}]}'), "ALL http.requestHeader");

    $djson->{graph}->{xmax} = undef;
    $djson->{graph}->{xmin} = undef;
    eq_or_diff($mjson, $json, "single doesn't match multi", { context => 3 });
    eq_or_diff($djson, $json, "single doesn't match date", { context => 3 });

# no map data
    $json = viewerGet("/spiview.json?date=-1&facets=1&spi=srcIp,dstIp,ipProtocol,fileand&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    $pjson = viewerPost("/api/spiview", '{"date":-1, "facets":1, "spi":"srcIp,dstIp,ipProtocol,fileand", "expression":"file=' . $pwd . '/bigendian.pcap"}');
    eq_or_diff($json->{map}, from_json('{}'), "no map data");
    eq_or_diff($pjson->{map}, from_json('{}'), "no map data");
