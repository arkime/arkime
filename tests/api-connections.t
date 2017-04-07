use Test::More tests => 8;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $pwd = getcwd() . "/pcap";
my $files = "(file=$pwd/socks-http-example.pcap||file=$pwd/socks-http-pass.pcap||file=$pwd/socks-https-example.pcap||file=$pwd/socks5-http-302.pcap||file=$pwd/socks5-rdp.pcap||file=$pwd/socks5-reverse.pcap||file=$pwd/socks5-smtp-503.pcap)";


my $json;
# a1 to a2
    $json = viewerGet("/connections.json?date=-1&expression=" . uri_escape("$files"));
    delete $json->{health};
    eq_or_diff($json, from_json('{ "nodes": [ { "id": "10.0.0.1", "db": 26034, "by": 30979, "pa": 86, "cnt": 2, "sessions": 3, "type": 3, "pos": 0 }, { "id": "10.0.0.2", "db": 26119, "by": 31647, "pa": 96, "cnt": 3, "sessions": 4, "type": 3, "pos": 1 }, { "id": "10.180.156.185", "db": 33866, "by": 46190, "pa": 184, "cnt": 1, "sessions": 9, "type": 1, "pos": 2 }, { "id": "10.180.156.249", "db": 33866, "by": 46190, "pa": 184, "cnt": 1, "sessions": 9, "type": 2, "pos": 3 }, { "id": "10.0.0.3", "db": 85, "by": 668, "pa": 10, "cnt": 1, "sessions": 1, "type": 1, "pos": 4 } ], "links": [ { "value": 2, "source": 0, "target": 1, "by": 29487, "db": 25707, "pa": 66, "no": { "test": 1 } }, { "value": 1, "source": 1, "target": 0, "by": 1492, "db": 327, "pa": 20, "no": { "test": 1 } }, { "value": 9, "source": 2, "target": 3, "by": 46190, "db": 33866, "pa": 184, "no": { "test": 1 } }, { "value": 1, "source": 4, "target": 1, "by": 668, "db": 85, "pa": 10, "no": { "test": 1 } } ], "recordsFiltered": 13 }', {relaxed => 1}), "a1 to a2", { context => 3 });

    $json = multiGet("/connections.json?date=-1&expression=" . uri_escape("$files"));
    delete $json->{health};
    eq_or_diff($json, from_json('{ "nodes": [ { "id": "10.0.0.1", "db": 26034, "by": 30979, "pa": 86, "cnt": 2, "sessions": 3, "type": 3, "pos": 0 }, { "id": "10.0.0.2", "db": 26119, "by": 31647, "pa": 96, "cnt": 3, "sessions": 4, "type": 3, "pos": 1 }, { "id": "10.180.156.185", "db": 33866, "by": 46190, "pa": 184, "cnt": 1, "sessions": 9, "type": 1, "pos": 2 }, { "id": "10.180.156.249", "db": 33866, "by": 46190, "pa": 184, "cnt": 1, "sessions": 9, "type": 2, "pos": 3 }, { "id": "10.0.0.3", "db": 85, "by": 668, "pa": 10, "cnt": 1, "sessions": 1, "type": 1, "pos": 4 } ], "links": [ { "value": 2, "source": 0, "target": 1, "by": 29487, "db": 25707, "pa": 66, "no": { "test": 1 } }, { "value": 1, "source": 1, "target": 0, "by": 1492, "db": 327, "pa": 20, "no": { "test": 1 } }, { "value": 9, "source": 2, "target": 3, "by": 46190, "db": 33866, "pa": 184, "no": { "test": 1 } }, { "value": 1, "source": 4, "target": 1, "by": 668, "db": 85, "pa": 10, "no": { "test": 1 } } ], "recordsFiltered": 13 }', {relaxed => 1}), "multi a1 to a2", { context => 3 });


# a1 to ip.dst
    $json = viewerGet("/connections.json?date=-1&dstField=ip.dst:port&expression=" . uri_escape("$files"));
    delete $json->{health};
    eq_or_diff($json, from_json('{ "nodes": [ { "id": "10.0.0.1", "db": 25707, "by": 29487, "pa": 66, "cnt": 2, "sessions": 2, "type": 1, "pos": 0 }, { "id": "10.0.0.2:21477", "db": 1361, "by": 2176, "pa": 14, "cnt": 1, "sessions": 1, "type": 2, "pos": 1 }, { "id": "10.0.0.2", "db": 327, "by": 1492, "pa": 20, "cnt": 1, "sessions": 1, "type": 1, "pos": 2 }, { "id": "10.0.0.1:1080", "db": 327, "by": 1492, "pa": 20, "cnt": 1, "sessions": 1, "type": 2, "pos": 3 }, { "id": "10.180.156.185", "db": 33866, "by": 46190, "pa": 184, "cnt": 1, "sessions": 9, "type": 1, "pos": 4 }, { "id": "10.180.156.249:1080", "db": 33866, "by": 46190, "pa": 184, "cnt": 1, "sessions": 9, "type": 2, "pos": 5 }, { "id": "10.0.0.3", "db": 85, "by": 668, "pa": 10, "cnt": 1, "sessions": 1, "type": 1, "pos": 6 }, { "id": "10.0.0.2:42356", "db": 85, "by": 668, "pa": 10, "cnt": 1, "sessions": 1, "type": 2, "pos": 7 }, { "id": "10.0.0.2:8855", "db": 24346, "by": 27311, "pa": 52, "cnt": 1, "sessions": 1, "type": 2, "pos": 8 } ], "links": [ { "value": 1, "source": 0, "target": 1, "by": 2176, "db": 1361, "pa": 14, "no": { "test": 1 } }, { "value": 1, "source": 2, "target": 3, "by": 1492, "db": 327, "pa": 20, "no": { "test": 1 } }, { "value": 9, "source": 4, "target": 5, "by": 46190, "db": 33866, "pa": 184, "no": { "test": 1 } }, { "value": 1, "source": 6, "target": 7, "by": 668, "db": 85, "pa": 10, "no": { "test": 1 } }, { "value": 1, "source": 0, "target": 8, "by": 27311, "db": 24346, "pa": 52, "no": { "test": 1 } } ], "recordsFiltered": 13 }', {relaxed => 1}), "a1 to ip.dst", { context => 3 });

    $json = multiGet("/connections.json?date=-1&dstField=ip.dst:port&expression=" . uri_escape("$files"));
    delete $json->{health};
    eq_or_diff($json, from_json('{ "nodes": [ { "id": "10.0.0.1", "db": 25707, "by": 29487, "pa": 66, "cnt": 2, "sessions": 2, "type": 1, "pos": 0 }, { "id": "10.0.0.2:21477", "db": 1361, "by": 2176, "pa": 14, "cnt": 1, "sessions": 1, "type": 2, "pos": 1 }, { "id": "10.0.0.2", "db": 327, "by": 1492, "pa": 20, "cnt": 1, "sessions": 1, "type": 1, "pos": 2 }, { "id": "10.0.0.1:1080", "db": 327, "by": 1492, "pa": 20, "cnt": 1, "sessions": 1, "type": 2, "pos": 3 }, { "id": "10.180.156.185", "db": 33866, "by": 46190, "pa": 184, "cnt": 1, "sessions": 9, "type": 1, "pos": 4 }, { "id": "10.180.156.249:1080", "db": 33866, "by": 46190, "pa": 184, "cnt": 1, "sessions": 9, "type": 2, "pos": 5 }, { "id": "10.0.0.3", "db": 85, "by": 668, "pa": 10, "cnt": 1, "sessions": 1, "type": 1, "pos": 6 }, { "id": "10.0.0.2:42356", "db": 85, "by": 668, "pa": 10, "cnt": 1, "sessions": 1, "type": 2, "pos": 7 }, { "id": "10.0.0.2:8855", "db": 24346, "by": 27311, "pa": 52, "cnt": 1, "sessions": 1, "type": 2, "pos": 8 } ], "links": [ { "value": 1, "source": 0, "target": 1, "by": 2176, "db": 1361, "pa": 14, "no": { "test": 1 } }, { "value": 1, "source": 2, "target": 3, "by": 1492, "db": 327, "pa": 20, "no": { "test": 1 } }, { "value": 9, "source": 4, "target": 5, "by": 46190, "db": 33866, "pa": 184, "no": { "test": 1 } }, { "value": 1, "source": 6, "target": 7, "by": 668, "db": 85, "pa": 10, "no": { "test": 1 } }, { "value": 1, "source": 0, "target": 8, "by": 27311, "db": 24346, "pa": 52, "no": { "test": 1 } } ], "recordsFiltered": 13 }', {relaxed => 1}), "multi a1 to ip.dst", { context => 3 });


# a1 to tls.notAfter
    $json = viewerGet("/connections.json?date=-1&dstField=tls.notAfter&expression=" . uri_escape("$files"));
    delete $json->{health};
    eq_or_diff($json, from_json('{ "nodes": [ { "id": "1418212800", "db": 26760, "by": 32958, "pa": 93, "cnt": 1, "sessions": 3, "type": 2, "pos": 0 }, { "id": "1648944000", "db": 26760, "by": 32958, "pa": 93, "cnt": 1, "sessions": 3, "type": 2, "pos": 1 }, { "id": "10.180.156.185", "db": 53520, "by": 65916, "pa": 186, "cnt": 2, "sessions": 6, "type": 1, "pos": 2 } ], "links": [ { "value": 3, "source": 2, "target": 0, "by": 32958, "db": 26760, "pa": 93, "no": { "test": 1 } }, { "value": 3, "source": 2, "target": 1, "by": 32958, "db": 26760, "pa": 93, "no": { "test": 1 } } ], "recordsFiltered": 3 }', {relaxed => 1}), "a1 to tls.notAfter", { context => 3 });

    $json = multiGet("/connections.json?date=-1&dstField=tls.notAfter&expression=" . uri_escape("$files"));
    delete $json->{health};
    eq_or_diff($json, from_json('{ "nodes": [ { "id": "1418212800", "db": 26760, "by": 32958, "pa": 93, "cnt": 1, "sessions": 3, "type": 2, "pos": 0 }, { "id": "1648944000", "db": 26760, "by": 32958, "pa": 93, "cnt": 1, "sessions": 3, "type": 2, "pos": 1 }, { "id": "10.180.156.185", "db": 53520, "by": 65916, "pa": 186, "cnt": 2, "sessions": 6, "type": 1, "pos": 2 } ], "links": [ { "value": 3, "source": 2, "target": 0, "by": 32958, "db": 26760, "pa": 93, "no": { "test": 1 } }, { "value": 3, "source": 2, "target": 1, "by": 32958, "db": 26760, "pa": 93, "no": { "test": 1 } } ], "recordsFiltered": 3 }', {relaxed => 1}), "multi a1 to tls.notAfter", { context => 3 });

# ip.protocol unknown
    $json = viewerGet("/connections.json?date=-1&expression=" . uri_escape("$files&&ip.protocol==blah"));
    delete $json->{health};
    eq_or_diff($json, from_json('{ "success": false,  "text": "Unknown protocol string blah" }', {relaxed => 1}), "ip.protocol==blah", { context => 3 });

    $json = multiGet("/connections.json?date=-1&expression=" . uri_escape("$files&&ip.protocol==blah"));
    delete $json->{health};
    eq_or_diff($json, from_json('{ "success": false, "text": "Unknown protocol string blah" }', {relaxed => 1}), "multi ip.protocol==blah", { context => 3 });
