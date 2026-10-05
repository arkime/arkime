use Test::More tests => 10;
use ArkimeTest;
use Cwd;
use URI::Escape;
use strict;

# A sensor viewer whose elasticsearch is esProxy, started just for this test.
# Not --regressionTests, since that makes searches scroll which a sensor viewer
# doesn't do. Everything it serves should match the 8123 viewer byte for byte.

my $port = 8139;
my $pwd = getcwd() . "/pcap";

my $pid = fork();
die "fork: $!" if !defined $pid;
if ($pid == 0) {
    chdir('../viewer') or die "chdir ../viewer: $!";
    open(STDOUT, '>', '/dev/null');
    open(STDERR, '>&', STDOUT);
    my @args = ('viewer.js', '-c', '../tests/config.test.ini', '-n', 'test',
        '-o', "elasticsearch=http://test:test\@$ArkimeTest::host:7200",
        '-o', "usersElasticsearch=$ArkimeTest::usersElasticsearch",
        '-o', "viewPort=$port", '-o', 'authMode=anonymous', '-o', 'cronQueries=false');
    push @args, $ENV{INSECURE} if defined $ENV{INSECURE} && $ENV{INSECURE} ne '';
    exec('node', @args);
    die "exec: $!";
}
END { kill 'TERM', $pid if $pid; }

waitFor($ArkimeTest::host, $port, 1);

sub both {
    my ($url) = @_;
    my $direct = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123$url");
    my $proxied = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:$port$url");
    return ($direct, $proxied);
}

my $json = viewerGet("/api/sessions?date=-1&fields=rootId&expression=" . uri_escape("file=$pwd/http-content-gzip.pcap"));
my $id = $json->{data}->[0]->{id};
ok ($id, "found a session");

foreach my $what ("pcap", "packets", "detail") {
    my ($direct, $proxied) = both("/api/session/test/$id/$what");
    is ($proxied->code, 200, "session $what through esProxy");
    ok ($proxied->content ne '' && $proxied->content eq $direct->content, "session $what through esProxy matches") or
        diag("direct " . length($direct->content) . " bytes, esProxy " . length($proxied->content) . " bytes");
}

$json = viewerGet("/api/sessions?date=-1&fields=rootId&expression=" . uri_escape("file=$pwd/cloudshark-bgp-md5.pcap"));
my $rootId = $json->{data}->[0]->{rootId};
ok ($rootId, "found a session with a rootId");

my ($direct, $proxied) = both("/api/session/entire/test/$rootId/pcap");
is ($proxied->code, 200, "entire pcap through esProxy");
ok ($proxied->content ne '' && $proxied->content eq $direct->content, "entire pcap through esProxy matches") or
    diag("direct " . length($direct->content) . " bytes, esProxy " . length($proxied->content) . " bytes");
