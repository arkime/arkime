use Test::More tests => 38;
use Cwd;
use URI::Escape;
use MolochTest;
use Data::Dumper;
use Test::Differences;
use JSON;
use strict;

my $copytest = getcwd() . "/copytest.pcap";
my $token = getTokenCookie();

countTest(0, "date=-1&expression=" . uri_escape("file=$copytest"));

system("../db/db.pl --prefix tests $MolochTest::elasticsearch rm $copytest 2>&1 1>/dev/null");
viewerPost("/regressionTests/flushCache");
system("/bin/cp pcap/socks-http-example.pcap copytest.pcap");
system("../capture/capture -c config.test.ini -n test -r copytest.pcap");
esGet("/_flush");
esGet("/_refresh");

countTest(3, "date=-1&expression=" . uri_escape("file=$copytest"));

    my $idQuery = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("file=$copytest"));

# Get png
    my $hit = $idQuery->{data}->[0];
    my $png1 = $MolochTest::userAgent->get("http://$MolochTest::host:8123/api/session/raw/$hit->{node}/$hit->{id}.png");
    my $png2 = $MolochTest::userAgent->get("http://$MolochTest::host:8124/api/session/raw/$hit->{node}/$hit->{id}.png");
    my $pngm = $MolochTest::userAgent->get("http://$MolochTest::host:8125/api/session/raw/$hit->{node}/$hit->{id}.png");

    eq_or_diff($png1->content, $png2->content);
    eq_or_diff($png1->content, $pngm->content);

# scrub 1 id
    viewerPostToken("/delete?removePcap=true&removeSpi=false&date=-1", "ids=" . $idQuery->{data}->[0]->{id}, $token);

    esGet("/_flush");
    esGet("/_refresh");

    countTest(1, "date=-1&expression=" . uri_escape("file=$copytest&&scrubbed.by==anonymous"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$copytest&&scrubbed.by!=anonymous"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$copytest&&scrubbed.by==Anonymous"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$copytest&&scrubbed.by==[Anonymous]"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$copytest&&scrubbed.by!=[Anonymous]"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$copytest&&scrubbed.by==Anon*mous"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$copytest&&scrubbed.by==/Anon.*mous/"));

# get png
    $png1 = $MolochTest::userAgent->get("http://$MolochTest::host:8123/api/session/raw/$hit->{node}/$hit->{id}.png");
    $png2 = $MolochTest::userAgent->get("http://$MolochTest::host:8124/api/session/raw/$hit->{node}/$hit->{id}.png");
    $pngm = $MolochTest::userAgent->get("http://$MolochTest::host:8125/api/session/raw/$hit->{node}/$hit->{id}.png");

    eq_or_diff($png1->content, $png2->content);
    eq_or_diff($png1->content, $pngm->content);

# scrub expression
    viewerPostToken("/api/delete?removePcap=true&removeSpi=false&date=-1&expression=" . uri_escape("file=$copytest"), {}, $token);

    esGet("/_flush");
    esGet("/_refresh");

    countTest(3, "date=-1&expression=" . uri_escape("file=$copytest&&scrubbed.by==anonymous"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$copytest&&scrubbed.by!=anonymous"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$copytest&&scrubbed.by==Anonymous"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$copytest&&scrubbed.by==[Anonymous]"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$copytest&&scrubbed.by!=[Anonymous]"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$copytest&&scrubbed.by==Anon*mous"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$copytest&&scrubbed.by==/Anon.*mous/"));

# delete
    viewerPostToken("/delete?removePcap=true&removeSpi=true&date=-1&expression=" . uri_escape("file=$copytest"), {}, $token);

    esGet("/_flush");
    esGet("/_refresh");

    countTest(0, "date=-1&expression=" . uri_escape("file=$copytest"));

# cleanup
    unlink("copytest.pcap");
    system("../db/db.pl --prefix tests $MolochTest::elasticsearch rm $copytest 2>&1 1>/dev/null");
    viewerPost("/regressionTests/flushCache");
    esGet("/_flush");
    esGet("/_refresh");
