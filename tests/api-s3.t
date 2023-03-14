use Test::More;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $s3AccessKeyId=$ENV{s3AccessKeyId};
my $s3SecretAccessKey=$ENV{s3SecretAccessKey};

if($s3AccessKeyId eq "" || $s3SecretAccessKey eq "") {
    plan skip_all => 'S3 info not set';
} else {
    plan tests => 18;
}

sub run {
my ($tag, $compression) = @_;

    my $cmd = "../capture/capture -o 's3AccessKeyId=$s3AccessKeyId' -o 's3SecretAccessKey=$s3SecretAccessKey' -c config.test.ini -n s3 --copy -R pcap --tag $tag -o s3Compression=$compression";
    system($cmd);

    # Test 1
    my $json = countTest2(1, "date=-1&expression=" . uri_escape("tags=$tag && port.src == 54072"));
    my $id = $json->{data}->[0]->{id};
    my $content = $MolochTest::userAgent->get("http://$MolochTest::host:8124/s3/session/$id/packets?line=false&ts=false&base=ascii")->content;

    # Test string crosses 2 packets
    ok ($content =~ /domain in examples without prior coordination or asking for permission/);

    # Test 2
    $json = countTest2(1, "date=-1&expression=" . uri_escape("tags=$tag && ip.src == 172.17.96.143:59221"));
    $id = $json->{data}->[0]->{id};
    $content = $MolochTest::userAgent->get("http://$MolochTest::host:8124/s3/session/$id/packets?line=false&ts=false&base=ascii")->content;

    # Test string crosses 2 packets
    ok ($content =~ /NWIXML:notificationSequence xmlns:/);
}

my $value = int(rand()*1000000);

run("none-$value", "none");
run("gzip-$value", "gzip");
run("zstd-$value", "zstd");
