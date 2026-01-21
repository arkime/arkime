use Test::More tests => 12;
use Cwd;
use URI::Escape;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $pwd = "*";
my $files = "(file=$pwd/socks-http-example.pcap||file=$pwd/socks-http-pass.pcap||file=$pwd/socks-https-example.pcap||file=$pwd/socks5-http-302.pcap||file=$pwd/socks5-rdp.pcap||file=$pwd/socks5-reverse.pcap||file=$pwd/socks5-smtp-503.pcap)";
my $files_baseline = "(file=$pwd/smtp-originating.pcap||file=$pwd/dns-flags0000.pcap||file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap||file=$pwd/smtp-subject-windows.pcap||file=$pwd/dns-error.pcap||file=$pwd/smtp-subject-multi-nospace.pcap||file=$pwd/mysql-deny.pcap||file=$pwd/mysql-allow.pcap||file=$pwd/smtp-subject-encoded-empty.pcap||file=$pwd/postgres-good.pcap||file=$pwd/postgres-badpass.pcap||file=$pwd/long-session.pcap||file=$pwd/gre-erspan.pcap||file=$pwd/gre-erspan-vxlan.pcap||file=$pwd/smb-port80.pcap||file=$pwd/dns-notify.pcap||file=$pwd/smtp-subject-utf8-q.pcap||file=$pwd/http-simple-get.pcap||file=$pwd/http-301-get.pcap||file=$pwd/https3-301-get.pcap||file=$pwd/dns-udp.pcap||file=$pwd/https2-301-get.pcap||file=$pwd/socks5-http-302-frag.pcap||file=$pwd/socks5-http-302.pcap||file=$pwd/socks5-smtp-503.pcap||file=$pwd/dns-tcp.pcap||file=$pwd/socks-http-example.pcap||file=$pwd/socks-https-example.pcap||file=$pwd/socks-http-pass.pcap||file=$pwd/dns-mx.pcap||file=$pwd/smtp-data-521.pcap||file=$pwd/smtp-rcpt-553.pcap||file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-subject-8859-b.pcap||file=$pwd/smtp-subject-gb2312-b.pcap||file=$pwd/socks5-rdp.pcap||file=$pwd/socks5-reverse.pcap||file=$pwd/smtp-subject-8859-q.pcap||file=$pwd/smtp-subject-utf8-mixed.pcap||file=$pwd/smtp-subject-8859-multi.pcap||file=$pwd/ip-boundaries.pcap||file=$pwd/bt-udp.pcap||file=$pwd/smtp-zip.pcap||file=$pwd/smb-smbclient.pcap||file=$pwd/irc.pcap||file=$pwd/ssh2-moloch-crash.pcap||file=$pwd/ssh2.pcap||file=$pwd/pop3-tag.pcap||file=$pwd/imap-tag.pcap||file=$pwd/smtp-starttls.pcap||file=$pwd/http-content-zip.pcap)";

my ($json, $mjson);
# srcIp to dstIp
    $json = viewerGet("/api/connections?date=-1&expression=" . uri_escape("$files"));
    eq_or_diff($json, from_json('{ "nodes": [
            { "id": "10.0.0.1", "totDataBytes": 26034, "network.bytes": 30979, "network.packets": 86, "cnt": 2, "sessions": 3, "type": 3, "pos": 0, "inresult": 1, "node": [ "test" ] },
            { "id": "10.0.0.2", "totDataBytes": 26119, "network.bytes": 31647, "network.packets": 96, "cnt": 3, "sessions": 4, "type": 3, "pos": 1, "inresult": 1, "node": [ "test" ] },
            { "id": "10.0.0.3", "totDataBytes": 85, "network.bytes": 668, "network.packets": 10, "cnt": 1, "sessions": 1, "type": 1, "pos": 2, "inresult": 1, "node": [ "test" ] },
            { "id": "10.180.156.185", "totDataBytes": 33866, "network.bytes": 46190, "network.packets": 184, "cnt": 1, "sessions": 9, "type": 1, "pos": 3, "inresult": 1, "node": [ "test" ] },
            { "id": "10.180.156.249", "totDataBytes": 33866, "network.bytes": 46190, "network.packets": 184, "cnt": 1, "sessions": 9, "type": 2, "pos": 4, "inresult": 1, "node": [ "test" ] } ], "links": [ { "value": 2, "source": 0, "target": 1, "network.bytes": 29487, "totDataBytes": 25707, "network.packets": 66, "node": [ "test" ] }, { "value": 1, "source": 1, "target": 0, "network.bytes": 1492, "totDataBytes": 327, "network.packets": 20, "node": [ "test" ] }, { "value": 9, "source": 3, "target": 4, "network.bytes": 46190, "totDataBytes": 33866, "network.packets": 184, "node": [ "test" ] }, { "value": 1, "source": 2, "target": 1, "network.bytes": 668, "totDataBytes": 85, "network.packets": 10, "node": [ "test" ] } ], "recordsFiltered": 13 }', {relaxed => 1}), "srcIp to dstIp", { context => 3 });

    $mjson = multiGet("/api/connections?date=-1&expression=" . uri_escape("$files"));
    eq_or_diff($mjson, $json, "multi: srcIp to dstIp", { context => 3 });


# srcIp to ip.dst
    $json = viewerPost("/api/connections", '{"date":-1, "dstField":"ip.dst:port", "expression":"' . $files . '"}');
    eq_or_diff($json, from_json('{ "nodes": [
            { "id": "10.0.0.1", "destination.port": [21477, 8855], "node": [ "test" ], "totDataBytes": 25707, "network.bytes": 29487, "network.packets": 66, "cnt": 2, "sessions": 2, "type": 1, "pos": 0, "inresult": 1 },
            { "id": "10.0.0.1:1080", "destination.port": [1080], "node": [ "test" ], "totDataBytes": 327, "network.bytes": 1492, "network.packets": 20, "cnt": 1, "sessions": 1, "type": 2, "pos": 1, "inresult": 1 },
            { "id": "10.0.0.2", "destination.port": [1080], "node": [ "test" ], "totDataBytes": 327, "network.bytes": 1492, "network.packets": 20, "cnt": 1, "sessions": 1, "type": 1, "pos": 2, "inresult": 1 },
            { "id": "10.0.0.2:21477", "destination.port": [21477], "node": [ "test" ], "totDataBytes": 1361, "network.bytes": 2176, "network.packets": 14, "cnt": 1, "sessions": 1, "type": 2, "pos": 3, "inresult": 1 },
            { "id": "10.0.0.2:42356", "destination.port": [42356], "node": [ "test" ], "totDataBytes": 85, "network.bytes": 668, "network.packets": 10, "cnt": 1, "sessions": 1, "type": 2, "pos": 4, "inresult": 1 },
            { "id": "10.0.0.2:8855", "destination.port": [8855], "node": [ "test" ], "totDataBytes": 24346, "network.bytes": 27311, "network.packets": 52, "cnt": 1, "sessions": 1, "type": 2, "pos": 5, "inresult": 1 },
            { "id": "10.0.0.3", "destination.port": [42356], "node": [ "test" ], "totDataBytes": 85, "network.bytes": 668, "network.packets": 10, "cnt": 1, "sessions": 1, "type": 1, "pos": 6, "inresult": 1 },
            { "id": "10.180.156.185", "destination.port": [1080], "node": [ "test" ], "totDataBytes": 33866, "network.bytes": 46190, "network.packets": 184, "cnt": 1, "sessions": 9, "type": 1, "pos": 7, "inresult": 1 },
            { "id": "10.180.156.249:1080", "destination.port": [1080], "node": [ "test" ], "totDataBytes": 33866, "network.bytes": 46190, "network.packets": 184, "cnt": 1, "sessions": 9, "type": 2, "pos": 8, "inresult": 1 }
            ], "links": [ { "destination.port": [21477], "value": 1, "source": 0, "target": 3, "network.bytes": 2176, "totDataBytes": 1361, "network.packets": 14, "node": [ "test" ] }, { "destination.port": [1080], "value": 1, "source": 2, "target": 1, "network.bytes": 1492, "totDataBytes": 327, "network.packets": 20, "node": [ "test" ] }, { "destination.port": [1080], "value": 9, "source": 7, "target": 8, "network.bytes": 46190, "totDataBytes": 33866, "network.packets": 184, "node": [ "test" ] }, { "destination.port": [42356], "value": 1, "source": 6, "target": 4, "network.bytes": 668, "totDataBytes": 85, "network.packets": 10, "node": [ "test" ] }, { "destination.port": [8855], "value": 1, "source": 0, "target": 5, "network.bytes": 27311, "totDataBytes": 24346, "network.packets": 52, "node": [ "test" ] } ], "recordsFiltered": 13 }', {relaxed => 1}), "srcIp to ip.dst", { context => 3 });

    $mjson = multiGet("/api/connections?date=-1&dstField=ip.dst:port&expression=" . uri_escape("$files"));
    eq_or_diff($mjson, $json, "multi: srcIp to ip.dst:port", { context => 3 });

# srcIp to cert.notAfter
    $json = viewerGet("/api/connections?date=-1&dstField=cert.notAfter&expression=" . uri_escape("$files"));
    eq_or_diff($json, from_json('{ "nodes": [
            { "id": "10.180.156.185", "totDataBytes": 53520, "network.bytes": 65916, "network.packets": 186, "cnt": 2, "sessions": 6, "type": 1, "pos": 0, "inresult": 1, "node": [ "test" ] },
            { "id": "1418212800000", "totDataBytes": 26760, "network.bytes": 32958, "network.packets": 93, "cnt": 1, "sessions": 3, "type": 2, "pos": 1, "inresult": 1, "node": [ "test" ] },
            { "id": "1648944000000", "totDataBytes": 26760, "network.bytes": 32958, "network.packets": 93, "cnt": 1, "sessions": 3, "type": 2, "pos": 2, "inresult": 1, "node": [ "test" ] }
            ], "links": [ { "value": 3, "source": 0, "target": 1, "network.bytes": 32958, "totDataBytes": 26760, "network.packets": 93, "node": [ "test" ] }, { "value": 3, "source": 0, "target": 2, "network.bytes": 32958, "totDataBytes": 26760, "network.packets": 93, "node": [ "test" ] } ], "recordsFiltered": 3 }', {relaxed => 1}), "srcIp to cert.notAfter", { context => 3 });

    $mjson = multiGet("/api/connections?date=-1&dstField=cert.notAfter&expression=" . uri_escape("$files"));
    eq_or_diff($mjson, $json, "multi: srcIp to cert.notAfter", { context => 3 });

# ip.protocol unknown
    $json = viewerGet("/api/connections?date=-1&expression=" . uri_escape("$files&&ip.protocol==blah"));
    my $pjson = viewerPost("/api/connections", '{"date":-1, "expression":"ip.protocol==blah"}');
    eq_or_diff($json, $pjson, "GET and POST versions of connections endpoint are not the same");
    eq_or_diff($json, from_json('{ "success": false,  "text": "Unknown protocol string blah" }', {relaxed => 1}), "ip.protocol==blah", { context => 3 });

    $mjson = multiGet("/api/connections?date=-1&expression=" . uri_escape("$files&&ip.protocol==blah"));
    eq_or_diff($mjson, from_json('{ "success": false, "text": "Unknown protocol string blah" }', {relaxed => 1}), "multi ip.protocol==blah", { context => 3 });

# node to protocols with baseline
    $json = viewerGet("/api/connections?date=0&startTime=1388559600&stopTime=1404194399&srcField=node&dstField=protocol&baselineDate=1x&baselineVis=all&expression=" . uri_escape("$files_baseline"));
    eq_or_diff($json, from_json('{"nodes":[
            {"network.packets":4,"network.bytes":955,"node":["test"],"sessions":4,"type":2,"totDataBytes":787,"cnt":1,"pos":0,"inresult":2,"id":"bittorrent"},
            {"inresult":1,"pos":1,"cnt":1,"id":"corrupt-ip","node":["test"],"network.bytes":903,"network.packets":2,"totDataBytes":0,"type":2,"sessions":1},
            {"totDataBytes":1878,"sessions":10,"type":2,"network.packets":35,"node":["test"],"network.bytes":3876,"id":"dns","pos":2,"inresult":3,"cnt":1},
            {"totDataBytes":320,"sessions":2,"type":2,"network.packets":6,"node":["test"],"network.bytes":1559,"id":"gre","inresult":1,"pos":3,"cnt":1},
            {"inresult":3,"pos":4,"cnt":1,"id":"http","network.packets":187,"node":["test"],"network.bytes":52451,"totDataBytes":40719,"type":2,"sessions":12},
            {"pos":5,"inresult":1,"cnt":1,"id":"icmp","network.packets":4,"node":["test"],"network.bytes":656,"totDataBytes":320,"type":2,"sessions":1},
            {"sessions":1,"type":2,"totDataBytes":18,"network.packets":4,"node":["test"],"network.bytes":256,"id":"imap","cnt":1,"inresult":2,"pos":6},
            {"id":"irc","pos":7,"inresult":2,"cnt":1,"totDataBytes":7015,"type":2,"sessions":1,"network.bytes":8945,"node":["test"],"network.packets":29},
            {"totDataBytes":548,"sessions":2,"type":2,"network.packets":21,"network.bytes":1966,"node":["test"],"id":"mysql","inresult":1,"pos":8,"cnt":1},
            {"id":"pop3","cnt":1,"inresult":2,"pos":9,"sessions":1,"type":2,"totDataBytes":16,"node":["test"],"network.bytes":254,"network.packets":4},
            {"cnt":1,"pos":10,"inresult":1,"id":"postgresql","network.packets":24,"node":["test"],"network.bytes":2172,"sessions":2,"type":2,"totDataBytes":552},
            {"inresult":2,"pos":11,"cnt":1,"id":"rdp","network.bytes":668,"node":["test"],"network.packets":10,"totDataBytes":85,"sessions":1,"type":2},
            {"node":["test"],"network.bytes":4776,"network.packets":43,"type":2,"sessions":2,"totDataBytes":1906,"cnt":1,"inresult":2,"pos":12,"id":"smb"},
            {"id":"smtp","cnt":1,"pos":13,"inresult":3,"sessions":16,"type":2,"totDataBytes":30577,"network.packets":404,"network.bytes":56558,"node":["test"]},
            {"network.packets":283,"node":["test"],"network.bytes":79218,"totDataBytes":61340,"type":2,"sessions":13,"inresult":2,"pos":14,"cnt":1,"id":"socks"},
            {"network.packets":52,"network.bytes":27311,"node":["test"],"totDataBytes":24346,"sessions":1,"type":2,"pos":15,"inresult":2,"cnt":1,"id":"socksipset"},
            {"sessions":2,"type":2,"totDataBytes":5032,"network.packets":44,"node":["test"],"network.bytes":7504,"id":"ssh","cnt":1,"inresult":2,"pos":16},
            {"network.packets":954,"node":["test"],"network.bytes":184908,"totDataBytes":123574,"type":2,"sessions":49,"pos":17,"inresult":3,"cnt":1,"id":"tcp"},
            {"totDataBytes":375065,"sessions":143,"type":1,"node":["test"],"network.bytes":531068,"network.packets":2420,"id":"test","pos":18,"inresult":3,"cnt":21},
            {"totDataBytes":42426,"type":2,"sessions":6,"node":["test"],"network.bytes":54020,"network.packets":175,"id":"tls","pos":19,"inresult":2,"cnt":1},
            {"inresult":2,"pos":20,"cnt":1,"id":"tlsrulestest","node":["test"],"network.bytes":39298,"network.packets":116,"totDataBytes":31598,"type":2,"sessions":4},
            {"totDataBytes":2008,"type":2,"sessions":12,"network.packets":19,"node":["test"],"network.bytes":2814,"id":"udp","inresult":3,"pos":21,"cnt":1}],"recordsFiltered":63,"links":[{"target":13,"network.packets":404,"node":["test"],"network.bytes":56558,"source":18,"totDataBytes":30577,"value":16},
            {"target":17,"totDataBytes":123574,"value":49,"network.packets":954,"node":["test"],"network.bytes":184908,"source":18},
            {"totDataBytes":1878,"value":10,"node":["test"],"network.bytes":3876,"network.packets":35,"source":18,"target":2},
            {"source":18,"node":["test"],"network.bytes":2814,"network.packets":19,"value":12,"totDataBytes":2008,"target":21},
            {"value":2,"totDataBytes":548,"source":18,"node":["test"],"network.bytes":1966,"network.packets":21,"target":8},
            {"target":10,"value":2,"totDataBytes":552,"source":18,"network.packets":24,"network.bytes":2172,"node":["test"]},
            {"network.bytes":52451,"node":["test"],"network.packets":187,"source":18,"totDataBytes":40719,"value":12,"target":4},
            {"target":1,"source":18,"node":["test"],"network.bytes":903,"network.packets":2,"value":1,"totDataBytes":0},
            {"target":3,"source":18,"network.packets":6,"network.bytes":1559,"node":["test"],"value":2,"totDataBytes":320},
            {"totDataBytes":320,"value":1,"node":["test"],"network.bytes":656,"network.packets":4,"source":18,"target":5},
            {"totDataBytes":1906,"value":2,"network.packets":43,"node":["test"],"network.bytes":4776,"source":18,"target":12},
            {"value":6,"totDataBytes":42426,"source":18,"node":["test"],"network.bytes":54020,"network.packets":175,"target":19},
            {"target":20,"totDataBytes":31598,"value":4,"network.packets":116,"node":["test"],"network.bytes":39298,"source":18},
            {"value":13,"totDataBytes":61340,"source":18,"network.packets":283,"network.bytes":79218,"node":["test"],"target":14},
            {"value":1,"totDataBytes":85,"source":18,"network.packets":10,"node":["test"],"network.bytes":668,"target":11},
            {"totDataBytes":24346,"value":1,"network.packets":52,"node":["test"],"network.bytes":27311,"source":18,"target":15},
            {"value":4,"totDataBytes":787,"source":18,"network.packets":4,"node":["test"],"network.bytes":955,"target":0},
            {"value":1,"totDataBytes":7015,"source":18,"network.packets":29,"network.bytes":8945,"node":["test"],"target":7},
            {"totDataBytes":5032,"value":2,"node":["test"],"network.bytes":7504,"network.packets":44,"source":18,"target":16},
            {"value":1,"totDataBytes":16,"source":18,"network.packets":4,"network.bytes":254,"node":["test"],"target":9},
            {"target":6,"source":18,"network.packets":4,"network.bytes":256,"node":["test"],"value":1,"totDataBytes":18}]}', {relaxed => 1}), "node to protocol baselined", { context => 3 });

    $mjson = multiGet("/api/connections?date=0&startTime=1388559600&stopTime=1404194399&srcField=node&dstField=protocol&baselineDate=1x&baselineVis=all&expression=" . uri_escape("$files_baseline"));
    eq_or_diff($mjson, $json, "multi: node to protocol baselined", { context => 3 });

# csv
    my $csv = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/connections.csv?date=-1&dstField=cert.notAfter&expression=" . uri_escape("$files"))->content;
    $csv =~ s/\r//g;
    eq_or_diff($csv, qq(Source, Destination, Sessions, Bytes, Bytes, Data bytes, Packets, Packets, Arkime Node
"10.180.156.185","1418212800000",3,32958,26760,93,test
"10.180.156.185","1648944000000",3,32958,26760,93,test
));
