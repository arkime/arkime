use Test::More tests => 43;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $pwd = getcwd() . "/pcap";

my $json;

#node
    $json = viewerGet("/spigraph.json?date=-1&field=no&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3, "CAN": 1}, "src":{"USA": 3, "RUS":1}}'), "map field: no");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1]]'), "lpHisto field: no");
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1335956400000", 2], ["1386003600000", 46], [1387742400000, 4]]'), "paHisto field: no");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1335956400000", 0], ["1386003600000", 5287], [1387742400000, 68]]'), "dbHisto field: no");
    eq_or_diff($json->{items}, from_json('[{ "name": "test", "count": 5, "graph": { "lpHisto": [ [ 1335956400000, 1 ], [ 1386003600000, 3 ], [ 1387742400000, 1 ] ], "dbHisto": [ [ 1335956400000, 0 ], [ 1386003600000, 5287 ], [ 1387742400000, 68 ] ], "paHisto": [ [ 1335956400000, 2 ], [ 1386003600000, 46 ], [ 1387742400000, 4 ] ], "xmin": 1335956400000, "xmax": 1387742400000, "interval": 3600 }, "map": {"dst":{"USA": 3, "CAN": 1}, "src":{"USA": 3, "RUS":1}} } ]'), "items field: no");

#node multi
    $json = multiGet("/spigraph.json?date=-1&field=no&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3, "CAN": 1}, "src":{"USA": 3, "RUS":1}}'), "multi map field: no");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1]]'), "multi lpHisto field: no");
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1335956400000", 2], ["1386003600000", 46], [1387742400000, 4]]'), "multi paHisto field: no");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1335956400000", 0], ["1386003600000", 5287], [1387742400000, 68]]'), "multi dbHisto field: no");
    eq_or_diff($json->{items}, from_json('[{ "name": "test", "count": 5, "graph": { "lpHisto": [ [ 1335956400000, 1 ], [ 1386003600000, 3 ], [ 1387742400000, 1 ] ], "dbHisto": [ [ 1335956400000, 0 ], [ 1386003600000, 5287 ], [ 1387742400000, 68 ] ], "paHisto": [ [ 1335956400000, 2 ], [ 1386003600000, 46 ], [ 1387742400000, 4 ] ], "xmin": 1335956400000, "xmax": 1387742400000, "interval": 3600 }, "map": {"dst":{"USA": 3, "CAN": 1}, "src":{"USA": 3, "RUS":1}} } ]'), "multi items field: no");
    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3, "CAN": 1}, "src":{"USA": 3, "RUS":1}}'), "multi map ALL");



#ta
    $json = viewerGet("/spigraph.json?date=-1&field=ta&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3, "CAN": 1}, "src":{"USA": 3, "RUS":1}}'), "map field: ta");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1]]'), "lpHisto field: ta");
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1335956400000", 2], ["1386003600000", 46], [1387742400000, 4]]'), "paHisto field: ta");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1335956400000", 0], ["1386003600000", 5287], [1387742400000, 68]]'), "dbHisto field: ta");

    my @items = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
    eq_or_diff(\@items, from_json('[{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"byhost2","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}},{"map":{"src":{},"dst":{}},"count":1,"name":"byip2","graph":{"lpHisto":[[1335956400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1335956400000,2]],"dbHisto":[[1335956400000,0]]}},{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"domainwise","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}},{"map":{"src":{"RUS":1},"dst":{"CAN":1}},"count":1,"name":"dstip","graph":{"lpHisto":[[1387742400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1387742400000,4]],"dbHisto":[[1387742400000,68]]}},{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"hosttaggertest1","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}},{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"hosttaggertest2","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}},{"map":{"src":{},"dst":{}},"count":1,"name":"iptaggertest1","graph":{"lpHisto":[[1335956400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1335956400000,2]],"dbHisto":[[1335956400000,0]]}},{"map":{"src":{},"dst":{}},"count":1,"name":"iptaggertest2","graph":{"lpHisto":[[1335956400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1335956400000,2]],"dbHisto":[[1335956400000,0]]}},{"map":{"src":{},"dst":{}},"count":1,"name":"ipwise","graph":{"lpHisto":[[1335956400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1335956400000,2]],"dbHisto":[[1335956400000,0]]}},{"map":{"src":{"RUS":1},"dst":{"CAN":1}},"count":1,"name":"ipwisecsv","graph":{"lpHisto":[[1387742400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1387742400000,4]],"dbHisto":[[1387742400000,68]]}},{"map":{"src":{"RUS":1},"dst":{"CAN":1}},"count":1,"name":"srcip","graph":{"lpHisto":[[1387742400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1387742400000,4]],"dbHisto":[[1387742400000,68]]}},{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"wisebyhost2","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}},{"map":{"src":{},"dst":{}},"count":1,"name":"wisebyip2","graph":{"lpHisto":[[1335956400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1335956400000,2]],"dbHisto":[[1335956400000,0]]}}]'), "items field: ta");

#ta multi
    $json = multiGet("/spigraph.json?date=-1&field=ta&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3, "CAN": 1}, "src":{"USA": 3, "RUS":1}}'), "multi map field: ta");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1]]'), "multi lpHisto field: ta");
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1335956400000", 2], ["1386003600000", 46], [1387742400000, 4]]'), "multi paHisto field: ta");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1335956400000", 0], ["1386003600000", 5287], [1387742400000, 68]]'), "multi dbHisto field: ta");

    my @items = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
    eq_or_diff(\@items, from_json('[{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"byhost2","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}},{"map":{"src":{},"dst":{}},"count":1,"name":"byip2","graph":{"lpHisto":[[1335956400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1335956400000,2]],"dbHisto":[[1335956400000,0]]}},{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"domainwise","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}},{"map":{"src":{"RUS":1},"dst":{"CAN":1}},"count":1,"name":"dstip","graph":{"lpHisto":[[1387742400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1387742400000,4]],"dbHisto":[[1387742400000,68]]}},{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"hosttaggertest1","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}},{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"hosttaggertest2","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}},{"map":{"src":{},"dst":{}},"count":1,"name":"iptaggertest1","graph":{"lpHisto":[[1335956400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1335956400000,2]],"dbHisto":[[1335956400000,0]]}},{"map":{"src":{},"dst":{}},"count":1,"name":"iptaggertest2","graph":{"lpHisto":[[1335956400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1335956400000,2]],"dbHisto":[[1335956400000,0]]}},{"map":{"src":{},"dst":{}},"count":1,"name":"ipwise","graph":{"lpHisto":[[1335956400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1335956400000,2]],"dbHisto":[[1335956400000,0]]}},{"map":{"src":{"RUS":1},"dst":{"CAN":1}},"count":1,"name":"ipwisecsv","graph":{"lpHisto":[[1387742400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1387742400000,4]],"dbHisto":[[1387742400000,68]]}},{"map":{"src":{"RUS":1},"dst":{"CAN":1}},"count":1,"name":"srcip","graph":{"lpHisto":[[1387742400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1387742400000,4]],"dbHisto":[[1387742400000,68]]}},{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"wisebyhost2","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}},{"map":{"src":{},"dst":{}},"count":1,"name":"wisebyip2","graph":{"lpHisto":[[1335956400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1335956400000,2]],"dbHisto":[[1335956400000,0]]}}]'), "multi items field: ta");


#a1
    $json = viewerGet("/spigraph.json?date=-1&field=a1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3, "CAN": 1}, "src":{"USA": 3, "RUS":1}}'), "map field: a1");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1]]'), "lpHisto field: a1");
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1335956400000", 2], ["1386003600000", 46], [1387742400000, 4]]'), "paHisto field: a1");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1335956400000", 0], ["1386003600000", 5287], [1387742400000, 68]]'), "dbHisto field: a1");
    my @items = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
    eq_or_diff(\@items, from_json('[{"map":{"src":{"RUS":1},"dst":{"CAN":1}},"count":1,"name":"10.0.0.1","graph":{"lpHisto":[[1387742400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1387742400000,4]],"dbHisto":[[1387742400000,68]]}},{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"10.180.156.185","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}},{"map":{"src":{},"dst":{}},"count":1,"name":"192.168.177.160","graph":{"lpHisto":[[1335956400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1335956400000,2]],"dbHisto":[[1335956400000,0]]}}]'), "items field: a1");

#a1 multi
    $json = multiGet("/spigraph.json?date=-1&field=a1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3, "CAN": 1}, "src":{"USA": 3, "RUS":1}}'), "multi map field: a1");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1]]'), "multi lpHisto field: a1");
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1335956400000", 2], ["1386003600000", 46], [1387742400000, 4]]'), "multi paHisto field: a1");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1335956400000", 0], ["1386003600000", 5287], [1387742400000, 68]]'), "multi dbHisto field: a1");
    my @items = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
    eq_or_diff(\@items, from_json('[{"map":{"src":{"RUS":1},"dst":{"CAN":1}},"count":1,"name":"10.0.0.1","graph":{"lpHisto":[[1387742400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1387742400000,4]],"dbHisto":[[1387742400000,68]]}},{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"10.180.156.185","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}},{"map":{"src":{},"dst":{}},"count":1,"name":"192.168.177.160","graph":{"lpHisto":[[1335956400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1335956400000,2]],"dbHisto":[[1335956400000,0]]}}]'), "multi items field: a1");

#hh1
    $json = viewerGet("/spigraph.json?date=-1&field=hh1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3, "CAN": 1}, "src":{"USA": 3, "RUS":1}}'), "map field: hh1");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1]]'), "lpHisto field: hh1");
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1335956400000", 2], ["1386003600000", 46], [1387742400000, 4]]'), "paHisto field: hh1");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1335956400000", 0], ["1386003600000", 5287], [1387742400000, 68]]'), "dbHisto field: hh1");
    my @items = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
    eq_or_diff(\@items, from_json('[{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"accept","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}},{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"host","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}},{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"user-agent","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}}]'), "items field: hh1");

#hh1 multi
    $json = multiGet("/spigraph.json?date=-1&field=hh1&expression=" . uri_escape("file=$pwd/bigendian.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    eq_or_diff($json->{map}, from_json('{"dst":{"USA": 3, "CAN": 1}, "src":{"USA": 3, "RUS":1}}'), "multi map field: hh1");
    eq_or_diff($json->{graph}->{lpHisto}, from_json('[["1335956400000", 1], ["1386003600000", 3], [1387742400000, 1]]'), "multi lpHisto field: hh1");
    eq_or_diff($json->{graph}->{paHisto}, from_json('[["1335956400000", 2], ["1386003600000", 46], [1387742400000, 4]]'), "multi paHisto field: hh1");
    eq_or_diff($json->{graph}->{dbHisto}, from_json('[["1335956400000", 0], ["1386003600000", 5287], [1387742400000, 68]]'), "multi dbHisto field: hh1");
    my @items = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
    eq_or_diff(\@items, from_json('[{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"accept","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}},{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"host","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}},{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"user-agent","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1335956400000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}}]'), "multi items field: hh1");

#rawua
    $json = viewerGet("/spigraph.json?date=-1&field=rawua&expression=" . uri_escape("file=$pwd/socks5-reverse.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    my @items = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
    eq_or_diff(\@items, from_json('[{"map":{"src":{"RUS":1},"dst":{"CAN":1}},"count":1,"name":"Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322)","graph":{"lpHisto":[[1386788400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1386003600000,"paHisto":[[1386788400000,52]],"dbHisto":[[1386788400000,24346]]}},{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"curl/7.24.0 (x86_64-apple-darwin12.0) libcurl/7.24.0 OpenSSL/0.9.8y zlib/1.2.5","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1386003600000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}}]'), "items field: rawua");

#rawua multi
    $json = multiGet("/spigraph.json?date=-1&field=rawua&expression=" . uri_escape("file=$pwd/socks5-reverse.pcap|file=$pwd/socks-http-example.pcap|file=$pwd/bt-tcp.pcap"));
    my @items = sort({$a->{name} cmp $b->{name}} @{$json->{items}});
    eq_or_diff(\@items, from_json('[{"map":{"src":{"RUS":1},"dst":{"CAN":1}},"count":1,"name":"Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322)","graph":{"lpHisto":[[1386788400000,1]],"xmax":1387742400000,"interval":3600,"xmin":1386003600000,"paHisto":[[1386788400000,52]],"dbHisto":[[1386788400000,24346]]}},{"map":{"src":{"USA":3},"dst":{"USA":3}},"count":3,"name":"curl/7.24.0 (x86_64-apple-darwin12.0) libcurl/7.24.0 OpenSSL/0.9.8y zlib/1.2.5","graph":{"lpHisto":[[1386003600000,3]],"xmax":1387742400000,"interval":3600,"xmin":1386003600000,"paHisto":[[1386003600000,46]],"dbHisto":[[1386003600000,5287]]}}]'), "multi items field: rawua");
