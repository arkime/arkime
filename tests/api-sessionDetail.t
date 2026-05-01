use Test::More tests => 39;

use Cwd;
use URI::Escape;
use ArkimeTest;
use JSON;
use Data::Dumper;
use strict;

my $pwd = "*/pcap";

# new /detail api
    my $sdId = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/http-content-gzip.pcap"));
    my $id = $sdId->{data}->[0]->{id};
    my $encodedId = uri_escape($id);

    my $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/detail")->content;
    ok($sd =~ m{sessionid.*\Q$id\E}s, "/detail");
    ok($sd =~ m{Tags.*md5taggertest1}s, "/detail Tags");

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8125/api/session/test/$id/detail")->content;
    ok($sd =~ m{sessionid.*\Q$id\E}s, "multi /detail");
    ok($sd =~ m{Tags.*md5taggertest1}s, "multi /detail Tags");

# http
    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=false&ts=false&base=natural&showFrames=true")->content;
    my $count = 0;
    while ($sd =~ /ts-value/g) {
        $count++;
    }
    is (12, $count);

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=false&ts=false&base=natural")->content;
    ok(bin2hex($sd) =~ /636f6c3a2038303a71756963.*08000000000002/, "encoding:natural");

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=false&ts=false&base=ascii")->content;
    ok(bin2hex($sd) =~ /636f6c3a2038303a71756963.*08000000000002/, "encoding:ascii");

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=false&ts=false&base=hex")->content;
    ok($sd =~ /636f 6c3a 2038 303a 7175 6963 0d0a 0d0a.*col:.80:quic.*0800 0000 0000 02ff/s, "encoding:hex");

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=true&ts=false&base=hex")->content;
    ok($sd =~ /00000272:.*636f 6c3a 2038 303a 7175 6963 0d0a 0d0a.*col:.80:quic.*00000000:.*0800 0000 0000 02ff/s, "encoding:hex line:true");

# http gzip:true
    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=false&ts=false&base=natural&gzip=true")->content;
    ok($sd =~ /col:.80:quic.*&lt;\?xml version=&quot;1.0&quot;\?&gt;/s, "encoding:natural gzip:true");

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=false&ts=false&base=ascii&gzip=true")->content;
    ok($sd =~ /col:.80:quic.*&lt;\?xml version=&quot;1.0&quot;\?&gt;/s, "encoding:ascii gzip:true");

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=false&ts=false&base=hex&gzip=true")->content;
    ok($sd =~ /636f 6c3a 2038 303a 7175 6963 0d0a 0d0a.*col:.80:quic.*3c3f 786d 6c20 7665 7273 696f 6e3d 2231.*&lt;\?xml.version=&quot;1/s, "encoding:hex gzip:true");

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=true&ts=false&base=hex&gzip=true")->content;
    ok($sd =~ /00000272:.*636f 6c3a 2038 303a 7175 6963 0d0a 0d0a.*col:.80:quic.*00000000:.*3c3f 786d 6c20 7665 7273 696f 6e3d 2231.*&lt;\?xml.version=&quot;1/s, "encoding:hex gzip:true");


# http image:true
    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=false&ts=false&base=natural&image=true")->content;
    ok($sd =~ m{col:.80:quic.*test/\Q$encodedId\E/body/file/1/crossdomain.xml.pellet}s, "encoding:natural image:true");

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=false&ts=false&base=ascii&image=true")->content;
    ok($sd =~ m{col:.80:quic.*test/\Q$encodedId\E/body/file/1/crossdomain.xml.pellet}s, "encoding:ascii image:true");

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=false&ts=false&base=hex&image=true")->content;
    ok($sd =~ m{636f 6c3a 2038 303a 7175 6963 0d0a 0d0a.*col:.80:quic.*test/\Q$encodedId\E/body/file/1/crossdomain.xml.pellet}s, "encoding:hex image:true");

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=true&ts=false&base=hex&image=true")->content;
    ok($sd =~ m{00000272:.*636f 6c3a 2038 303a 7175 6963 0d0a 0d0a.*col:.80:quic.*test/\Q$encodedId\E/body/file/1/crossdomain.xml.pellet}s, "encoding:hex line:true image:true");

# smtp
    $sdId = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/smtp-zip.pcap"));
    $id = $sdId->{data}->[0]->{id};
    $encodedId = uri_escape($id);

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=false&ts=false&base=natural&image=false")->content;
    ok($sd =~ /UEsDBAoAAAAAACdOkUME9yniBgAAAAYAAAAFABwAZmlsZTFVVAkAA2lksFJpZLBSdXgLAAEE/, "smtp encoding:natural image:false");

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=false&ts=false&base=natural&image=true")->content;
    ok($sd =~ m{href="api/session/test/\Q$encodedId\E/body/file/1/a.zip.pellet">a.zip<\/a>}s, "smtp encoding:natural image:true");

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/test\/$id\/body\/file\/1\/a.zip.pellet")->content;
    ok(bin2hex($sd) eq "504b03040a0000000000274e914304f729e2060000000600000005001c0066696c653155540900036964b0526964b05275780b0001044c060000040204000066696c65310a504b03040a0000000000294e9143c7a404c9060000000600000005001c0066696c653255540900036e64b0526e64b05275780b0001044c060000040204000066696c65320a504b01021e030a0000000000274e914304f729e20600000006000000050018000000000001000000a4810000000066696c653155540500036964b05275780b0001044c0600000402040000504b01021e030a0000000000294e9143c7a404c90600000006000000050018000000000001000000a4814500000066696c653255540500036e64b05275780b0001044c0600000402040000504b05060000000002000200960000008a0000000000", "smtp zip file");

# smtp-html
    $sdId = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/smtp-html.pcap"));
    $id = $sdId->{data}->[0]->{id};
    $encodedId = uri_escape($id);
    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=false&ts=false&base=natural&image=true")->content;
    ok($sd =~ m{&lt;a href=http:&#47;&#47;link.com&gt;link&lt;&#47;a&gt;}s, "smtp-html encoding:natural image:true");

# bodyHash in session
    $sdId = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/80211http.pcap"));
    $id = $sdId->{data}->[0]->{id};

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/bodyhash/87b94c6c763d2ae6e47cfb9c35e6d54d8a84bf125cbc9430082a9f1b3e592bec")->content;
    is($sd, "Username=Qggh&Passwd=Bhhhh&Section=Auuu", "Right sha256");

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/bodyhash/a4a7b8b5eaf4c2cf356b1052133c6cdb")->content;
    is($sd, "Username=Q&Passwd=B&Section=A", "Right md5");

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/bodyhash/87b94c6c763d2ae6e47cfb9c35e6d54d8a84bf125cbc9430082a")->content;
    is($sd, "No match", "No Match");

# Global bodyHash
    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/sessions/bodyhash/87b94c6c763d2ae6e47cfb9c35e6d54d8a84bf125cbc9430082a9f1b3e592bec?date=-1&expression=http.sha256=87b94c6c763d2ae6e47cfb9c35e6d54d8a84bf125cbc9430082a9f1b3e592bec")->content;
    is($sd, "Username=Qggh&Passwd=Bhhhh&Section=Auuu", "Global Right sha256");

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/sessions/bodyhash/a4a7b8b5eaf4c2cf356b1052133c6cdb?date=-1&expression=http.md5=a4a7b8b5eaf4c2cf356b1052133c6cdb")->content;
    is($sd, "Username=Q&Passwd=B&Section=A", "Global Right md5");

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/sessions/bodyhash/a4a7b8b5eaf4c2cf356b1052133c6cdb?date=-1&expression=http.md5=1")->content;
    is($sd, "No Match Found", "No Match Found");

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/sessions/bodyhash/a4a7b8b5eaf4c2cf356b105c6cdb?date=-1&expression=http.md5=a4a7b8b5eaf4c2cf356b1052133c6cdb")->content;
    is($sd, "No match", "No Match");

# ipv6/4 port separators
    $sdId = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/v6.pcap"));
    $id = $sdId->{data}->[0]->{id};
    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/detail")->content;
    ok($sd =~ m|'sessions.exportUnique', {name: "Src IP.Port"}|s, "ipv6 separator");

    $sdId = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/mpls-basic.pcap"));
    $id = $sdId->{data}->[0]->{id};
    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/detail")->content;
    ok($sd =~ m|'sessions.exportUnique', {name: "Src IP:Port"}|s, "ipv4 separator");

# cyberchef
    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/cyberchef.html")->content;
    ok($sd =~ m{<base href="./cyberchef/" /><meta name="referrer" content="no-referrer">}s, "cyber chef header");
    ok($sd =~ m{else if \(param.startsWith\('session'\)\)}s, "cyber chef script");

    $sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/cyberchef/test/session/$id?type=src")->content;
    is ($sd, '{"data":"000100100a0100010000010c0f0300040a010001000100100a0100010000010d0f0300040a010001000100100a0100010000010e0f0300040a010001000100100a0100010000010f0f0300040a010001000100100a010001000001100f0300040a010001000100100a010001000001110f0300040a010001"}');

# http scheme
system ("cp pcap/wireshark-smb-on-windows10.pcap ../assets/scheme1.pcap");

my $suffix = int(rand()*100000);

my $cmd = "../capture/capture $ArkimeTest::es -c config.test.ini -n test -r http://localhost:8123/assets/scheme1.pcap --tag scheme-$suffix";
system($cmd);

$sdId = viewerGet("/sessions.json?date=-1&expression=" . uri_escape(qq(tags=scheme-$suffix&&communityId=="1:aMwepcNBDLDP6EYqeqpGpJEWZUo=")));

$id = $sdId->{data}->[0]->{id};
$encodedId = uri_escape($id);

$sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=false&ts=false&base=ascii")->content;
ok($sd =~ m{NETWORK PROGRAM 1.0}s);

unlink "../assets/scheme1.pcap";
esPost("/tests_files/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "term": { "name": "http://localhost:8123/assets/scheme1.pcap" } } }');

# Session with no packetPos / no fileId (e.g. session forwarded from a metadata-only
# source). /detail must tell the client to hidepackets, and /packets must not crash
# the viewer with TypeError: Cannot set properties of undefined (setting 'id').
my $noPcapTag = "no-pcap-detail-regression-tag";
my $noPcapDocId = "abcdef0123456789nopcap00";
my $noPcapIndex = "tests_sessions3-26m04";
my $noPcapDoc = '{
  "@timestamp": 1745000000000,
  "firstPacket": 1745000000000,
  "lastPacket": 1745000001000,
  "node": "test",
  "ipProtocol": 17,
  "source": {"ip": "10.99.99.10", "port": 1000, "bytes": 100, "packets": 1},
  "destination": {"ip": "10.99.99.20", "port": 53, "bytes": 100, "packets": 1},
  "network": {"packets": 2, "bytes": 200},
  "totDataBytes": 100,
  "length": 1,
  "protocol": ["udp"],
  "protocolCnt": 1,
  "tags": ["' . $noPcapTag . '"],
  "tagsCnt": 1
}';
esPost("/$noPcapIndex/_doc/$noPcapDocId?refresh=wait_for", $noPcapDoc);

my $noPcapList = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("tags=$noPcapTag"));
is (scalar @{$noPcapList->{data}}, 1, "no-pcap session indexed");
my $noPcapId = $noPcapList->{data}->[0]->{id};

$sd = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$noPcapId/detail")->content;
ok($sd =~ m{sessionid="\Q$noPcapId\E"}s, "no-pcap /detail rendered");
ok($sd =~ m{hidepackets="true"}, "no-pcap /detail sets hidepackets=\"true\"");

my $noPcapPackets = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$noPcapId/packets?line=false&ts=false&base=ascii");
is ($noPcapPackets->code, 200, "no-pcap /packets returns 200 (no crash)");
ok($noPcapPackets->content !~ m{Cannot set properties of undefined}, "no-pcap /packets does not throw TypeError");

esPost("/$noPcapIndex/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "term": { "tags": "' . $noPcapTag . '" } } }');
