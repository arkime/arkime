use Test::More tests => 8;
use ArkimeTest;
use File::Temp qw(tempfile);
use JSON;
use POSIX qw(WNOHANG);
use IO::Socket::INET;
use URI::Escape;
use strict;

# A viewer whose config is loaded from an elasticsearch:// uri with credentials,
# started just for this test. Uploads must never return or log those credentials.

my $port = 8140;
my $index = "tests_uploadconfig";

# Reuse real credentials if the test ES needs them, otherwise ES ignores fake ones.
# Real ones also reach the viewer through -o elasticsearch, so only check the whole userinfo then.
my ($es) = split(/,/, $ArkimeTest::elasticsearch);
my ($scheme, $userinfo, $hostpath) = $es =~ m{^\s*(https?)://(?:([^@/]*)@)?(.*?)\s*$};
my $secret = $userinfo;
if (!defined $userinfo) {
    $userinfo = "uploaduser:s3cr3tpw";
    $secret = "s3cr3tpw";
}
$scheme =~ s/^http/elasticsearch/;
$hostpath =~ s{/$}{};
my $configUri = "$scheme://$userinfo\@$hostpath/$index/_doc/viewer";

esPut("/$index/_doc/viewer?refresh=true", to_json({
    default => {
        uploadRoles => 'arkimeUser',
        # The second echo prints the uri rewritten the way capture does in its errors
        uploadCommand => "echo {CONFIG}; echo {CONFIG} | sed 's/^elasticsearch/http/;s/_doc/_source/' && test {ORIGINALNAME} != fail.pcap"
    }
}));

my (undef, $logFile) = tempfile("arkime.uploadconfig.XXXXXX", TMPDIR => 1, UNLINK => 1);

my $pid = fork();
die "fork: $!" if !defined $pid;
if ($pid == 0) {
    chdir('../viewer') or die "chdir ../viewer: $!";
    open(STDOUT, '>', $logFile);
    open(STDERR, '>&', STDOUT);
    my @args = ('viewer.js', '-c', $configUri, '-n', 'test', '--regressionTests',
        '-o', "elasticsearch=$ArkimeTest::elasticsearch",
        '-o', "usersElasticsearch=$ArkimeTest::usersElasticsearch",
        '-o', 'prefix=tests', '-o', 'usersPrefix=tests', '-o', 'passwordSecret=password',
        '-o', "viewPort=$port", '-o', 'authMode=anonymous', '-o', 'cronQueries=false');
    push @args, $ENV{INSECURE} if defined $ENV{INSECURE} && $ENV{INSECURE} ne '';
    exec('node', @args);
    die "exec: $!";
}
END {
    kill 'TERM', $pid if $pid;
    esDelete("/$index");
}

# Don't hang if the viewer couldn't start from the uri config
for (my $i = 0; ; $i++) {
    last if IO::Socket::INET->new(PeerAddr => $ArkimeTest::host, PeerPort => $port, Proto => 'tcp');
    if (waitpid($pid, WNOHANG) != 0 || $i >= 60) {
        my $log = do { local $/; open my $fh, '<', $logFile; <$fh> };
        BAIL_OUT("viewer from uri config didn't start:\n$log");
    }
    sleep 1;
}
sleep 1;

my $setCookie = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:$port/regressionTests/makeToken")->{"_headers"}->{"set-cookie"};
$setCookie =~ /ARKIME-COOKIE=([^;]*)/;
my $token = uri_unescape($1);

sub upload {
    my ($name) = @_;
    return $ArkimeTest::userAgent->post("http://$ArkimeTest::host:$port/api/upload", "x-arkime-cookie" => $token,
        Content_Type => 'form-data',
        Content => [
            file => [undef, $name, 'Content-Type' => 'application/octet-stream', Content => 'notreallyapcap']
        ]);
}

my $response = upload('good.pcap');
is ($response->code, 200);
like ($response->content, qr/^echo &#39;?elasticsearchs?:&#47;&#47;\*\*\*\*@/, "upload response shows the redacted config");
like ($response->content, qr/^https?:&#47;&#47;\*\*\*\*@/m, "upload response redacts the uri as capture rewrites it");
unlike ($response->content, qr/\Q$secret\E/, "upload response hides the config password");

$response = upload('fail.pcap');
is ($response->code, 500);
unlike ($response->content, qr/\Q$secret\E/, "failed upload response hides the config password");

my $log = do { local $/; open my $fh, '<', $logFile or die $!; <$fh> };
like ($log, qr{upload command: +echo '?elasticsearchs?://\*\*\*\*@}, "upload log shows the redacted config");
unlike ($log, qr/\Q$secret\E/, "viewer log hides the config password");
