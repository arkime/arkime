use Test::More;
use Cwd;
use URI::Escape;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $s3AccessKeyId=$ENV{s3AccessKeyId};
my $s3SecretAccessKey=$ENV{s3SecretAccessKey};

if($s3AccessKeyId eq "" || $s3SecretAccessKey eq "") {
    plan skip_all => 'S3 info not set';
} else {
    plan tests => 20;
}

sub run {
my ($tag, $compression, $extension, $gap) = @_;

    my $cmd = "../capture/capture -o 's3AccessKeyId=$s3AccessKeyId' -o 's3SecretAccessKey=$s3SecretAccessKey' -o s3GapPacketPos=$gap -c config.test.ini -n s3 --copy -R pcap --tag $tag -o s3Compression=$compression";
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
    system("AWS_ACCESS_KEY_ID='$s3AccessKeyId'  AWS_SECRET_ACCESS_KEY='$s3SecretAccessKey' aws s3 cp $s3url /tmp/arkime.file.$compression.$gap$extension");
}

my $value = int(rand()*1000000);

run("none-$value", "none", "", "true");
run("gzip-$value", "gzip", ".gz", "true");
run("zstd-$value", "zstd", ".zst", "false");

system("gzip -d /tmp/arkime.file.gzip.true.gz");
system("zstd -d /tmp/arkime.file.zstd.false.zst");

# Make sure all 3 downloads are the same
is (system("diff /tmp/arkime.file.none.true /tmp/arkime.file.zstd.false"), 0);
is (system("diff /tmp/arkime.file.none.true /tmp/arkime.file.gzip.true"), 0);
