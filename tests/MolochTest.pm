package MolochTest;
use Exporter;
use strict;
use Test::More;
@MolochTest::ISA = qw(Exporter);
@MolochTest::EXPORT = qw (esGet esPost esDelete esCopy viewerGet viewerGetToken viewerGet2 viewerDelete viewerPost viewerPost2 viewerPostToken viewerPostToken2 countTest countTest2 errTest bin2hex mesGet mesPost multiGet getTokenCookie getTokenCookie2 parliamentGet parliamentGetToken parliamentPost parliamentPut parliamentDelete parliamentDeleteToken waitFor);

use LWP::UserAgent;
use HTTP::Request::Common;
use JSON;
use URI::Escape;
use Data::Dumper;
use IO::Socket::INET;

$MolochTest::userAgent = LWP::UserAgent->new(timeout => 120);
$MolochTest::host = "127.0.0.1";
$MolochTest::elasticsearch = $ENV{ELASTICSEARCH} || "http://127.0.0.1:9200";


################################################################################
sub viewerGet {
my ($url, $debug) = @_;

    my $response = $MolochTest::userAgent->get("http://$MolochTest::host:8123$url");
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub viewerGetToken {
my ($url, $token, $debug) = @_;

    my $response = $MolochTest::userAgent->get("http://$MolochTest::host:8123$url", "x-moloch-cookie" => $token);
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub viewerGet2 {
my ($url, $debug) = @_;

    my $response = $MolochTest::userAgent->get("http://$MolochTest::host:8124$url");
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub multiGet {
my ($url, $debug) = @_;

    my $response = $MolochTest::userAgent->get("http://$MolochTest::host:8125$url");
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub viewerDelete {
my ($url, $debug) = @_;

    my $response = $MolochTest::userAgent->request(HTTP::Request::Common::_simple_req("DELETE", "http://$MolochTest::host:8123$url"));
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub viewerPost {
my ($url, $content, $debug) = @_;

    my $response = $MolochTest::userAgent->post("http://$MolochTest::host:8123$url", Content => $content);
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub viewerPost2 {
my ($url, $content) = @_;

    my $response = $MolochTest::userAgent->post("http://$MolochTest::host:8124$url", Content => $content);
    #print $url, " response:", $response->content;
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub viewerPostToken {
my ($url, $content, $token, $debug) = @_;

    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $MolochTest::userAgent->post("http://$MolochTest::host:8123$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8", "x-moloch-cookie" => $token);
    } else {
        $response = $MolochTest::userAgent->post("http://$MolochTest::host:8123$url", Content => $content, "x-moloch-cookie" => $token);
    }
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub viewerPostToken2 {
my ($url, $content, $token, $debug) = @_;

    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $MolochTest::userAgent->post("http://$MolochTest::host:8124$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8", "x-moloch-cookie" => $token);
    } else {
        $response = $MolochTest::userAgent->post("http://$MolochTest::host:8124$url", Content => $content, "x-moloch-cookie" => $token);
    }
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub mesGet {
my ($url, $debug) = @_;

    my $response = $MolochTest::userAgent->get("http://$MolochTest::host:8200$url");
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub mesPost {
my ($url, $content) = @_;

    my $response = $MolochTest::userAgent->post("http://$MolochTest::host:8200$url", Content => $content);
    #print $url, " response:", $response->content, "\n";
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub esGet {
my ($url) = @_;

    my $response = $MolochTest::userAgent->get("$MolochTest::elasticsearch$url");
    #print $url, " response:", $response->content;
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub esPost {
my ($url, $content) = @_;

    my $response = $MolochTest::userAgent->post("$MolochTest::elasticsearch$url", Content => $content);
    #print $url, " response:", $response->content;
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub esDelete {
my ($url) = @_;

    my $response = $MolochTest::userAgent->request(HTTP::Request::Common::_simple_req("DELETE", "$MolochTest::elasticsearch$url"));
    #print $url, " response:", $response->content;
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub esCopy
{
    my ($srci, $dsti, $type) = @_;

    my $id = "";
    while (1) {
        my $url;
        if ($id eq "") {
            $url = "/$srci/$type/_search?scroll=10m&size=500";
        } else {
            $url = "/_search/scroll?scroll=10m&scroll_id=$id";
        }
        

        my $incoming = esGet($url);
        my $out = "";
        last if (@{$incoming->{hits}->{hits}} == 0);

        foreach my $hit (@{$incoming->{hits}->{hits}}) {
            $out .= "{\"index\": {\"_index\": \"$dsti\", \"_type\": \"$type\", \"_id\": \"" . $hit->{_id} . "\"}}\n";
            $out .= to_json($hit->{_source}) . "\n";
        }

        $id = $incoming->{_scroll_id};

        esPost("/_bulk", $out);
    }
    esGet("/_flush");
}
################################################################################
sub countTest {
my ($count, $test, $debug) = @_;
    local $Test::Builder::Level = $Test::Builder::Level + 1;
    my $json = viewerGet("/sessions.json?$test");
    diag Dumper($json) if ($debug);
    is ($json->{recordsFiltered}, $count, " recordsFiltered");
    is (scalar @{$json->{data}}, $count, " data count");
    return $json
}
################################################################################
sub countTest2 {
my ($count, $test, $debug) = @_;
    local $Test::Builder::Level = $Test::Builder::Level + 1;
    my $json = viewerGet2("/sessions.json?$test");
    diag Dumper($json) if ($debug);
    is ($json->{recordsFiltered}, $count, " recordsFiltered");
    is (scalar @{$json->{data}}, $count, " data count");
    return $json
}
################################################################################
sub errTest {
my ($test, $debug) = @_;
    local $Test::Builder::Level = $Test::Builder::Level + 1;
    my $json = viewerGet("/sessions.json?$test");
    diag Dumper($json) if ($debug);
    ok (exists $json->{bsqErr}, " bsqErr exists");
}
################################################################################
sub bin2hex {
    my ($data) = @_;

    return unpack("H*", $data);
}
################################################################################
sub getTokenCookie {
my ($userId) = @_;

    my $setCookie;
    if ($userId) {
        $setCookie = $MolochTest::userAgent->get("http://$MolochTest::host:8123/makeToken?molochRegressionUser=$userId")->{"_headers"}->{"set-cookie"};
    } else {
        $setCookie = $MolochTest::userAgent->get("http://$MolochTest::host:8123/makeToken")->{"_headers"}->{"set-cookie"};
    }

    $setCookie =~ /MOLOCH-COOKIE=([^;]*)/;
    return $1;
}
################################################################################
sub getTokenCookie2 {
    my $setCookie = $MolochTest::userAgent->get("http://$MolochTest::host:8124/users")->{"_headers"}->{"set-cookie"};
    $setCookie =~ /MOLOCH-COOKIE=([^;]*)/;
    return $1;
}
################################################################################
sub parliamentGet {
my ($url, $debug) = @_;
    my $response = $MolochTest::userAgent->get("http://$MolochTest::host:8008$url");
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub parliamentGetToken {
my ($url, $token, $debug) = @_;
    my $response = $MolochTest::userAgent->get("http://$MolochTest::host:8008$url", "x-access-token" => $token);
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub parliamentPost {
my ($url, $content, $debug) = @_;
    my $response = $MolochTest::userAgent->post("http://$MolochTest::host:8008$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8");
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub parliamentPut {
my ($url, $content, $debug) = @_;
    my $response = $MolochTest::userAgent->request(HTTP::Request::Common::PUT("http://$MolochTest::host:8008$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8"));
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub parliamentDelete {
my ($url, $debug) = @_;
    my $response = $MolochTest::userAgent->request(HTTP::Request::Common::DELETE("http://$MolochTest::host:8008$url"));
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub parliamentDeleteToken {
my ($url, $token, $debug) = @_;
    my $response = $MolochTest::userAgent->request(HTTP::Request::Common::DELETE("http://$MolochTest::host:8008$url", "x-access-token" => $token));
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub waitFor {
my ($host, $port) = @_;
    while (1) {
        my $sock = IO::Socket::INET->new(
	    PeerAddr => $host,
	    PeerPort => $port,
	    Proto    => 'tcp'
	);
        if ($sock) {
            close($sock);
            return;
        };
        sleep 1;
    }
}

return 1;
