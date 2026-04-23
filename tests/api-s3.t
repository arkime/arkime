use Test::More tests => 23;
use Cwd;
use URI::Escape;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

system("perl mini-aws.pl --debug 4566 > /tmp/arkime.s3 &");
my $nodeFilter = '{ "query": { "terms": { "node": ["s3-test", "sqs-test"] } } }';
esPost("/tests2_sessions*/_delete_by_query?conflicts=proceed&refresh", $nodeFilter);

sub run {
my ($tag, $compression, $extension, $gap) = @_;

    my $cmd = "../capture/capture -o disablePython=true -o s3GapPacketPos=$gap -c config.test.ini -n s3-test --copy -R pcap --tag $tag -o s3Compression=$compression --flush > /tmp/arkime.capture.$tag.log 2>&1";
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

# --- SQS end-to-end: bucket-notification -> SQS queue -> capture reads from SQS ---
my $sqsvalue = int(rand() * 1000000);
my $reftag   = "sqsref-$sqsvalue";
my $sqstag   = "sqs-$sqsvalue";

# Reference: ingest the pcap directly to learn expected session count.
system("../capture/capture -o disablePython=true -c config.test.ini -n s3-test -R pcap/bt-tcp.pcap --tag $reftag > /tmp/arkime.capture.sqsref.log 2>&1");
my $refjson = viewerGet2("/sessions.json?date=-1&expression=" . uri_escape("tags=$reftag"));
my $expected = $refjson->{recordsFiltered};
ok ($expected > 0, "SQS reference pcap yielded $expected sessions");

# Configure S3 -> SQS bucket notification using the real AWS API shape.
system(qq(curl -s -X PUT "http://localhost:4566/sqsbucket?notification" --data-binary '<NotificationConfiguration><QueueConfiguration><Queue>arn:aws:sqs:us-east-1:000000000000:sqsqueue</Queue><Event>s3:ObjectCreated:*</Event></QueueConfiguration></NotificationConfiguration>' > /dev/null));

# Upload the pcap using the real AWS CLI; mini-aws will publish an S3 event to SQS.
my $s3key = "bt-tcp-$sqsvalue.pcap";
system("AWS_ACCESS_KEY_ID=foo AWS_SECRET_ACCESS_KEY=foo aws --endpoint-url http://localhost:4566 s3 cp pcap/bt-tcp.pcap s3://sqsbucket/$s3key > /dev/null 2>&1");

# Run capture pointing at the SQS queue; it should receive the event, fetch the
# pcap back from S3 (mini-aws), index it, and delete the SQS message.
system("../capture/capture -o disablePython=true -c config.test.ini -n sqs-test --tag $sqstag -r sqshttp://127.0.0.1:4566/000000000000/sqsqueue > /tmp/arkime.capture.sqs.log 2>&1");

countTest2($expected, "date=-1&expression=" . uri_escape("tags=$sqstag"));

system("curl -s http://localhost:4566/_shutdown > /dev/null 2>&1");

esPost("/tests2_sessions*/_delete_by_query?conflicts=proceed&refresh", $nodeFilter);
