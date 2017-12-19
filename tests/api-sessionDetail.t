use Test::More tests => 19;

use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Data::Dumper;
use strict;

my $pwd = "*/pcap";

# new /detail api
    my $sdId = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/http-content-gzip.pcap"));
    my $id = $sdId->{data}->[0]->{id};

    my $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/detail")->content;
    ok($sd =~ m{sessionid.*\Q$id\E}s, "/detail");
    ok($sd =~ m{Tags.*md5taggertest1}s, "/detail Tags");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8125/test/session/$id/detail")->content;
    ok($sd =~ m{sessionid.*\Q$id\E}s, "multi /detail");
    ok($sd =~ m{Tags.*md5taggertest1}s, "multi /detail Tags");

# http
    #https://moloch.itsec.aol.com/daha/moloch-daha-mtc02/session/170905-nyoUb23L-XVNMY3GLBg2tcpp/packets?base=hex&decode=%7B%22BODY-UNXORBRUTEGZ%22:%7B%7D,%22BODY-UNBASE64%22:%7B%7D,%22BODY-UNXOR%22:%7B%22skip%22:%221%22%7D%7D&gzip=false&image=false&line=true&packets=50&ts=false
    #my $sdId = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/http-content-gzip.pcap"));
    #my $id = $sdId->{data}->[0]->{id};

    #col: 80:quic\r\n\r\n
    #636f6c3a2038303a71756963 then return representations then gzip header 08000000000002

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/packets?line=false&ts=false&base=natural")->content;
    ok(bin2hex($sd) =~ /636f6c3a2038303a71756963.*08000000000002/, "encoding:natural");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/packets?line=false&ts=false&base=ascii")->content;
    ok(bin2hex($sd) =~ /636f6c3a2038303a71756963.*08000000000002/, "encoding:ascii");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/packets?line=false&ts=false&base=hex")->content;
    ok($sd =~ /636f 6c3a 2038 303a 7175 6963 0d0a 0d0a.*col:.80:quic.*0800 0000 0000 02ff/s, "encoding:hex");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/packets?line=true&ts=false&base=hex")->content;
    ok($sd =~ /00000272:.*636f 6c3a 2038 303a 7175 6963 0d0a 0d0a.*col:.80:quic.*00000000:.*0800 0000 0000 02ff/s, "encoding:hex line:true");

# http gzip:true
    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/packets?line=false&ts=false&base=natural&gzip=true")->content;
    ok($sd =~ /col:.80:quic.*&lt;\?xml version=&quot;1.0&quot;\?&gt;/s, "encoding:natural gzip:true");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/packets?line=false&ts=false&base=ascii&gzip=true")->content;
    ok($sd =~ /col:.80:quic.*&lt;\?xml version=&quot;1.0&quot;\?&gt;/s, "encoding:ascii gzip:true");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/packets?line=false&ts=false&base=hex&gzip=true")->content;
    ok($sd =~ /636f 6c3a 2038 303a 7175 6963 0d0a 0d0a.*col:.80:quic.*3c3f 786d 6c20 7665 7273 696f 6e3d 2231.*&lt;\?xml.version=&quot;1/s, "encoding:hex gzip:true");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/packets?line=true&ts=false&base=hex&gzip=true")->content;
    ok($sd =~ /00000272:.*636f 6c3a 2038 303a 7175 6963 0d0a 0d0a.*col:.80:quic.*00000000:.*3c3f 786d 6c20 7665 7273 696f 6e3d 2231.*&lt;\?xml.version=&quot;1/s, "encoding:hex gzip:true");


# http image:true
    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/packets?line=false&ts=false&base=natural&image=true")->content;
    ok($sd =~ m{col:.80:quic.*\Q$id\E/body/file/1/crossdomain.xml.pellet}s, "encoding:natural image:true");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/packets?line=false&ts=false&base=ascii&image=true")->content;
    ok($sd =~ m{col:.80:quic.*\Q$id\E/body/file/1/crossdomain.xml.pellet}s, "encoding:ascii image:true");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/packets?line=false&ts=false&base=hex&image=true")->content;
    ok($sd =~ m{636f 6c3a 2038 303a 7175 6963 0d0a 0d0a.*col:.80:quic.*\Q$id\E/body/file/1/crossdomain.xml.pellet}s, "encoding:hex image:true");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/packets?line=true&ts=false&base=hex&image=true")->content;
    ok($sd =~ m{00000272:.*636f 6c3a 2038 303a 7175 6963 0d0a 0d0a.*col:.80:quic.*\Q$id\E/body/file/1/crossdomain.xml.pellet}s, "encoding:hex line:true image:true");

# smtp
    $sdId = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/smtp-zip.pcap"));
    $id = $sdId->{data}->[0]->{id};

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/packets?line=false&ts=false&base=natural&image=false")->content;
    ok($sd =~ /UEsDBAoAAAAAACdOkUME9yniBgAAAAYAAAAFABwAZmlsZTFVVAkAA2lksFJpZLBSdXgLAAEE/, "smtp encoding:natural image:false");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/packets?line=false&ts=false&base=natural&image=true")->content;
    ok($sd =~ m{href="test/\Q$id\E/body/file/1/a.zip.pellet">a.zip<\/a>}s, "smtp encoding:natural image:true");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test\/$id\/body\/file\/1\/a.zip.pellet")->content;
    ok(bin2hex($sd) eq "504b03040a0000000000274e914304f729e2060000000600000005001c0066696c653155540900036964b0526964b05275780b0001044c060000040204000066696c65310a504b03040a0000000000294e9143c7a404c9060000000600000005001c0066696c653255540900036e64b0526e64b05275780b0001044c060000040204000066696c65320a504b01021e030a0000000000274e914304f729e20600000006000000050018000000000001000000a4810000000066696c653155540500036964b05275780b0001044c0600000402040000504b01021e030a0000000000294e9143c7a404c90600000006000000050018000000000001000000a4814500000066696c653255540500036e64b05275780b0001044c0600000402040000504b05060000000002000200960000008a0000000000", "smtp zip file");
