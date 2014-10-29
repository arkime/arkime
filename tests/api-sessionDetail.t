use Test::More tests => 12;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use strict;

my $pwd = getcwd() . "/pcap";

# sessionDetail
    my $sdId = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/http-content-gzip.pcap"));

    my $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/" . $sdId->{aaData}->[0]->{id} . "/sessionDetail?line=false&ts=false&base=natural")->content;
    ok(bin2hex($sd) =~ /636f6c3a2038303a717569633c62723e3c62723e1fefbfbd08000000000002efbfbd6cefbfbdefbfbd0eefbfbd2018efbfb/, "encoding:natural");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/" . $sdId->{aaData}->[0]->{id} . "/sessionDetail?line=false&ts=false&base=ascii")->content;
    ok(bin2hex($sd) =~ /636f6c3a2038303a717569630d0a0d0a1fc28b08000000000002c3bf6cc2/, "encoding:ascii");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/" . $sdId->{aaData}->[0]->{id} . "/sessionDetail?line=false&ts=false&base=hex")->content;
    ok(bin2hex($sd) =~ /636f6c3a2e38303a717569632e2e2e2e0a316638622030383030203030303020303030302030326666203663386520623130652063323230202e2e2e2e2e2e2e2e2e2e6c2e2e2e2e2e0a313838342037373965203032313920346430302035643064206335343120336231612034646461202e2e772e2e2e4d2e5d2e2e413b2e4d2e0a/, "encoding:hex");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/" . $sdId->{aaData}->[0]->{id} . "/sessionDetail?line=true&ts=false&base=hex")->content;
    ok(bin2hex($sd) =~ /636f6c3a2e38303a717569632e2e2e2e0a3c7370616e20636c6173733d2273657373696f6e6c6e223e30303030303238383a3c2f7370616e3e20316638622030383030203030303020303030302030326666203663386520623130652063323230202e2e2e2e2e2e2e2e2e2e6c2e2e2e2e2e0a3c7370616e20636c6173733d2273657373696f6e6c6e223e30303030303330343a3c2f7370616e3e20313838342037373965203032313920346430302035643064206335343120336231612034646461202e2e772e2e2e4d2e5d2e2e413b2e4d2e0a/, "encoding:hex line:true");

# sessionDetail gzip:true
    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/" . $sdId->{aaData}->[0]->{id} . "/sessionDetail?line=false&ts=false&base=natural&gzip=true")->content;
    ok(bin2hex($sd) =~ /636f6c3a2038303a717569633c62723e3c62723e266c743b3f786d6c2076657273696f6e3d2671756f743b312e302671756f/, "encoding:natural gzip:true");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/" . $sdId->{aaData}->[0]->{id} . "/sessionDetail?line=false&ts=false&base=ascii&gzip=true")->content;
    ok(bin2hex($sd) =~ /636f6c3a2038303a717569630d0a0d0a266c743b3f786d6c2076657273696f6e3d2671756f743b312e302671756f/, "encoding:ascii gzip:true");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/" . $sdId->{aaData}->[0]->{id} . "/sessionDetail?line=false&ts=false&base=hex&gzip=true")->content;
    ok(bin2hex($sd) =~ /636f6c3a2e38303a717569632e2e2e2e0a33633366203738366420366332302037363635203732373320363936662036653364203232333120266c743b3f786d6c2e76657273696f6e3d2671756f743b310a326533302032323366203365306120336332312034343466203433353420353935302034353230202e302671756f/, "encoding:hex gzip:true");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/" . $sdId->{aaData}->[0]->{id} . "/sessionDetail?line=true&ts=false&base=hex&gzip=true")->content;
    ok(bin2hex($sd) =~ /636f6c3a2e38303a717569632e2e2e2e0a3c7370616e20636c6173733d2273657373696f6e6c6e223e30303030303238383a3c2f7370616e3e2033633366203738366420366332302037363635203732373320363936662036653364203232333120266c743b3f786d6c2e76657273696f6e3d2671756f743b310a3c7370616e20636c6173733d2273657373696f6e6c6e223e30303030303330343a3c2f7370616e3e20326533302032323366203365306120336332312034343466203433353420353935302034353230202e302671756f/, "encoding:hex line:true gzip:true");

# sessionDetail image:true
    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/" . $sdId->{aaData}->[0]->{id} . "/sessionDetail?line=false&ts=false&base=natural&image=true")->content;
    ok(bin2hex($sd) =~ /636f6c3a2038303a717569633c62723e3c62723e3c6120636c6173733d27696d6167657461672720687265663d22746573742f313430383035/, "encoding:natural image:true");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/" . $sdId->{aaData}->[0]->{id} . "/sessionDetail?line=false&ts=false&base=ascii&image=true")->content;
    ok(bin2hex($sd) =~ /636f6c3a2038303a717569630d0a0d0a3c2f7072653e3c6120636c6173733d27696d6167657461672720687265663d22746573742f313430383035/, "encoding:ascii image:true");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/" . $sdId->{aaData}->[0]->{id} . "/sessionDetail?line=false&ts=false&base=hex&image=true")->content;
    ok(bin2hex($sd) =~ /636f6c3a2e38303a717569632e2e2e2e0a3c2f7072653e3c6120636c6173733d27696d6167657461672720687265663d22746573742f313430383035/, "encoding:hex image:true");

    $sd = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/" . $sdId->{aaData}->[0]->{id} . "/sessionDetail?line=true&ts=false&base=hex&image=true")->content;
    ok(bin2hex($sd) =~ /636f6c3a2e38303a717569632e2e2e2e0a3c2f7072653e3c6120636c6173733d27696d6167657461672720687265663d22746573742f313430383035/, "encoding:hex line:true image:true");

