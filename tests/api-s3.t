use Test::More tests => 75;
use Cwd;
use URI::Escape;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;
use Time::HiRes ();

# Without a region the aws cli asks the instance metadata service on every call, which hangs where it isn't reachable
my $aws = "AWS_EC2_METADATA_DISABLED=true AWS_DEFAULT_REGION=us-east-1 aws --endpoint-url http://localhost:4566";

# A hung command's report also says what the mock aws last did and whether it still answers
$ArkimeTest::hungExtra = sub {
    diag "--- last of /tmp/arkime.aws\n" . `tail -80 /tmp/arkime.aws 2>&1` if (-s "/tmp/arkime.aws");
    diag "--- mock aws answers with: " . `curl -s -m 5 -o /dev/null -w '%{http_code}' http://localhost:4566/_auth_seen 2>&1`;
    diag "--- connections to the mock\n" . `ss -tnp 2>/dev/null | grep 4566`;
};

sessionsDeleteByNode("tests2_sessions3-*", "s3-test", "sqs-test");

# Also runs when a hung command ends the test, so its sessions don't change the multi viewer tests
END {
    my $died = $?;
    local $?;
    # a killed capture's last bulk can still be landing
    sleep(2) if ($died);
    sessionsDeleteByNode("tests2_sessions3-*", "s3-test", "sqs-test");
}

sub run {
my ($tag, $compression, $extension, $gap, $input) = @_;

    my $cmd = "../capture/capture $ArkimeTest::es -o disablePython=true -o s3GapPacketPos=$gap -c config.test.ini -n s3-test --copy $input --tag $tag -o s3Compression=$compression --flush > /tmp/arkime.capture.$tag.log 2>&1";
    timedSystem($cmd);

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
    timedSystem("AWS_ACCESS_KEY_ID='foo'  AWS_SECRET_ACCESS_KEY='foo' $aws s3 cp $s3url /tmp/arkime.file.$compression.$gap$extension");
    return $s3url;
}

my $value = int(rand()*1000000);

my $files = "-r pcap/socks-http-pass.pcap -r pcap/wireshark-retrans.pcap";

run("none-$value", "none", "",     "true",  $files);
run("gzip-$value", "gzip", ".gz",  "true",  $files);
my $s3url = run("zstd-$value", "zstd", ".zst", "false", $files);

timedSystem("gzip -d /tmp/arkime.file.gzip.true.gz > /dev/null 2>&1");
timedSystem("zstd -d /tmp/arkime.file.zstd.false.zst > /dev/null 2>&1");

# Make sure all 3 downloads are the same
is (timedSystem("diff /tmp/arkime.file.none.true /tmp/arkime.file.zstd.false"), 0);
is (timedSystem("diff /tmp/arkime.file.none.true /tmp/arkime.file.gzip.true"), 0);

# --- SQS end-to-end: bucket-notification -> SQS queue -> capture reads from SQS ---
my $sqsvalue = int(rand() * 1000000);
my $reftag   = "sqsref-$sqsvalue";
my $sqstag   = "sqs-$sqsvalue";

# Reference: ingest the pcap directly to learn expected session count.
timedSystem("../capture/capture $ArkimeTest::es -o disablePython=true -c config.test.ini -n s3-test -R pcap/bt-tcp.pcap --tag $reftag > /tmp/arkime.capture.sqsref.log 2>&1");
my $refjson = viewerGet2("/sessions.json?date=-1&expression=" . uri_escape("tags=$reftag"));
my $expected = $refjson->{recordsFiltered};
ok ($expected > 0, "SQS reference pcap yielded $expected sessions");

# Configure S3 -> SQS bucket notification using the real AWS API shape.
timedSystem(qq(curl -s -X PUT "http://localhost:4566/sqsbucket?notification" --data-binary '<NotificationConfiguration><QueueConfiguration><Queue>arn:aws:sqs:us-east-1:000000000000:sqsqueue</Queue><Event>s3:ObjectCreated:*</Event></QueueConfiguration></NotificationConfiguration>' > /dev/null));

# Upload the pcap using the real AWS CLI; mini-aws will publish an S3 event to SQS.
my $s3key = "bt-tcp-$sqsvalue.pcap";
timedSystem("AWS_ACCESS_KEY_ID=foo AWS_SECRET_ACCESS_KEY=foo $aws s3 cp pcap/bt-tcp.pcap s3://sqsbucket/$s3key > /dev/null 2>&1");

# Run capture pointing at the SQS queue; it should receive the event, fetch the
# pcap back from S3 (mini-aws), index it, and delete the SQS message.
timedSystem("../capture/capture $ArkimeTest::es -o disablePython=true -c config.test.ini -n sqs-test --tag $sqstag -r sqshttp://127.0.0.1:4566/000000000000/sqsqueue > /tmp/arkime.capture.sqs.log 2>&1");

countTest2($expected, "date=-1&expression=" . uri_escape("tags=$sqstag"));

# --- S3 keys with URL/XML special characters must be encoded in the request path ---
my $specialDir = "special-$sqsvalue";
my $specialKey = "$specialDir/a?b&c d+e#f%g'h.pcap";
my $encodedKey = "$specialDir/a%3Fb%26c%20d%2Be%23f%25g%27h.pcap";

# From a bucket listing (XML escaped keys)
timedSystem(qq(AWS_ACCESS_KEY_ID=foo AWS_SECRET_ACCESS_KEY=foo $aws s3 cp pcap/bt-tcp.pcap "s3://testbucket/$specialKey" > /dev/null 2>&1));
my $listtag = "s3list-$sqsvalue";
timedSystem("../capture/capture $ArkimeTest::es -o disablePython=true -c config.test.ini -n sqs-test --tag $listtag -r s3http://127.0.0.1:4566/testbucket/$specialDir/ > /tmp/arkime.capture.s3list.log 2>&1");
countTest2($expected, "date=-1&expression=" . uri_escape("tags=$listtag"));

esGet("/tests2_files/_refresh");
my $fjson = clusterSearch(8124, "files", {where => {name => "s3http://127.0.0.1:4566/testbucket/$encodedKey"}});
is ($fjson->{hits}->{total}, 1, "files record name has the encoded key");
is ($fjson->{hits}->{hits}->[0]->{_source}->{extra}->{path}, "/$specialKey", "files record extra.path has the raw key");

# A listing prefix with special characters
timedSystem(qq(AWS_ACCESS_KEY_ID=foo AWS_SECRET_ACCESS_KEY=foo $aws s3 cp pcap/bt-tcp.pcap "s3://testbucket/pre fix&$sqsvalue/x.pcap" > /dev/null 2>&1));
my $prefixtag = "s3prefix-$sqsvalue";
timedSystem("../capture/capture $ArkimeTest::es -o disablePython=true -c config.test.ini -n sqs-test --tag $prefixtag -r 's3http://127.0.0.1:4566/testbucket/pre%20fix%26$sqsvalue/' > /tmp/arkime.capture.s3prefix.log 2>&1");
countTest2($expected, "date=-1&expression=" . uri_escape("tags=$prefixtag"));

# From an SQS bucket notification (URL encoded keys)
my $sqsspecialtag = "sqsspecial-$sqsvalue";
timedSystem(qq(AWS_ACCESS_KEY_ID=foo AWS_SECRET_ACCESS_KEY=foo $aws s3 cp pcap/bt-tcp.pcap "s3://sqsbucket/$specialKey" > /dev/null 2>&1));
timedSystem("../capture/capture $ArkimeTest::es -o disablePython=true -c config.test.ini -n sqs-test --tag $sqsspecialtag -r sqshttp://127.0.0.1:4566/000000000000/sqsqueue > /tmp/arkime.capture.sqsspecial.log 2>&1");
countTest2($expected, "date=-1&expression=" . uri_escape("tags=$sqsspecialtag"));

# A key with .. segments must not reach another bucket
my $travtag = "sqstrav-$sqsvalue";
timedSystem(qq(AWS_ACCESS_KEY_ID=foo AWS_SECRET_ACCESS_KEY=foo $aws s3 cp pcap/bt-tcp.pcap "s3://testbucket/$specialDir/plain.pcap" > /dev/null 2>&1));
timedSystem("curl -s -X POST 'http://localhost:4566/_sqs_enqueue/000000000000/sqsqueue?bucket=sqsbucket&key=" . uri_escape("x/../../testbucket/$specialDir/plain.pcap") . "' > /dev/null");
timedSystem("../capture/capture $ArkimeTest::es -o disablePython=true -c config.test.ini -n sqs-test --tag $travtag -r sqshttp://127.0.0.1:4566/000000000000/sqsqueue > /tmp/arkime.capture.sqstrav.log 2>&1");
countTest2(0, "date=-1&expression=" . uri_escape("tags=$travtag"));
is (timedSystem("grep -q 'Invalid S3 key' /tmp/arkime.capture.sqstrav.log"), 0, "traversal key rejected");

# --- SQS bucket names must not change the S3 host or path ---
my $badtag = "sqsbadbucket-$sqsvalue";
foreach my $bucket ("evil.example#", "localhost?", "a/../testbucket") {
    timedSystem("curl -s -X POST 'http://localhost:4566/_sqs_enqueue/000000000000/sqsqueue?bucket=" . uri_escape($bucket) . "&key=" . uri_escape("$specialDir/plain.pcap") . "' > /dev/null");
}
timedSystem("../capture/capture $ArkimeTest::es -o disablePython=true -c config.test.ini -n sqs-test --tag $badtag -r sqshttp://127.0.0.1:4566/000000000000/sqsqueue > /tmp/arkime.capture.sqsbadbucket.log 2>&1");
countTest2(0, "date=-1&expression=" . uri_escape("tags=$badtag"));
is (`grep -c 'Invalid S3 bucket' /tmp/arkime.capture.sqsbadbucket.log` + 0, 3, "bad SQS buckets rejected");

timedSystem(qq(../capture/capture $ArkimeTest::es -o disablePython=true -c config.test.ini -n s3-test -r "s3://evil.example#/$specialDir/plain.pcap" > /tmp/arkime.capture.s3badbucket.log 2>&1));
is (timedSystem("grep -q 'Invalid S3 bucket' /tmp/arkime.capture.s3badbucket.log"), 0, "bad s3:// bucket rejected");

# --- aws credentials chain (EKS IRSA, Pod Identity, env, ECS) from mini-aws, with the config keys blanked ---
my $eksvalue = int(rand() * 1000000);
my $eksBase = "env -u ECS_CONTAINER_METADATA_URI_V4 -u AWS_ACCESS_KEY_ID -u AWS_SECRET_ACCESS_KEY -u AWS_SESSION_TOKEN -u AWS_PROFILE -u AWS_ROLE_ARN -u AWS_WEB_IDENTITY_TOKEN_FILE " .
              "-u AWS_CONTAINER_CREDENTIALS_FULL_URI -u AWS_CONTAINER_CREDENTIALS_RELATIVE_URI -u AWS_CONTAINER_AUTHORIZATION_TOKEN_FILE " .
              "-u AWS_CONTAINER_AUTHORIZATION_TOKEN AWS_SHARED_CREDENTIALS_FILE=/nonexistent AWS_CONFIG_FILE=/nonexistent";
my $podEnv = "AWS_CONTAINER_CREDENTIALS_FULL_URI=http://127.0.0.1:4566/_eks/credentials AWS_CONTAINER_AUTHORIZATION_TOKEN_FILE";
my $irsaEnv = "AWS_ROLE_ARN=arn:aws:iam::000000000000:role/arkime AWS_ENDPOINT_URL_STS=http://127.0.0.1:4566 AWS_WEB_IDENTITY_TOKEN_FILE";
my $blankOpts = "-o s3AccessKeyId= -o s3SecretAccessKey=";
my %eksTokens = (pod => "mini-aws-pod-identity-token", web => "mini.aws-web+identity/token=", bad => "bad-token");
foreach my $t (keys %eksTokens) {
    open(my $fh, '>', "/tmp/arkime.eks.$t") or die "Can't write /tmp/arkime.eks.$t";
    print $fh "$eksTokens{$t}\n";
    close($fh);
}

sub authSeen {
my ($reset) = @_;
    my $url = "http://localhost:4566/_auth_seen";
    return from_json(($reset ? $ArkimeTest::userAgent->delete($url) : $ArkimeTest::userAgent->get($url))->content);
}

# Writer uploads with the EKS credentials, then the viewer reads the pcap back
sub eksWriter {
my ($name, $env, $opts, $key, $token) = @_;
    my $tag = "eks$name-$eksvalue";
    authSeen(1);
    timedSystem("$eksBase $env ../capture/capture $ArkimeTest::es -o disablePython=true -c config.test.ini -n s3-test $opts --copy -r pcap/socks-http-pass.pcap --tag $tag --flush > /tmp/arkime.capture.$tag.log 2>&1");
    my $seen = authSeen();
    ok (($seen->{keys}->{$key}->{$token} // 0) > 0, "s3 writer signed with $name credentials");

    my $json = countTest2(1, "date=-1&expression=" . uri_escape("tags=$tag && port.src == 54072"));
    my $content = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8124/api/session/s3/$json->{data}->[0]->{id}/packets?line=false&ts=false&base=ascii")->content;
    ok ($content =~ /domain in examples without prior coordination or asking for permission/, "$name written pcap readable");
    return $seen;
}

eksWriter("pod", "$podEnv=/tmp/arkime.eks.pod", $blankOpts, "ASIAMINIPODIDENTITY", "mini/pod+session+token=");
# An empty node setting from the environment is the same as unset
my $seen = eksWriter("irsa", "$irsaEnv=/tmp/arkime.eks.web ARKIME_s3DASHtest__s3AccessKeyId= ARKIME_s3DASHtest__s3SecretAccessKey=", "", "ASIAMINIIRSA", "mini/irsa+session+token=");
is ($seen->{sts}->{"arkime-s3-test"}, 1, "STS called once with the node session name");
# The writer uses the same aws credentials chain as the readers, so env credentials work too
eksWriter("env", "AWS_ACCESS_KEY_ID=AKIAMINIENV AWS_SECRET_ACCESS_KEY=mini/env+secret= AWS_SESSION_TOKEN=mini/env+session+token=", $blankOpts, "AKIAMINIENV", "mini/env+session+token=");
# ECS container credentials are found without s3UseECSEnv, and don't get an IMDSv2 token request
$seen = eksWriter("ecs", "ECS_CONTAINER_METADATA_URI_V4=http://127.0.0.1:4566/v4/mini AWS_CONTAINER_CREDENTIALS_RELATIVE_URI=/_ecs/credentials", "$blankOpts -o s3UseECSEnv=true", "ASIAMINIECS", "mini/ecs+session+token=");
is ($seen->{imdsToken} // 0, 0, "no IMDSv2 token request to the ECS endpoint");
is (timedSystem("grep -q 's3UseECSEnv is no longer needed' /tmp/arkime.capture.eksecs-$eksvalue.log"), 0, "s3UseECSEnv warns it isn't needed");

# s3 reader through the aws credentials chain
my $ekskey = "eks-$eksvalue.pcap";
timedSystem("AWS_ACCESS_KEY_ID=foo AWS_SECRET_ACCESS_KEY=foo $aws s3 cp pcap/bt-tcp.pcap s3://testbucket/$ekskey > /dev/null 2>&1");

sub eksReader {
my ($name, $env, $opts, $key, $token) = @_;
    my $tag = "eksread$name-$eksvalue";
    authSeen(1);
    timedSystem("$eksBase $env ../capture/capture $ArkimeTest::es -o disablePython=true -c config.test.ini -n sqs-test $opts --tag $tag -r s3http://127.0.0.1:4566/testbucket/$ekskey > /tmp/arkime.capture.$tag.log 2>&1");
    ok ((authSeen()->{keys}->{$key}->{$token} // 0) > 0, "s3 reader signed with $name credentials");
    countTest2($expected, "date=-1&expression=" . uri_escape("tags=$tag"));
}

eksReader("pod", "$podEnv=/tmp/arkime.eks.pod ARKIME_sqsDASHtest__s3AccessKeyId= ARKIME_sqsDASHtest__s3SecretAccessKey=", "", "ASIAMINIPODIDENTITY", "mini/pod+session+token=");
eksReader("irsa", "$irsaEnv=/tmp/arkime.eks.web", $blankOpts, "ASIAMINIIRSA", "mini/irsa+session+token=");

# The s3 writer's s3AccessKeyId and the sqs reader's sqsAccessKeyId are kept apart, even though the writer starts first
my $sqsekstag = "ekssqs-$eksvalue";
timedSystem("AWS_ACCESS_KEY_ID=foo AWS_SECRET_ACCESS_KEY=foo $aws s3 cp pcap/bt-tcp.pcap s3://sqsbucket/$sqsekstag.pcap > /dev/null 2>&1");
authSeen(1);
timedSystem("$eksBase ../capture/capture $ArkimeTest::es -o disablePython=true -c config.test.ini -n s3-test -o sqsAccessKeyId=AKIAMINISQS -o sqsSecretAccessKey=mini/sqs+secret= --copy --tag $sqsekstag -r sqshttp://127.0.0.1:4566/000000000000/sqsqueue > /tmp/arkime.capture.$sqsekstag.log 2>&1");
$seen = authSeen();
ok (($seen->{keys}->{AKIAMINISQS}->{""} // 0) > 0, "sqs reader signed with sqsAccessKeyId");
ok (($seen->{keys}->{foo}->{""} // 0) > 0, "s3 writer and reader signed with s3AccessKeyId");
countTest2($expected, "date=-1&expression=" . uri_escape("tags=$sqsekstag"));

# Bad tokens exit with the error from STS or the Pod Identity agent
my $badlog = "/tmp/arkime.capture.eksbad-$eksvalue.log";
isnt (timedSystem("$eksBase $irsaEnv=/tmp/arkime.eks.bad ../capture/capture $ArkimeTest::es -o disablePython=true -c config.test.ini -n s3-test $blankOpts --copy -r pcap/socks-http-pass.pcap > $badlog 2>&1"), 0, "s3 writer exits with a bad web identity token");
is (timedSystem("grep -q 'InvalidIdentityToken' $badlog"), 0, "STS error logged");
isnt (timedSystem("$eksBase $podEnv=/tmp/arkime.eks.bad ../capture/capture $ArkimeTest::es -o disablePython=true -c config.test.ini -n sqs-test $blankOpts -r s3http://127.0.0.1:4566/testbucket/$ekskey > $badlog 2>&1"), 0, "s3 reader exits with a bad pod identity token");
is (timedSystem("grep -q 'Container credentials request failed 401' $badlog"), 0, "Pod Identity error logged");
unlink("/tmp/arkime.eks.$_") foreach (keys %eksTokens);

# --- s3Expire (issue #4099): force expire via regressionTests endpoint ---
my $filesQuery = {where => {name => {'$prefix' => "s3://"}, locked => {'$ne' => 1}}};

esGet("/tests2_files/_refresh");
my $esjson = clusterSearch(8124, "files", $filesQuery);
my $before = $esjson->{hits}->{total};
cmp_ok($before, '>', 0, "s3 files exist before expire");

# A huge expireDays shouldn't expire anything
viewerPost2("/plugin/regressionTests/s3Expire?days=36500", "");
esGet("/tests2_files/_refresh");
$esjson = clusterSearch(8124, "files", $filesQuery);
is($esjson->{hits}->{total}, $before, "no s3 files expired with large expire days");

# Test pcaps have old packet times for first, so everything should expire
viewerPost2("/plugin/regressionTests/s3Expire?days=1", "");
esGet("/tests2_files/_refresh");
$esjson = clusterSearch(8124, "files", $filesQuery);
is($esjson->{hits}->{total}, 0, "all s3 files expired");

# The S3 object itself should be gone
isnt(timedSystem("AWS_ACCESS_KEY_ID='foo' AWS_SECRET_ACCESS_KEY='foo' $aws s3 cp $s3url /tmp/arkime.file.expired > /dev/null 2>&1"), 0, "s3 object deleted");
unlink("/tmp/arkime.file.expired");
