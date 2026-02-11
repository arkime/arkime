use Test::More tests => 20;
use Cwd;
use URI::Escape;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

system("perl mini-s3.pl 4566 &");

sub run {
my ($tag, $compression, $extension, $gap) = @_;

    my $cmd = "../capture/capture -o s3GapPacketPos=$gap -c config.test.ini -n s3-test --copy -R pcap --tag $tag -o s3Compression=$compression";
    system($cmd);

    # Test 1
    my $json = countTest2(1, "date=-1&expression=" . uri_escape("tags=$tag && port.src == 54072"));
    my $id = $json->{data}->[0]->{id};
    my $content = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8124/api/session/s3/$id/packets?line=false&ts=false&base=ascii")->content;

    # Test string crosses 2 packets
    ok ($content =~ /domain in examples without prior coordination or asking for permission/);

    # Test 2
    $json = countTest2(1, "date=-1&expression=" . uri_escape("tags=$tag && ip.src == 172.17.96.143:59221"));
    $id = $json->{data}->[0]->{id};
    $content = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8124/api/session/s3/$id/packets?line=false&ts=false&base=ascii")->content;

    # Test string crosses 2 packets
    ok ($content =~ /NWIXML:notificationSequence xmlns:/);

    $json = viewerGet2('/api/files?length=10&start=0&filter=&sortField=num&desc=true');

    my @parts = split("/", $json->{data}->[0]->{name}, 4);
    splice @parts, 2, 1;
    my $s3url = join('/', @parts);
    unlink("/tmp/arkime.file.$compression.$gap");
    unlink("/tmp/arkime.file.$compression.$gap$extension");
    system("AWS_ACCESS_KEY_ID='foo'  AWS_SECRET_ACCESS_KEY='foo' aws --endpoint-url http://localhost:4566 s3 cp $s3url /tmp/arkime.file.$compression.$gap$extension");
}

my $value = int(rand()*1000000);

run("none-$value", "none", "", "true");
run("gzip-$value", "gzip", ".gz", "true");
run("zstd-$value", "zstd", ".zst", "false");

system("gzip -d /tmp/arkime.file.gzip.true.gz > /dev/null 2>&1");
system("zstd -d /tmp/arkime.file.zstd.false.zst > /dev/null 2>&1");

# Make sure all 3 downloads are the same
is (system("diff /tmp/arkime.file.none.true /tmp/arkime.file.zstd.false"), 0);
is (system("diff /tmp/arkime.file.none.true /tmp/arkime.file.gzip.true"), 0);

system("curl -s http://localhost:4566/_shutdown > /dev/null 2>&1");
