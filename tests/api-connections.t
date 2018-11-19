use Test::More tests => 8;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $pwd = "*";
my $files = "(file=$pwd/socks-http-example.pcap||file=$pwd/socks-http-pass.pcap||file=$pwd/socks-https-example.pcap||file=$pwd/socks5-http-302.pcap||file=$pwd/socks5-rdp.pcap||file=$pwd/socks5-reverse.pcap||file=$pwd/socks5-smtp-503.pcap)";


my ($json, $mjson);
# srcIp to dstIp
    $json = viewerGet("/connections.json?date=-1&expression=" . uri_escape("$files"));
    delete $json->{health};
    eq_or_diff($json, from_json('{ "nodes": [
            { "id": "10.0.0.1", "totDataBytes": 26034, "totBytes": 30979, "totPackets": 86, "cnt": 2, "sessions": 3, "type": 3, "pos": 0, "node": [ "test" ] },
            { "id": "10.0.0.2", "totDataBytes": 26119, "totBytes": 31647, "totPackets": 96, "cnt": 3, "sessions": 4, "type": 3, "pos": 1, "node": [ "test" ] },
            { "id": "10.0.0.3", "totDataBytes": 85, "totBytes": 668, "totPackets": 10, "cnt": 1, "sessions": 1, "type": 1, "pos": 2, "node": [ "test" ] },
            { "id": "10.180.156.185", "totDataBytes": 33866, "totBytes": 46190, "totPackets": 184, "cnt": 1, "sessions": 9, "type": 1, "pos": 3, "node": [ "test" ] },
            { "id": "10.180.156.249", "totDataBytes": 33866, "totBytes": 46190, "totPackets": 184, "cnt": 1, "sessions": 9, "type": 2, "pos": 4, "node": [ "test" ] } ], "links": [ { "value": 2, "source": 0, "target": 1, "totBytes": 29487, "totDataBytes": 25707, "totPackets": 66, "node": [ "test" ] }, { "value": 1, "source": 1, "target": 0, "totBytes": 1492, "totDataBytes": 327, "totPackets": 20, "node": [ "test" ] }, { "value": 9, "source": 3, "target": 4, "totBytes": 46190, "totDataBytes": 33866, "totPackets": 184, "node": [ "test" ] }, { "value": 1, "source": 2, "target": 1, "totBytes": 668, "totDataBytes": 85, "totPackets": 10, "node": [ "test" ] } ], "recordsFiltered": 13 }', {relaxed => 1}), "srcIp to dstIp", { context => 3 });

    $mjson = multiGet("/connections.json?date=-1&expression=" . uri_escape("$files"));
    delete $mjson->{health};
    eq_or_diff($mjson, $json, "multi: srcIp to dstIp", { context => 3 });


# srcIp to ip.dst
    $json = viewerGet("/connections.json?date=-1&dstField=ip.dst:port&expression=" . uri_escape("$files"));
    delete $json->{health};
    eq_or_diff($json, from_json('{ "nodes": [
            { "id": "10.0.0.1", "dstPort": [21477, 8855], "node": [ "test" ], "totDataBytes": 25707, "totBytes": 29487, "totPackets": 66, "cnt": 2, "sessions": 2, "type": 1, "pos": 0 },
            { "id": "10.0.0.1:1080", "dstPort": [1080], "node": [ "test" ], "totDataBytes": 327, "totBytes": 1492, "totPackets": 20, "cnt": 1, "sessions": 1, "type": 2, "pos": 1 },
            { "id": "10.0.0.2", "dstPort": [1080], "node": [ "test" ], "totDataBytes": 327, "totBytes": 1492, "totPackets": 20, "cnt": 1, "sessions": 1, "type": 1, "pos": 2 },
            { "id": "10.0.0.2:21477", "dstPort": [21477], "node": [ "test" ], "totDataBytes": 1361, "totBytes": 2176, "totPackets": 14, "cnt": 1, "sessions": 1, "type": 2, "pos": 3 },
            { "id": "10.0.0.2:42356", "dstPort": [42356], "node": [ "test" ], "totDataBytes": 85, "totBytes": 668, "totPackets": 10, "cnt": 1, "sessions": 1, "type": 2, "pos": 4 },
            { "id": "10.0.0.2:8855", "dstPort": [8855], "node": [ "test" ], "totDataBytes": 24346, "totBytes": 27311, "totPackets": 52, "cnt": 1, "sessions": 1, "type": 2, "pos": 5 },
            { "id": "10.0.0.3", "dstPort": [42356], "node": [ "test" ], "totDataBytes": 85, "totBytes": 668, "totPackets": 10, "cnt": 1, "sessions": 1, "type": 1, "pos": 6 },
            { "id": "10.180.156.185", "dstPort": [1080], "node": [ "test" ], "totDataBytes": 33866, "totBytes": 46190, "totPackets": 184, "cnt": 1, "sessions": 9, "type": 1, "pos": 7 },
            { "id": "10.180.156.249:1080", "dstPort": [1080], "node": [ "test" ], "totDataBytes": 33866, "totBytes": 46190, "totPackets": 184, "cnt": 1, "sessions": 9, "type": 2, "pos": 8 }
            ], "links": [ { "dstPort": [21477], "value": 1, "source": 0, "target": 3, "totBytes": 2176, "totDataBytes": 1361, "totPackets": 14, "node": [ "test" ] }, { "dstPort": [1080], "value": 1, "source": 2, "target": 1, "totBytes": 1492, "totDataBytes": 327, "totPackets": 20, "node": [ "test" ] }, { "dstPort": [1080], "value": 9, "source": 7, "target": 8, "totBytes": 46190, "totDataBytes": 33866, "totPackets": 184, "node": [ "test" ] }, { "dstPort": [42356], "value": 1, "source": 6, "target": 4, "totBytes": 668, "totDataBytes": 85, "totPackets": 10, "node": [ "test" ] }, { "dstPort": [8855], "value": 1, "source": 0, "target": 5, "totBytes": 27311, "totDataBytes": 24346, "totPackets": 52, "node": [ "test" ] } ], "recordsFiltered": 13 }', {relaxed => 1}), "srcIp to ip.dst", { context => 3 });

    $mjson = multiGet("/connections.json?date=-1&dstField=ip.dst:port&expression=" . uri_escape("$files"));
    delete $mjson->{health};
    eq_or_diff($mjson, $json, "multi: srcIp to ip.dst:port", { context => 3 });

# srcIp to cert.notAfter
    $json = viewerGet("/connections.json?date=-1&dstField=cert.notAfter&expression=" . uri_escape("$files"));
    delete $json->{health};
    eq_or_diff($json, from_json('{ "nodes": [
            { "id": "10.180.156.185", "totDataBytes": 53520, "totBytes": 65916, "totPackets": 186, "cnt": 2, "sessions": 6, "type": 1, "pos": 0, "node": [ "test" ] },
            { "id": "1418212800000", "totDataBytes": 26760, "totBytes": 32958, "totPackets": 93, "cnt": 1, "sessions": 3, "type": 2, "pos": 1, "node": [ "test" ] },
            { "id": "1648944000000", "totDataBytes": 26760, "totBytes": 32958, "totPackets": 93, "cnt": 1, "sessions": 3, "type": 2, "pos": 2, "node": [ "test" ] }
            ], "links": [ { "value": 3, "source": 0, "target": 1, "totBytes": 32958, "totDataBytes": 26760, "totPackets": 93, "node": [ "test" ] }, { "value": 3, "source": 0, "target": 2, "totBytes": 32958, "totDataBytes": 26760, "totPackets": 93, "node": [ "test" ] } ], "recordsFiltered": 3 }', {relaxed => 1}), "srcIp to cert.notAfter", { context => 3 });

    $mjson = multiGet("/connections.json?date=-1&dstField=cert.notAfter&expression=" . uri_escape("$files"));
    delete $mjson->{health};
    eq_or_diff($mjson, $json, "multi: srcIp to cert.notAfter", { context => 3 });

# ip.protocol unknown
    $json = viewerGet("/connections.json?date=-1&expression=" . uri_escape("$files&&ip.protocol==blah"));
    delete $json->{health};
    eq_or_diff($json, from_json('{ "success": false,  "text": "Unknown protocol string blah" }', {relaxed => 1}), "ip.protocol==blah", { context => 3 });

    $json = multiGet("/connections.json?date=-1&expression=" . uri_escape("$files&&ip.protocol==blah"));
    delete $json->{health};
    eq_or_diff($json, from_json('{ "success": false, "text": "Unknown protocol string blah" }', {relaxed => 1}), "multi ip.protocol==blah", { context => 3 });
