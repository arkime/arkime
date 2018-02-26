use Test::More tests => 77;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $pwd = "*/pcap";

my ($json, $mjson);

#node
    $json = viewerGet("/spigraph.json?date=-1&field=node&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($json->{map}, from_json('{"dst":{"US": 3, "CA": 1}, "src":{"US": 3, "RU":1}}'), "map field: no");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1], [1482552000000, 1]]'), "lpHisto field: node");
    eq_or_diff($json->{graph}->{pa1Histo}, from_json('[["1335956400000", 2], ["1386003600000", 26], [1387742400000, 3], [1482552000000, 3]]'), "pa1Histo field: node");
    eq_or_diff($json->{graph}->{pa2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 20], [1387742400000, 1], [1482552000000, 1]]'), "pa2Histo field: node");
    eq_or_diff($json->{graph}->{db1Histo}, from_json('[["1335956400000", 0], ["1386003600000", 486], [1387742400000, 68], [1482552000000, 68]]'), "db1Histo field: node");
    eq_or_diff($json->{graph}->{db2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 4801], [1387742400000, 0], [1482552000000, 0]]'), "db2Histo field: node");
    eq_or_diff($json->{items}, from_json('[{"graph":{"lpHisto":[[1335956400000,1],[1386003600000,3],[1387742400000,1],[1482552000000,1]],"xmin":1335956400000,"db2Histo":[[1335956400000,0],[1386003600000,4801],[1387742400000,0],[1482552000000,0]],"interval":3600,"xmax":1482552000000,"db1Histo":[[1335956400000,0],[1386003600000,486],[1387742400000,68],[1482552000000,68]],"pa2Histo":[[1335956400000,0],[1386003600000,20],[1387742400000,1],[1482552000000,1]],"pa1Histo":[[1335956400000,2],[1386003600000,26],[1387742400000,3],[1482552000000,3]]},"dbHisto":5423,"map":{"dst":{"CA":1,"US":3},"src":{"US":3,"RU":1}},"count":6,"name":"test","paHisto":56,"lpHisto":6}]'), "items field: node", { context => 3 });
    cmp_ok ($json->{recordsTotal}, '>=', 194);
    cmp_ok ($json->{recordsFiltered}, '==', 6);

#node multi
    $mjson = multiGet("/spigraph.json?date=-1&field=node&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($mjson->{map}, $json->{map}, "multi map field: node");
    eq_or_diff($mjson->{graph}->{lpHisto}, $json->{graph}->{lpHisto}, "multi lpHisto field: node");
    eq_or_diff($mjson->{graph}->{pa1Histo}, $json->{graph}->{pa1Histo}, "multi pa1Histo field: node");
    eq_or_diff($mjson->{graph}->{pa2Histo}, $json->{graph}->{pa2Histo}, "multi pa2Histo field: node");
    eq_or_diff($mjson->{graph}->{db1Histo}, $json->{graph}->{db1Histo}, "multi db1Histo field: node");
    eq_or_diff($mjson->{graph}->{db2Histo}, $json->{graph}->{db2Histo}, "multi db2Histo field: node");
    eq_or_diff($mjson->{items}, $json->{items}, "multi items field: node");
    cmp_ok ($mjson->{recordsTotal}, '>=', 194, "recordsTotal");
    cmp_ok ($mjson->{recordsFiltered}, '==', 6);



#tags
    $json = viewerGet("/spigraph.json?date=-1&field=tags&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($json->{map}, from_json('{"dst":{"US": 3, "CA": 1}, "src":{"US": 3, "RU":1}}'), "map field: tags");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1], [1482552000000, 1]]'), "lpHisto field: tags");
    eq_or_diff($json->{graph}->{pa1Histo}, from_json('[["1335956400000", 2], ["1386003600000", 26], [1387742400000, 3], [1482552000000, 3]]'), "pa1Histo field: tags");
    eq_or_diff($json->{graph}->{pa2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 20], [1387742400000, 1], [1482552000000, 1]]'), "pa2Histo field: tags");
    eq_or_diff($json->{graph}->{db1Histo}, from_json('[["1335956400000", 0], ["1386003600000", 486], [1387742400000, 68], [1482552000000, 68]]'), "db1Histo field: tags");
    eq_or_diff($json->{graph}->{db2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 4801], [1387742400000, 0], [1482552000000, 0]]'), "db2Histo field: tags");
    cmp_ok ($json->{recordsTotal}, '>=', 194);
    cmp_ok ($json->{recordsFiltered}, '==', 6);

    my @items = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
    eq_or_diff(\@items, from_json('[{"count":3,"map":{"dst":{"US":3},"src":{"US":3}},"lpHisto":3,"paHisto":46,"name":"byhost2","dbHisto":5287,"graph":{"db2Histo":[[1386003600000,4801]],"xmin":1335956400000,"pa2Histo":[[1386003600000,20]],"lpHisto":[[1386003600000,3]],"xmax":1482552000000,"db1Histo":[[1386003600000,486]],"interval":3600,"pa1Histo":[[1386003600000,26]]}},{"paHisto":2,"dbHisto":0,"name":"byip2","graph":{"pa1Histo":[[1335956400000,2]],"interval":3600,"db1Histo":[[1335956400000,0]],"xmax":1482552000000,"lpHisto":[[1335956400000,1]],"pa2Histo":[[1335956400000,0]],"db2Histo":[[1335956400000,0]],"xmin":1335956400000},"count":1,"map":{"src":{},"dst":{}},"lpHisto":1},{"name":"domainwise","paHisto":46,"dbHisto":5287,"graph":{"xmin":1335956400000,"db2Histo":[[1386003600000,4801]],"pa2Histo":[[1386003600000,20]],"lpHisto":[[1386003600000,3]],"xmax":1482552000000,"db1Histo":[[1386003600000,486]],"interval":3600,"pa1Histo":[[1386003600000,26]]},"count":3,"map":{"src":{"US":3},"dst":{"US":3}},"lpHisto":3},{"dbHisto":68,"paHisto":4,"name":"dstip","graph":{"pa2Histo":[[1387742400000,1]],"xmin":1335956400000,"db2Histo":[[1387742400000,0]],"lpHisto":[[1387742400000,1]],"xmax":1482552000000,"pa1Histo":[[1387742400000,3]],"interval":3600,"db1Histo":[[1387742400000,68]]},"lpHisto":1,"map":{"src":{"RU":1},"dst":{"CA":1}},"count":1},{"dbHisto":5287,"paHisto":46,"name":"hosttaggertest1","graph":{"interval":3600,"pa1Histo":[[1386003600000,26]],"db1Histo":[[1386003600000,486]],"xmax":1482552000000,"lpHisto":[[1386003600000,3]],"pa2Histo":[[1386003600000,20]],"db2Histo":[[1386003600000,4801]],"xmin":1335956400000},"lpHisto":3,"map":{"dst":{"US":3},"src":{"US":3}},"count":3},{"lpHisto":3,"count":3,"map":{"dst":{"US":3},"src":{"US":3}},"paHisto":46,"dbHisto":5287,"graph":{"xmax":1482552000000,"pa1Histo":[[1386003600000,26]],"interval":3600,"db1Histo":[[1386003600000,486]],"pa2Histo":[[1386003600000,20]],"xmin":1335956400000,"db2Histo":[[1386003600000,4801]],"lpHisto":[[1386003600000,3]]},"name":"hosttaggertest2"},{"paHisto":2,"dbHisto":0,"name":"iptaggertest1","graph":{"pa2Histo":[[1335956400000,0]],"db2Histo":[[1335956400000,0]],"xmin":1335956400000,"lpHisto":[[1335956400000,1]],"xmax":1482552000000,"interval":3600,"pa1Histo":[[1335956400000,2]],"db1Histo":[[1335956400000,0]]},"lpHisto":1,"count":1,"map":{"src":{},"dst":{}}},{"lpHisto":1,"map":{"src":{},"dst":{}},"count":1,"paHisto":2,"graph":{"db1Histo":[[1335956400000,0]],"interval":3600,"pa1Histo":[[1335956400000,2]],"xmax":1482552000000,"lpHisto":[[1335956400000,1]],"db2Histo":[[1335956400000,0]],"xmin":1335956400000,"pa2Histo":[[1335956400000,0]]},"dbHisto":0,"name":"iptaggertest2"},{"map":{"dst":{},"src":{}},"count":1,"lpHisto":1,"paHisto":2,"graph":{"interval":3600,"pa1Histo":[[1335956400000,2]],"db1Histo":[[1335956400000,0]],"xmax":1482552000000,"lpHisto":[[1335956400000,1]],"pa2Histo":[[1335956400000,0]],"xmin":1335956400000,"db2Histo":[[1335956400000,0]]},"dbHisto":0,"name":"ipwise"},{"dbHisto":68,"paHisto":4,"name":"ipwisecsv","graph":{"lpHisto":[[1387742400000,1]],"pa2Histo":[[1387742400000,1]],"db2Histo":[[1387742400000,0]],"xmin":1335956400000,"interval":3600,"pa1Histo":[[1387742400000,3]],"db1Histo":[[1387742400000,68]],"xmax":1482552000000},"count":1,"map":{"src":{"RU":1},"dst":{"CA":1}},"lpHisto":1},{"count":1,"map":{"src":{"RU":1},"dst":{"CA":1}},"lpHisto":1,"paHisto":4,"name":"srcip","dbHisto":68,"graph":{"lpHisto":[[1387742400000,1]],"pa2Histo":[[1387742400000,1]],"xmin":1335956400000,"db2Histo":[[1387742400000,0]],"interval":3600,"pa1Histo":[[1387742400000,3]],"db1Histo":[[1387742400000,68]],"xmax":1482552000000}},{"lpHisto":3,"count":3,"map":{"src":{"US":3},"dst":{"US":3}},"paHisto":46,"dbHisto":5287,"graph":{"lpHisto":[[1386003600000,3]],"xmin":1335956400000,"db2Histo":[[1386003600000,4801]],"pa2Histo":[[1386003600000,20]],"db1Histo":[[1386003600000,486]],"interval":3600,"pa1Histo":[[1386003600000,26]],"xmax":1482552000000},"name":"wisebyhost2"},{"paHisto":2,"name":"wisebyip2","dbHisto":0,"graph":{"lpHisto":[[1335956400000,1]],"pa2Histo":[[1335956400000,0]],"xmin":1335956400000,"db2Histo":[[1335956400000,0]],"pa1Histo":[[1335956400000,2]],"interval":3600,"db1Histo":[[1335956400000,0]],"xmax":1482552000000},"count":1,"map":{"dst":{},"src":{}},"lpHisto":1}]'), "items field: tags", { context => 3 });

#tags multi
    $mjson = multiGet("/spigraph.json?date=-1&field=tags&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($mjson->{map}, $json->{map}, "multi map field: tags");
    eq_or_diff($mjson->{graph}->{lpHisto}, $json->{graph}->{lpHisto}, "multi lpHisto field: tags");
    eq_or_diff($mjson->{graph}->{pa1Histo}, $json->{graph}->{pa1Histo}, "multi pa1Histo field: tags");
    eq_or_diff($mjson->{graph}->{pa2Histo}, $json->{graph}->{pa2Histo}, "multi pa2Histo field: tags");
    eq_or_diff($mjson->{graph}->{db1Histo}, $json->{graph}->{db1Histo}, "multi db1Histo field: tags");
    eq_or_diff($mjson->{graph}->{db2Histo}, $json->{graph}->{db2Histo}, "multi db2Histo field: tags");

    my @mitems = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
    eq_or_diff(\@items, \@items, "multi items field: tags");


#srcIp
    $json = viewerGet("/spigraph.json?date=-1&field=srcIp&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($json->{map}, from_json('{"dst":{"US": 3, "CA": 1}, "src":{"US": 3, "RU":1}}'), "map field: srcIp");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1], [1482552000000, 1]]'), "lpHisto field: srcIp");
    eq_or_diff($json->{graph}->{pa1Histo}, from_json('[["1335956400000", 2], ["1386003600000", 26], [1387742400000, 3], [1482552000000, 3]]'), "pa1Histo field: srcIp");
    eq_or_diff($json->{graph}->{pa2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 20], [1387742400000, 1], [1482552000000, 1]]'), "pa2Histo field: srcIp");
    eq_or_diff($json->{graph}->{db1Histo}, from_json('[["1335956400000", 0], ["1386003600000", 486], [1387742400000, 68], [1482552000000, 68]]'), "db1Histo field: srcIp");
    eq_or_diff($json->{graph}->{db2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 4801], [1387742400000, 0], [1482552000000, 0]]'), "db2Histo field: srcIp");
    my @items = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
    eq_or_diff(\@items, from_json('[{"paHisto":4,"name":"10.0.0.1","count":1,"lpHisto":1,"map":{"src":{"RU":1},"dst":{"CA":1}},"dbHisto":68,"graph":{"lpHisto":[[1387742400000,1]],"xmin":1335956400000,"db2Histo":[[1387742400000,0]],"db1Histo":[[1387742400000,68]],"pa2Histo":[[1387742400000,1]],"xmax":1482552000000,"pa1Histo":[[1387742400000,3]],"interval":3600}},{"name":"10.10.10.10","paHisto":4,"count":1,"lpHisto":1,"map":{"dst":{},"src":{}},"graph":{"interval":3600,"pa1Histo":[[1482552000000,3]],"xmax":1482552000000,"pa2Histo":[[1482552000000,1]],"db2Histo":[[1482552000000,0]],"db1Histo":[[1482552000000,68]],"xmin":1335956400000,"lpHisto":[[1482552000000,1]]},"dbHisto":68},{"graph":{"db2Histo":[[1386003600000,4801]],"db1Histo":[[1386003600000,486]],"lpHisto":[[1386003600000,3]],"xmin":1335956400000,"pa1Histo":[[1386003600000,26]],"interval":3600,"xmax":1482552000000,"pa2Histo":[[1386003600000,20]]},"dbHisto":5287,"lpHisto":3,"count":3,"map":{"dst":{"US":3},"src":{"US":3}},"name":"10.180.156.185","paHisto":46},{"graph":{"xmax":1482552000000,"pa2Histo":[[1335956400000,0]],"pa1Histo":[[1335956400000,2]],"interval":3600,"lpHisto":[[1335956400000,1]],"xmin":1335956400000,"db1Histo":[[1335956400000,0]],"db2Histo":[[1335956400000,0]]},"dbHisto":0,"map":{"dst":{},"src":{}},"lpHisto":1,"count":1,"name":"192.168.177.160","paHisto":2}]'), "items field: srcIp", { context => 3 });
    cmp_ok ($json->{recordsTotal}, '>=', 194);
    cmp_ok ($json->{recordsFiltered}, '==', 6);

#srcIp multi
    $mjson = multiGet("/spigraph.json?date=-1&field=srcIp&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($mjson->{map}, $json->{map}, "multi map field: srcIp");
    eq_or_diff($mjson->{graph}->{lpHisto}, $json->{graph}->{lpHisto}, "multi lpHisto field: srcIp");
    eq_or_diff($mjson->{graph}->{pa1Histo}, $json->{graph}->{pa1Histo}, "multi pa1Histo field: srcIp");
    eq_or_diff($mjson->{graph}->{pa2Histo}, $json->{graph}->{pa2Histo}, "multi pa2Histo field: srcIp");
    eq_or_diff($mjson->{graph}->{db1Histo}, $json->{graph}->{db1Histo}, "multi db1Histo field: srcIp");
    eq_or_diff($mjson->{graph}->{db2Histo}, $json->{graph}->{db2Histo}, "multi db2Histo field: srcIp");
    eq_or_diff($mjson->{items}, $json->{items}, "multi items field: srcIp");

    my @mitems = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
    eq_or_diff(\@items, \@items, "multi items field: srcIp");

    SKIP: {
        skip "Upgrade test", 15 if ($ENV{MOLOCH_REINDEX_TEST}); # reindex doesn't have requestHeader

#http.requestHeader
        $json = viewerGet("/spigraph.json?date=-1&field=http.requestHeader&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
        eq_or_diff($json->{map}, from_json('{"dst":{"US": 3, "CA": 1}, "src":{"US": 3, "RU":1}}'), "map field: http.requestHeader");
        eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1], [1482552000000, 1]]'), "lpHisto field: h1");
        eq_or_diff($json->{graph}->{pa1Histo}, from_json('[["1335956400000", 2], ["1386003600000", 26], [1387742400000, 3], [1482552000000, 3]]'), "pa1Histo field: h1");
        eq_or_diff($json->{graph}->{pa2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 20], [1387742400000, 1], [1482552000000, 1]]'), "pa2Histo field: h1");
        eq_or_diff($json->{graph}->{db1Histo}, from_json('[["1335956400000", 0], ["1386003600000", 486], [1387742400000, 68], [1482552000000, 68]]'), "db1Histo field: h1");
        eq_or_diff($json->{graph}->{db2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 4801], [1387742400000, 0], [1482552000000, 0]]'), "db2Histo field: h1");
        my @items = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
        eq_or_diff(\@items, from_json('[{"name":"accept","count":3,"dbHisto":5287,"map":{"dst":{"US":3},"src":{"US":3}},"graph":{"pa2Histo":[[1386003600000,20]],"pa1Histo":[[1386003600000,26]],"interval":3600,"lpHisto":[[1386003600000,3]],"xmax":1482552000000,"db1Histo":[[1386003600000,486]],"xmin":1335956400000,"db2Histo":[[1386003600000,4801]]},"paHisto":46,"lpHisto":3},{"lpHisto":3,"name":"host","count":3,"map":{"dst":{"US":3},"src":{"US":3}},"dbHisto":5287,"graph":{"db2Histo":[[1386003600000,4801]],"lpHisto":[[1386003600000,3]],"xmax":1482552000000,"db1Histo":[[1386003600000,486]],"xmin":1335956400000,"pa2Histo":[[1386003600000,20]],"pa1Histo":[[1386003600000,26]],"interval":3600},"paHisto":46},{"lpHisto":3,"name":"user-agent","count":3,"dbHisto":5287,"graph":{"interval":3600,"pa2Histo":[[1386003600000,20]],"pa1Histo":[[1386003600000,26]],"db1Histo":[[1386003600000,486]],"xmin":1335956400000,"lpHisto":[[1386003600000,3]],"xmax":1482552000000,"db2Histo":[[1386003600000,4801]]},"map":{"src":{"US":3},"dst":{"US":3}},"paHisto":46}]'), "items field: http.requestHeader", { context => 3 });
    cmp_ok ($json->{recordsTotal}, '>=', 194);
    cmp_ok ($json->{recordsFiltered}, '==', 6);

#http.requestHeader multi
        $mjson = multiGet("/spigraph.json?date=-1&field=http.requestHeader&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
        eq_or_diff($mjson->{map}, $json->{map}, "multi map field: http.requestHeader");
        eq_or_diff($mjson->{graph}->{lpHisto}, $json->{graph}->{lpHisto}, "multi lpHisto field: http.requestHeader");
        eq_or_diff($mjson->{graph}->{pa1Histo}, $json->{graph}->{pa1Histo}, "multi pa1Histo field: http.requestHeader");
        eq_or_diff($mjson->{graph}->{pa2Histo}, $json->{graph}->{pa2Histo}, "multi pa2Histo field: http.requestHeader");
        eq_or_diff($mjson->{graph}->{db1Histo}, $json->{graph}->{db1Histo}, "multi db1Histo field: http.requestHeader");
        eq_or_diff($mjson->{graph}->{db2Histo}, $json->{graph}->{db2Histo}, "multi db2Histo field: http.requestHeader");
        eq_or_diff($mjson->{items}, $json->{items}, "multi items field: http.requestHeader");

        my @mitems = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
        eq_or_diff(\@items, \@items, "multi items field: http.requestHeader");
    }


#http.useragent
    $json = viewerGet("/spigraph.json?date=-1&field=http.useragent&expression=" . uri_escape("file=$pwd/socks5-reverse.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    my @items = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
    eq_or_diff(\@items, from_json('[{"map":{"dst":{"CA":1},"src":{"RU":1}},"paHisto":52,"name":"Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322)","dbHisto":24346,"graph":{"xmax":1482552000000,"pa2Histo":[[1386788400000,21]],"pa1Histo":[[1386788400000,31]],"db2Histo":[[1386788400000,954]],"interval":3600,"lpHisto":[[1386788400000,1]],"xmin":1386003600000,"db1Histo":[[1386788400000,23392]]},"count":1,"lpHisto":1},{"lpHisto":3,"count":3,"graph":{"db2Histo":[[1386003600000,4801]],"interval":3600,"lpHisto":[[1386003600000,3]],"pa1Histo":[[1386003600000,26]],"xmax":1482552000000,"pa2Histo":[[1386003600000,20]],"xmin":1386003600000,"db1Histo":[[1386003600000,486]]},"dbHisto":5287,"name":"curl/7.24.0 (x86_64-apple-darwin12.0) libcurl/7.24.0 OpenSSL/0.9.8y zlib/1.2.5","map":{"src":{"US":3},"dst":{"US":3}},"paHisto":46}]'), "items field: http.useragent", { context => 3 });
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1386003600000", 3], ["1386788400000", 1], [1387742400000, 1], [1482552000000, 1]]'), "multi lpHisto field: http.useragent");
    eq_or_diff($json->{graph}->{pa1Histo}, from_json('[["1386003600000", 26], ["1386788400000", 31], [1387742400000, 3], [1482552000000, 3]]'), "multi pa1Histo field: http.useragent");
    eq_or_diff($json->{graph}->{pa2Histo}, from_json('[["1386003600000", 20], ["1386788400000", 21], [1387742400000, 1], [1482552000000, 1]]'), "multi pa2Histo field: http.useragent");
    eq_or_diff($json->{graph}->{db1Histo}, from_json('[["1386003600000", 486], ["1386788400000", 23392], [1387742400000, 68], [1482552000000, 68]]'), "multi db1Histo field: http.useragent");
    eq_or_diff($json->{graph}->{db2Histo}, from_json('[["1386003600000", 4801], ["1386788400000", 954], [1387742400000, 0], [1482552000000, 0]]'), "multi db2Histo field: http.useragent");
    cmp_ok ($json->{recordsTotal}, '>=', 194);
    cmp_ok ($json->{recordsFiltered}, '==', 6);

#http.useragent multi
    $mjson = multiGet("/spigraph.json?date=-1&field=http.useragent&expression=" . uri_escape("file=$pwd/socks5-reverse.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    my @mitems = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
    eq_or_diff(\@items, \@items, "multi items field: http.useragent");
