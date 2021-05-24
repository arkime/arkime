use Test::More tests => 11;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $pwd = "*";
my $files = "(file=$pwd/socks-http-example.pcap||file=$pwd/socks-http-pass.pcap||file=$pwd/socks-https-example.pcap||file=$pwd/socks5-http-302.pcap||file=$pwd/socks5-rdp.pcap||file=$pwd/socks5-reverse.pcap||file=$pwd/socks5-smtp-503.pcap)";
my $files_baseline = "(file=$pwd/smtp-originating.pcap||file=$pwd/dns-flags0000.pcap||file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap||file=$pwd/smtp-subject-windows.pcap||file=$pwd/dns-error.pcap||file=$pwd/smtp-subject-multi-nospace.pcap||file=$pwd/mysql-deny.pcap||file=$pwd/mysql-allow.pcap||file=$pwd/smtp-subject-encoded-empty.pcap||file=$pwd/postgres-good.pcap||file=$pwd/postgres-badpass.pcap||file=$pwd/long-session.pcap||file=$pwd/gre-erspan.pcap||file=$pwd/smb-port80.pcap||file=$pwd/dns-notify.pcap||file=$pwd/smtp-subject-utf8-q.pcap||file=$pwd/http-simple-get.pcap||file=$pwd/http-301-get.pcap||file=$pwd/https3-301-get.pcap||file=$pwd/dns-udp.pcap||file=$pwd/https2-301-get.pcap||file=$pwd/socks5-http-302-frag.pcap||file=$pwd/socks5-http-302.pcap||file=$pwd/socks5-smtp-503.pcap||file=$pwd/dns-tcp.pcap||file=$pwd/socks-http-example.pcap||file=$pwd/socks-https-example.pcap||file=$pwd/socks-http-pass.pcap||file=$pwd/dns-mx.pcap||file=$pwd/smtp-data-521.pcap||file=$pwd/smtp-rcpt-553.pcap||file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-subject-8859-b.pcap||file=$pwd/smtp-subject-gb2312-b.pcap||file=$pwd/socks5-rdp.pcap||file=$pwd/socks5-reverse.pcap||file=$pwd/smtp-subject-8859-q.pcap||file=$pwd/smtp-subject-utf8-mixed.pcap||file=$pwd/smtp-subject-8859-multi.pcap||file=$pwd/ip-boundaries.pcap||file=$pwd/bt-udp.pcap||file=$pwd/smtp-zip.pcap||file=$pwd/smb-smbclient.pcap||file=$pwd/irc.pcap||file=$pwd/ssh2-moloch-crash.pcap||file=$pwd/ssh2.pcap||file=$pwd/pop3-tag.pcap||file=$pwd/imap-tag.pcap||file=$pwd/smtp-starttls.pcap||file=$pwd/http-content-zip.pcap)";

my ($json, $mjson);
# srcIp to dstIp
    $json = viewerGet("/connections.json?date=-1&expression=" . uri_escape("$files"));
    eq_or_diff($json, from_json('{ "nodes": [
            { "id": "10.0.0.1", "totDataBytes": 26034, "network.bytes": 30979, "network.packets": 86, "cnt": 2, "sessions": 3, "type": 3, "pos": 0, "inresult": 1, "node": [ "test" ] },
            { "id": "10.0.0.2", "totDataBytes": 26119, "network.bytes": 31647, "network.packets": 96, "cnt": 3, "sessions": 4, "type": 3, "pos": 1, "inresult": 1, "node": [ "test" ] },
            { "id": "10.0.0.3", "totDataBytes": 85, "network.bytes": 668, "network.packets": 10, "cnt": 1, "sessions": 1, "type": 1, "pos": 2, "inresult": 1, "node": [ "test" ] },
            { "id": "10.180.156.185", "totDataBytes": 33866, "network.bytes": 46190, "network.packets": 184, "cnt": 1, "sessions": 9, "type": 1, "pos": 3, "inresult": 1, "node": [ "test" ] },
            { "id": "10.180.156.249", "totDataBytes": 33866, "network.bytes": 46190, "network.packets": 184, "cnt": 1, "sessions": 9, "type": 2, "pos": 4, "inresult": 1, "node": [ "test" ] } ], "links": [ { "value": 2, "source": 0, "target": 1, "network.bytes": 29487, "totDataBytes": 25707, "network.packets": 66, "node": [ "test" ] }, { "value": 1, "source": 1, "target": 0, "network.bytes": 1492, "totDataBytes": 327, "network.packets": 20, "node": [ "test" ] }, { "value": 9, "source": 3, "target": 4, "network.bytes": 46190, "totDataBytes": 33866, "network.packets": 184, "node": [ "test" ] }, { "value": 1, "source": 2, "target": 1, "network.bytes": 668, "totDataBytes": 85, "network.packets": 10, "node": [ "test" ] } ], "recordsFiltered": 13 }', {relaxed => 1}), "srcIp to dstIp", { context => 3 });

    $mjson = multiGet("/connections.json?date=-1&expression=" . uri_escape("$files"));
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

    $mjson = multiGet("/connections.json?date=-1&dstField=ip.dst:port&expression=" . uri_escape("$files"));
    eq_or_diff($mjson, $json, "multi: srcIp to ip.dst:port", { context => 3 });

# srcIp to cert.notAfter
    $json = viewerGet("/connections.json?date=-1&dstField=cert.notAfter&expression=" . uri_escape("$files"));
    eq_or_diff($json, from_json('{ "nodes": [
            { "id": "10.180.156.185", "totDataBytes": 53520, "network.bytes": 65916, "network.packets": 186, "cnt": 2, "sessions": 6, "type": 1, "pos": 0, "inresult": 1, "node": [ "test" ] },
            { "id": "1418212800000", "totDataBytes": 26760, "network.bytes": 32958, "network.packets": 93, "cnt": 1, "sessions": 3, "type": 2, "pos": 1, "inresult": 1, "node": [ "test" ] },
            { "id": "1648944000000", "totDataBytes": 26760, "network.bytes": 32958, "network.packets": 93, "cnt": 1, "sessions": 3, "type": 2, "pos": 2, "inresult": 1, "node": [ "test" ] }
            ], "links": [ { "value": 3, "source": 0, "target": 1, "network.bytes": 32958, "totDataBytes": 26760, "network.packets": 93, "node": [ "test" ] }, { "value": 3, "source": 0, "target": 2, "network.bytes": 32958, "totDataBytes": 26760, "network.packets": 93, "node": [ "test" ] } ], "recordsFiltered": 3 }', {relaxed => 1}), "srcIp to cert.notAfter", { context => 3 });

    $mjson = multiGet("/connections.json?date=-1&dstField=cert.notAfter&expression=" . uri_escape("$files"));
    eq_or_diff($mjson, $json, "multi: srcIp to cert.notAfter", { context => 3 });

# ip.protocol unknown
    $json = viewerGet("/connections.json?date=-1&expression=" . uri_escape("$files&&ip.protocol==blah"));
    my $pjson = viewerPost("/api/connections", '{"date":-1, "expression":"ip.protocol==blah"}');
    eq_or_diff($json, $pjson, "GET and POST versions of connections endpoint are not the same");
    eq_or_diff($json, from_json('{ "success": false,  "text": "Unknown protocol string blah" }', {relaxed => 1}), "ip.protocol==blah", { context => 3 });

    $mjson = multiGet("/connections.json?date=-1&expression=" . uri_escape("$files&&ip.protocol==blah"));
    eq_or_diff($mjson, from_json('{ "success": false, "text": "Unknown protocol string blah" }', {relaxed => 1}), "multi ip.protocol==blah", { context => 3 });

# node to protocols with baseline
    $json = viewerGet("/connections.json?date=0&startTime=1388559600&stopTime=1404194399&srcField=node&dstField=protocol&baselineDate=1x&baselineVis=all&expression=" . uri_escape("$files_baseline"));
    eq_or_diff($json, from_json('{ "nodes": [
            { "id": "bittorrent", "cnt": 1, "sessions": 4, "inresult": 2, "type": 2, "network.bytes": 955, "totDataBytes": 787, "network.packets": 4, "node": [ "test" ], "pos": 0 },
            { "id": "dns", "cnt": 1, "sessions": 10, "inresult": 3, "type": 2, "network.bytes": 3876, "totDataBytes": 1878, "network.packets": 35, "node": [ "test" ], "pos": 1 },
            { "id": "gre", "cnt": 1, "sessions": 1, "inresult": 1, "type": 2, "network.bytes": 656, "totDataBytes": 320, "network.packets": 4, "node": [ "test" ], "pos": 2 },
            { "id": "http", "cnt": 1, "sessions": 12, "inresult": 3, "type": 2, "network.bytes": 52451, "totDataBytes": 40719, "network.packets": 187, "node": [ "test" ], "pos": 3 },
            { "id": "icmp", "cnt": 1, "sessions": 1, "inresult": 1, "type": 2, "network.bytes": 656, "totDataBytes": 320, "network.packets": 4, "node": [ "test" ], "pos": 4 },
            { "id": "imap", "cnt": 1, "sessions": 1, "inresult": 2, "type": 2, "network.bytes": 256, "totDataBytes": 18, "network.packets": 4, "node": [ "test" ], "pos": 5 },
            { "id": "irc", "cnt": 1, "sessions": 1, "inresult": 2, "type": 2, "network.bytes": 8945, "totDataBytes": 7015, "network.packets": 29, "node": [ "test" ], "pos": 6 },
            { "id": "mysql", "cnt": 1, "sessions": 2, "inresult": 1, "type": 2, "network.bytes": 1966, "totDataBytes": 548, "network.packets": 21, "node": [ "test" ], "pos": 7 },
            { "id": "pop3", "cnt": 1, "sessions": 1, "inresult": 2, "type": 2, "network.bytes": 254, "totDataBytes": 16, "network.packets": 4, "node": [ "test" ], "pos": 8 },
            { "id": "postgresql", "cnt": 1, "sessions": 2, "inresult": 1, "type": 2, "network.bytes": 2172, "totDataBytes": 552, "network.packets": 24, "node": [ "test" ], "pos": 9 },
            { "id": "rdp", "cnt": 1, "sessions": 1, "inresult": 2, "type": 2, "network.bytes": 668, "totDataBytes": 85, "network.packets": 10, "node": [ "test" ], "pos": 10 },
            { "id": "smb", "cnt": 1, "sessions": 2, "inresult": 2, "type": 2, "network.bytes": 4776, "totDataBytes": 1906, "network.packets": 43, "node": [ "test" ], "pos": 11 },
            { "id": "smtp", "cnt": 1, "sessions": 16, "inresult": 3, "type": 2, "network.bytes": 56558, "totDataBytes": 30577, "network.packets": 404, "node": [ "test" ], "pos": 12 },
            { "id": "socks", "cnt": 1, "sessions": 13, "inresult": 2, "type": 2, "network.bytes": 79218, "totDataBytes": 61340, "network.packets": 283, "node": [ "test" ], "pos": 13 },
            { "id": "socksipset", "cnt": 1, "sessions": 1, "inresult": 2, "type": 2, "network.bytes": 27311, "totDataBytes": 24346, "network.packets": 52, "node": [ "test" ], "pos": 14 },
            { "id": "ssh", "cnt": 1, "sessions": 2, "inresult": 2, "type": 2, "network.bytes": 7504, "totDataBytes": 5032, "network.packets": 44, "node": [ "test" ], "pos": 15 },
            { "id": "tcp", "cnt": 1, "sessions": 49, "inresult": 3, "type": 2, "network.bytes": 184908, "totDataBytes": 123574, "network.packets": 954, "node": [ "test" ], "pos": 16 },
            { "id": "test", "cnt": 20, "sessions": 141, "inresult": 3, "type": 1, "network.bytes": 529262, "totDataBytes": 375065, "network.packets": 2416, "node": [ "test" ], "pos": 17 },
            { "id": "tls", "cnt": 1, "sessions": 6, "inresult": 2, "type": 2, "network.bytes": 54020, "totDataBytes": 42426, "network.packets": 175, "node": [ "test" ], "pos": 18 },
            { "id": "tlsrulestest", "cnt": 1, "sessions": 4, "inresult": 2, "type": 2, "network.bytes": 39298, "totDataBytes": 31598, "network.packets": 116, "node": [ "test" ], "pos": 19 },
            { "id": "udp", "cnt": 1, "sessions": 12, "inresult": 3, "type": 2, "network.bytes": 2814, "totDataBytes": 2008, "network.packets": 19, "node": [ "test" ], "pos": 20 }
            ], "links": [ { "value": 16, "source": 17, "target": 12, "network.bytes": 56558, "totDataBytes": 30577, "network.packets": 404, "node": [ "test" ] }, { "value": 49, "source": 17, "target": 16, "network.bytes": 184908, "totDataBytes": 123574, "network.packets": 954, "node": [ "test" ] }, { "value": 10, "source": 17, "target": 1, "network.bytes": 3876, "totDataBytes": 1878, "network.packets": 35, "node": [ "test" ] }, { "value": 12, "source": 17, "target": 20, "network.bytes": 2814, "totDataBytes": 2008, "network.packets": 19, "node": [ "test" ] }, { "value": 2, "source": 17, "target": 7, "network.bytes": 1966, "totDataBytes": 548, "network.packets": 21, "node": [ "test" ] }, { "value": 2, "source": 17, "target": 9, "network.bytes": 2172, "totDataBytes": 552, "network.packets": 24, "node": [ "test" ] }, { "value": 12, "source": 17, "target": 3, "network.bytes": 52451, "totDataBytes": 40719, "network.packets": 187, "node": [ "test" ] }, { "value": 1, "source": 17, "target": 2, "network.bytes": 656, "totDataBytes": 320, "network.packets": 4, "node": [ "test" ] }, { "value": 1, "source": 17, "target": 4, "network.bytes": 656, "totDataBytes": 320, "network.packets": 4, "node": [ "test" ] }, { "value": 2, "source": 17, "target": 11, "network.bytes": 4776, "totDataBytes": 1906, "network.packets": 43, "node": [ "test" ] }, { "value": 6, "source": 17, "target": 18, "network.bytes": 54020, "totDataBytes": 42426, "network.packets": 175, "node": [ "test" ] }, { "value": 4, "source": 17, "target": 19, "network.bytes": 39298, "totDataBytes": 31598, "network.packets": 116, "node": [ "test" ] }, { "value": 13, "source": 17, "target": 13, "network.bytes": 79218, "totDataBytes": 61340, "network.packets": 283, "node": [ "test" ] }, { "value": 1, "source": 17, "target": 10, "network.bytes": 668, "totDataBytes": 85, "network.packets": 10, "node": [ "test" ] }, { "value": 1, "source": 17, "target": 14, "network.bytes": 27311, "totDataBytes": 24346, "network.packets": 52, "node": [ "test" ] }, { "value": 4, "source": 17, "target": 0, "network.bytes": 955, "totDataBytes": 787, "network.packets": 4, "node": [ "test" ] }, { "value": 1, "source": 17, "target": 6, "network.bytes": 8945, "totDataBytes": 7015, "network.packets": 29, "node": [ "test" ] }, { "value": 2, "source": 17, "target": 15, "network.bytes": 7504, "totDataBytes": 5032, "network.packets": 44, "node": [ "test" ] }, { "value": 1, "source": 17, "target": 8, "network.bytes": 254, "totDataBytes": 16, "network.packets": 4, "node": [ "test" ] }, { "value": 1, "source": 17, "target": 5, "network.bytes": 256, "totDataBytes": 18, "network.packets": 4, "node": [ "test" ] } ], "recordsFiltered": 62 }', {relaxed => 1}), "node to protocol baselined", { context => 3 });

    $mjson = multiGet("/connections.json?date=0&startTime=1388559600&stopTime=1404194399&srcField=node&dstField=protocol&baselineDate=1x&baselineVis=all&expression=" . uri_escape("$files_baseline"));
    eq_or_diff($mjson, $json, "multi: node to protocol baselined", { context => 3 });
