use Test::More tests => 78;
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
    $json = viewerGet("/spigraph.json?map=true&date=-1&field=node&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($json->{map}, from_json('{"dst":{"US": 3, "CA": 1}, "src":{"US": 3, "RU":1}, "xffGeo":{}}'), "map field: no");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1], [1482552000000, 1]]'), "lpHisto field: node");
    eq_or_diff($json->{graph}->{pa1Histo}, from_json('[["1335956400000", 2], ["1386003600000", 26], [1387742400000, 3], [1482552000000, 3]]'), "pa1Histo field: node");
    eq_or_diff($json->{graph}->{pa2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 20], [1387742400000, 1], [1482552000000, 1]]'), "pa2Histo field: node");
    eq_or_diff($json->{graph}->{db1Histo}, from_json('[["1335956400000", 128], ["1386003600000", 486], [1387742400000, 68], [1482552000000, 68]]'), "db1Histo field: node");
    eq_or_diff($json->{graph}->{db2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 4801], [1387742400000, 0], [1482552000000, 0]]'), "db2Histo field: node");
    eq_or_diff($json->{items}, from_json('[{"dbHisto":5551,"name":"test","byHisto":9261,"paHisto":56,"count":6,"map":{"xffGeo":{},"dst":{"CA":1,"US":3},"src":{"RU":1,"US":3}},"graph":{"by2Histo":[[1335956400000,0],[1386003600000,6145],[1387742400000,66],[1482552000000,82]],"pa1Histo":[[1335956400000,2],[1386003600000,26],[1387742400000,3],[1482552000000,3]],"xmax":1482552000000,"xmin":1335956400000,"pa2Histo":[[1335956400000,0],[1386003600000,20],[1387742400000,1],[1482552000000,1]],"db1Histo":[[1335956400000,128],[1386003600000,486],[1387742400000,68],[1482552000000,68]],"by1Histo":[[1335956400000,196],[1386003600000,2238],[1387742400000,248],[1482552000000,286]],"db2Histo":[[1335956400000,0],[1386003600000,4801],[1387742400000,0],[1482552000000,0]],"interval":3600,"lpHisto":[[1335956400000,1],[1386003600000,3],[1387742400000,1],[1482552000000,1]]},"lpHisto":6}]'), "items field: node", { context => 3 });
    cmp_ok ($json->{recordsTotal}, '>=', 194);
    cmp_ok ($json->{recordsFiltered}, '==', 6);

#node multi
    $mjson = multiGet("/spigraph.json?map=true&date=-1&field=node&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
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
    $json = viewerGet("/spigraph.json?map=true&date=-1&field=tags&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($json->{map}, from_json('{"dst":{"US": 3, "CA": 1}, "src":{"US": 3, "RU":1}, "xffGeo":{}}'), "map field: tags");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1], [1482552000000, 1]]'), "lpHisto field: tags");
    eq_or_diff($json->{graph}->{pa1Histo}, from_json('[["1335956400000", 2], ["1386003600000", 26], [1387742400000, 3], [1482552000000, 3]]'), "pa1Histo field: tags");
    eq_or_diff($json->{graph}->{pa2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 20], [1387742400000, 1], [1482552000000, 1]]'), "pa2Histo field: tags");
    eq_or_diff($json->{graph}->{db1Histo}, from_json('[["1335956400000", 128], ["1386003600000", 486], [1387742400000, 68], [1482552000000, 68]]'), "db1Histo field: tags");
    eq_or_diff($json->{graph}->{db2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 4801], [1387742400000, 0], [1482552000000, 0]]'), "db2Histo field: tags");
    cmp_ok ($json->{recordsTotal}, '>=', 194);
    cmp_ok ($json->{recordsFiltered}, '==', 6);

    my @items = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
    eq_or_diff(\@items, from_json('[{"dbHisto":5287,"name":"byhost2","map":{"xffGeo":{},"src":{"US":3},"dst":{"US":3}},"byHisto":8383,"paHisto":46,"count":3,"lpHisto":3,"graph":{"by2Histo":[[1386003600000,6145]],"by1Histo":[[1386003600000,2238]],"xmin":1335956400000,"xmax":1482552000000,"pa2Histo":[[1386003600000,20]],"db1Histo":[[1386003600000,486]],"pa1Histo":[[1386003600000,26]],"interval":3600,"db2Histo":[[1386003600000,4801]],"lpHisto":[[1386003600000,3]]}},{"map":{"src":{},"dst":{},"xffGeo":{}},"byHisto":196,"paHisto":2,"count":1,"lpHisto":1,"graph":{"by1Histo":[[1335956400000,196]],"xmax":1482552000000,"db1Histo":[[1335956400000,128]],"xmin":1335956400000,"pa2Histo":[[1335956400000,0]],"pa1Histo":[[1335956400000,2]],"by2Histo":[[1335956400000,0]],"db2Histo":[[1335956400000,0]],"interval":3600,"lpHisto":[[1335956400000,1]]},"dbHisto":128,"name":"byip2"},{"name":"domainwise","dbHisto":5287,"count":3,"paHisto":46,"byHisto":8383,"map":{"dst":{"US":3},"src":{"US":3},"xffGeo":{}},"graph":{"interval":3600,"lpHisto":[[1386003600000,3]],"db2Histo":[[1386003600000,4801]],"by2Histo":[[1386003600000,6145]],"by1Histo":[[1386003600000,2238]],"db1Histo":[[1386003600000,486]],"xmax":1482552000000,"xmin":1335956400000,"pa2Histo":[[1386003600000,20]],"pa1Histo":[[1386003600000,26]]},"lpHisto":3},{"graph":{"db2Histo":[[1387742400000,0]],"interval":3600,"lpHisto":[[1387742400000,1]],"by1Histo":[[1387742400000,248]],"pa2Histo":[[1387742400000,1]],"xmax":1482552000000,"xmin":1335956400000,"db1Histo":[[1387742400000,68]],"pa1Histo":[[1387742400000,3]],"by2Histo":[[1387742400000,66]]},"lpHisto":1,"paHisto":4,"count":1,"byHisto":314,"map":{"dst":{"CA":1},"src":{"RU":1},"xffGeo":{}},"dbHisto":68,"name":"dstip"},{"dbHisto":5287,"name":"hosttaggertest1","map":{"dst":{"US":3},"src":{"US":3},"xffGeo":{}},"count":3,"paHisto":46,"byHisto":8383,"lpHisto":3,"graph":{"by2Histo":[[1386003600000,6145]],"by1Histo":[[1386003600000,2238]],"pa1Histo":[[1386003600000,26]],"xmax":1482552000000,"pa2Histo":[[1386003600000,20]],"xmin":1335956400000,"db1Histo":[[1386003600000,486]],"interval":3600,"db2Histo":[[1386003600000,4801]],"lpHisto":[[1386003600000,3]]}},{"graph":{"lpHisto":[[1386003600000,3]],"interval":3600,"db2Histo":[[1386003600000,4801]],"by1Histo":[[1386003600000,2238]],"xmin":1335956400000,"xmax":1482552000000,"db1Histo":[[1386003600000,486]],"pa2Histo":[[1386003600000,20]],"pa1Histo":[[1386003600000,26]],"by2Histo":[[1386003600000,6145]]},"lpHisto":3,"paHisto":46,"count":3,"byHisto":8383,"map":{"xffGeo":{},"dst":{"US":3},"src":{"US":3}},"dbHisto":5287,"name":"hosttaggertest2"},{"name":"iptaggertest1","dbHisto":128,"map":{"src":{},"dst":{},"xffGeo":{}},"paHisto":2,"count":1,"byHisto":196,"lpHisto":1,"graph":{"by1Histo":[[1335956400000,196]],"xmin":1335956400000,"xmax":1482552000000,"pa2Histo":[[1335956400000,0]],"db1Histo":[[1335956400000,128]],"pa1Histo":[[1335956400000,2]],"by2Histo":[[1335956400000,0]],"db2Histo":[[1335956400000,0]],"interval":3600,"lpHisto":[[1335956400000,1]]}},{"count":1,"paHisto":2,"byHisto":196,"map":{"xffGeo":{},"src":{},"dst":{}},"graph":{"by1Histo":[[1335956400000,196]],"pa1Histo":[[1335956400000,2]],"db1Histo":[[1335956400000,128]],"xmax":1482552000000,"xmin":1335956400000,"pa2Histo":[[1335956400000,0]],"by2Histo":[[1335956400000,0]],"db2Histo":[[1335956400000,0]],"interval":3600,"lpHisto":[[1335956400000,1]]},"lpHisto":1,"name":"iptaggertest2","dbHisto":128},{"dbHisto":128,"name":"ipwise","paHisto":2,"count":1,"byHisto":196,"map":{"xffGeo":{},"src":{},"dst":{}},"graph":{"by1Histo":[[1335956400000,196]],"xmax":1482552000000,"pa2Histo":[[1335956400000,0]],"xmin":1335956400000,"db1Histo":[[1335956400000,128]],"pa1Histo":[[1335956400000,2]],"by2Histo":[[1335956400000,0]],"db2Histo":[[1335956400000,0]],"interval":3600,"lpHisto":[[1335956400000,1]]},"lpHisto":1},{"dbHisto":68,"name":"ipwisecsv","graph":{"by2Histo":[[1387742400000,66]],"pa1Histo":[[1387742400000,3]],"xmin":1335956400000,"xmax":1482552000000,"pa2Histo":[[1387742400000,1]],"db1Histo":[[1387742400000,68]],"by1Histo":[[1387742400000,248]],"db2Histo":[[1387742400000,0]],"interval":3600,"lpHisto":[[1387742400000,1]]},"lpHisto":1,"paHisto":4,"count":1,"byHisto":314,"map":{"xffGeo":{},"src":{"RU":1},"dst":{"CA":1}}},{"lpHisto":1,"graph":{"db2Histo":[[1387742400000,0]],"interval":3600,"lpHisto":[[1387742400000,1]],"xmin":1335956400000,"xmax":1482552000000,"pa2Histo":[[1387742400000,1]],"db1Histo":[[1387742400000,68]],"pa1Histo":[[1387742400000,3]],"by1Histo":[[1387742400000,248]],"by2Histo":[[1387742400000,66]]},"map":{"xffGeo":{},"dst":{"CA":1},"src":{"RU":1}},"count":1,"paHisto":4,"byHisto":314,"dbHisto":68,"name":"srcip"},{"map":{"xffGeo":{},"src":{"US":3},"dst":{"US":3}},"paHisto":46,"count":3,"byHisto":8383,"lpHisto":3,"graph":{"by2Histo":[[1386003600000,6145]],"pa1Histo":[[1386003600000,26]],"xmax":1482552000000,"pa2Histo":[[1386003600000,20]],"xmin":1335956400000,"db1Histo":[[1386003600000,486]],"by1Histo":[[1386003600000,2238]],"lpHisto":[[1386003600000,3]],"interval":3600,"db2Histo":[[1386003600000,4801]]},"dbHisto":5287,"name":"wisebyhost2"},{"lpHisto":1,"graph":{"interval":3600,"lpHisto":[[1335956400000,1]],"db2Histo":[[1335956400000,0]],"by1Histo":[[1335956400000,196]],"xmax":1482552000000,"db1Histo":[[1335956400000,128]],"xmin":1335956400000,"pa2Histo":[[1335956400000,0]],"pa1Histo":[[1335956400000,2]],"by2Histo":[[1335956400000,0]]},"map":{"src":{},"dst":{},"xffGeo":{}},"byHisto":196,"count":1,"paHisto":2,"name":"wisebyip2","dbHisto":128}]'), "items field: tags", { context => 3 });

#tags multi
    $mjson = multiGet("/spigraph.json?map=true&date=-1&field=tags&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($mjson->{map}, $json->{map}, "multi map field: tags");
    eq_or_diff($mjson->{graph}->{lpHisto}, $json->{graph}->{lpHisto}, "multi lpHisto field: tags");
    eq_or_diff($mjson->{graph}->{pa1Histo}, $json->{graph}->{pa1Histo}, "multi pa1Histo field: tags");
    eq_or_diff($mjson->{graph}->{pa2Histo}, $json->{graph}->{pa2Histo}, "multi pa2Histo field: tags");
    eq_or_diff($mjson->{graph}->{db1Histo}, $json->{graph}->{db1Histo}, "multi db1Histo field: tags");
    eq_or_diff($mjson->{graph}->{db2Histo}, $json->{graph}->{db2Histo}, "multi db2Histo field: tags");

    my @mitems = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
    eq_or_diff(\@items, \@items, "multi items field: tags");


#srcIp
    $json = viewerGet("/spigraph.json?map=true&date=-1&field=srcIp&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($json->{map}, from_json('{"dst":{"US": 3, "CA": 1}, "src":{"US": 3, "RU":1}, "xffGeo":{}}'), "map field: srcIp");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1], [1482552000000, 1]]'), "lpHisto field: srcIp");
    eq_or_diff($json->{graph}->{pa1Histo}, from_json('[["1335956400000", 2], ["1386003600000", 26], [1387742400000, 3], [1482552000000, 3]]'), "pa1Histo field: srcIp");
    eq_or_diff($json->{graph}->{pa2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 20], [1387742400000, 1], [1482552000000, 1]]'), "pa2Histo field: srcIp");
    eq_or_diff($json->{graph}->{db1Histo}, from_json('[["1335956400000", 128], ["1386003600000", 486], [1387742400000, 68], [1482552000000, 68]]'), "db1Histo field: srcIp");
    eq_or_diff($json->{graph}->{db2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 4801], [1387742400000, 0], [1482552000000, 0]]'), "db2Histo field: srcIp");
    my @items = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
    eq_or_diff(\@items, from_json('[{"graph":{"by2Histo":[[1387742400000,66]],"xmin":1335956400000,"db2Histo":[[1387742400000,0]],"interval":3600,"db1Histo":[[1387742400000,68]],"xmax":1482552000000,"pa2Histo":[[1387742400000,1]],"lpHisto":[[1387742400000,1]],"pa1Histo":[[1387742400000,3]],"by1Histo":[[1387742400000,248]]},"dbHisto":68,"name":"10.0.0.1","paHisto":4,"byHisto":314,"count":1,"map":{"dst":{"CA":1},"xffGeo":{},"src":{"RU":1}},"lpHisto":1},{"graph":{"by1Histo":[[1482552000000,286]],"lpHisto":[[1482552000000,1]],"pa1Histo":[[1482552000000,3]],"pa2Histo":[[1482552000000,1]],"xmax":1482552000000,"db1Histo":[[1482552000000,68]],"interval":3600,"xmin":1335956400000,"db2Histo":[[1482552000000,0]],"by2Histo":[[1482552000000,82]]},"count":1,"byHisto":368,"paHisto":4,"dbHisto":68,"name":"10.10.10.10","lpHisto":1,"map":{"xffGeo":{},"src":{},"dst":{}}},{"lpHisto":3,"map":{"dst":{"US":3},"src":{"US":3},"xffGeo":{}},"graph":{"interval":3600,"db2Histo":[[1386003600000,4801]],"xmin":1335956400000,"by2Histo":[[1386003600000,6145]],"xmax":1482552000000,"pa2Histo":[[1386003600000,20]],"pa1Histo":[[1386003600000,26]],"lpHisto":[[1386003600000,3]],"by1Histo":[[1386003600000,2238]],"db1Histo":[[1386003600000,486]]},"count":3,"paHisto":46,"byHisto":8383,"dbHisto":5287,"name":"10.180.156.185"},{"graph":{"interval":3600,"by2Histo":[[1335956400000,0]],"xmin":1335956400000,"db2Histo":[[1335956400000,0]],"pa1Histo":[[1335956400000,2]],"by1Histo":[[1335956400000,196]],"lpHisto":[[1335956400000,1]],"pa2Histo":[[1335956400000,0]],"xmax":1482552000000,"db1Histo":[[1335956400000,128]]},"count":1,"name":"192.168.177.160","dbHisto":128,"paHisto":2,"byHisto":196,"lpHisto":1,"map":{"src":{},"xffGeo":{},"dst":{}}}]'), "items field: srcIp", { context => 3 });
    cmp_ok ($json->{recordsTotal}, '>=', 194);
    cmp_ok ($json->{recordsFiltered}, '==', 6);

#srcIp multi
    $mjson = multiGet("/spigraph.json?map=true&date=-1&field=srcIp&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
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
        $json = viewerGet("/spigraph.json?map=true&date=-1&field=http.requestHeader&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
        eq_or_diff($json->{map}, from_json('{"dst":{"US": 3, "CA": 1}, "src":{"US": 3, "RU":1}, "xffGeo":{}}'), "map field: http.requestHeader");
        eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1], [1482552000000, 1]]'), "lpHisto field: h1");
        eq_or_diff($json->{graph}->{pa1Histo}, from_json('[["1335956400000", 2], ["1386003600000", 26], [1387742400000, 3], [1482552000000, 3]]'), "pa1Histo field: h1");
        eq_or_diff($json->{graph}->{pa2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 20], [1387742400000, 1], [1482552000000, 1]]'), "pa2Histo field: h1");
        eq_or_diff($json->{graph}->{db1Histo}, from_json('[["1335956400000", 128], ["1386003600000", 486], [1387742400000, 68], [1482552000000, 68]]'), "db1Histo field: h1");
        eq_or_diff($json->{graph}->{db2Histo}, from_json('[["1335956400000", 0], ["1386003600000", 4801], [1387742400000, 0], [1482552000000, 0]]'), "db2Histo field: h1");
        my @items = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
        eq_or_diff(\@items, from_json('[{"graph":{"by2Histo":[[1386003600000,6145]],"xmin":1335956400000,"db2Histo":[[1386003600000,4801]],"interval":3600,"db1Histo":[[1386003600000,486]],"xmax":1482552000000,"pa2Histo":[[1386003600000,20]],"lpHisto":[[1386003600000,3]],"pa1Histo":[[1386003600000,26]],"by1Histo":[[1386003600000,2238]]},"name":"accept","dbHisto":5287,"paHisto":46,"byHisto":8383,"count":3,"map":{"dst":{"US":3},"src":{"US":3},"xffGeo":{}},"lpHisto":3},{"lpHisto":3,"map":{"dst":{"US":3},"src":{"US":3},"xffGeo":{}},"graph":{"by2Histo":[[1386003600000,6145]],"db2Histo":[[1386003600000,4801]],"xmin":1335956400000,"interval":3600,"db1Histo":[[1386003600000,486]],"by1Histo":[[1386003600000,2238]],"lpHisto":[[1386003600000,3]],"pa1Histo":[[1386003600000,26]],"pa2Histo":[[1386003600000,20]],"xmax":1482552000000},"count":3,"name":"host","dbHisto":5287,"byHisto":8383,"paHisto":46},{"paHisto":46,"byHisto":8383,"name":"user-agent","dbHisto":5287,"count":3,"graph":{"db1Histo":[[1386003600000,486]],"xmax":1482552000000,"pa2Histo":[[1386003600000,20]],"lpHisto":[[1386003600000,3]],"pa1Histo":[[1386003600000,26]],"by1Histo":[[1386003600000,2238]],"xmin":1335956400000,"db2Histo":[[1386003600000,4801]],"by2Histo":[[1386003600000,6145]],"interval":3600},"map":{"src":{"US":3},"xffGeo":{},"dst":{"US":3}},"lpHisto":3}]'), "items field: http.requestHeader", { context => 3 });
    cmp_ok ($json->{recordsTotal}, '>=', 194);
    cmp_ok ($json->{recordsFiltered}, '==', 6);

#http.requestHeader multi
        $mjson = multiGet("/spigraph.json?map=true&date=-1&field=http.requestHeader&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
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
    $json = viewerGet("/spigraph.json?map=true&date=-1&field=http.useragent&expression=" . uri_escape("file=$pwd/socks5-reverse.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    my @items = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
    eq_or_diff(\@items, from_json('[{"byHisto":27311,"dbHisto":24346,"count":1,"graph":{"db1Histo":[[1386788400000,23392]],"by1Histo":[[1386788400000,25112]],"pa1Histo":[[1386788400000,31]],"interval":3600,"lpHisto":[[1386788400000,1]],"xmin":1386003600000,"db2Histo":[[1386788400000,954]],"pa2Histo":[[1386788400000,21]],"xmax":1482552000000,"by2Histo":[[1386788400000,2199]]},"paHisto":52,"map":{"dst":{"CA":1},"src":{"RU":1},"xffGeo":{}},"name":"Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322)","lpHisto":1},{"byHisto":8383,"dbHisto":5287,"count":3,"graph":{"by2Histo":[[1386003600000,6145]],"xmin":1386003600000,"db2Histo":[[1386003600000,4801]],"xmax":1482552000000,"pa2Histo":[[1386003600000,20]],"lpHisto":[[1386003600000,3]],"interval":3600,"db1Histo":[[1386003600000,486]],"by1Histo":[[1386003600000,2238]],"pa1Histo":[[1386003600000,26]]},"map":{"src":{"US":3},"xffGeo":{},"dst":{"US":3}},"paHisto":46,"lpHisto":3,"name":"curl/7.24.0 (x86_64-apple-darwin12.0) libcurl/7.24.0 OpenSSL/0.9.8y zlib/1.2.5"}]'), "items field: http.useragent", { context => 3 });
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

# no map data
    $json = viewerGet("/spigraph.json?date=-1&field=http.useragent&expression=" . uri_escape("file=$pwd/socks5-reverse.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($json->{map}, from_json('{}'), "no map data");
