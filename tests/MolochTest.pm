package MolochTest;
use Exporter;
use strict;
use Test::More;
@MolochTest::ISA = qw(Exporter);
@MolochTest::EXPORT = qw (esGet esPost esPut esDelete esCopy viewerGet viewerGetToken viewerGet2 viewerDelete viewerDeleteToken viewerPost viewerPost2 viewerPostToken viewerPostToken2 countTest countTestToken countTest2 countTestMulti errTest bin2hex mesGet mesPost multiGet multiPost getTokenCookie getTokenCookie2 parliamentGet parliamentGetToken parliamentGetToken2 parliamentPost parliamentPostToken2 parliamentPut parliamentPutToken2 parliamentDelete parliamentDeleteToken parliamentDeleteToken2 getParliamentTokenCookie waitFor viewerPutToken viewerPut getCont3xtTokenCookie cont3xtGet cont3xtGetToken cont3xtPut cont3xtPutToken cont3xtDelete cont3xtDeleteToken cont3xtPost cont3xtPostToken);

use LWP::UserAgent;
use HTTP::Request::Common;
use JSON;
use URI::Escape;
use Data::Dumper;
use IO::Socket::INET;

$MolochTest::userAgent = LWP::UserAgent->new(timeout => 120);
$MolochTest::host = "127.0.0.1";
$MolochTest::elasticsearch = $ENV{ELASTICSEARCH} || "http://127.0.0.1:9200";

if ($ENV{INSECURE} eq "--insecure") {
    $MolochTest::userAgent->ssl_opts(
        SSL_verify_mode => 0,
        verify_hostname=> 0
    )
}

################################################################################
sub viewerGet {
my ($url, $debug) = @_;

    my $response = $MolochTest::userAgent->get("http://$MolochTest::host:8123$url");
    diag $url, " response:", $response->content if ($debug);
    my $tmp = $response->content;
    $tmp =~ s,/[-0-9A-Za-z./_]+tests/pcap/,/DIR/tests/pcap/,g;
    my $json = from_json($tmp);
    return ($json);
}
################################################################################
sub viewerGetToken {
my ($url, $token, $debug) = @_;

    my $response = $MolochTest::userAgent->get("http://$MolochTest::host:8123$url", "x-arkime-cookie" => $token);
    diag $url, " response:>", $response->content, "<:\n" if ($debug);
    my $tmp = $response->content;
    $tmp =~ s,/[-0-9A-Za-z./_]+tests/pcap/,/DIR/tests/pcap/,g;
    my $json = from_json($tmp);
    return ($json);
}
################################################################################
sub viewerGet2 {
my ($url, $debug) = @_;

    my $response = $MolochTest::userAgent->get("http://$MolochTest::host:8124$url");
    diag $url, " response:", $response->content if ($debug);
    my $tmp = $response->content;
    $tmp =~ s,/[-0-9A-Za-z./_]+tests/pcap/,/DIR/tests/pcap/,g;
    my $json = from_json($tmp);
    return ($json);
}
################################################################################
sub multiGet {
my ($url, $debug) = @_;

    my $response = $MolochTest::userAgent->get("http://$MolochTest::host:8125$url");
    diag $url, " response:", $response->content if ($debug);
    my $tmp = $response->content;
    $tmp =~ s,/[-0-9A-Za-z./_]+tests/pcap/,/DIR/tests/pcap/,g;
    my $json = from_json($tmp);
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
sub viewerDeleteToken {
my ($url, $token, $debug) = @_;

    my $response = $MolochTest::userAgent->request(HTTP::Request::Common::_simple_req("DELETE", "http://$MolochTest::host:8123$url", "x-arkime-cookie" => $token));
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub viewerPost {
my ($url, $content, $debug) = @_;

    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $MolochTest::userAgent->post("http://$MolochTest::host:8123$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8");
    } else {
        $response = $MolochTest::userAgent->post("http://$MolochTest::host:8123$url", Content => $content);
    }

    diag $url, " response:", $response->content if ($debug);
    my $tmp = $response->content;
    $tmp =~ s,/[-0-9A-Za-z./_]+tests/pcap/,/DIR/tests/pcap/,g;
    my $json = from_json($tmp);
    return ($json);
}
################################################################################
sub multiPost {
my ($url, $content, $debug) = @_;

    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $MolochTest::userAgent->post("http://$MolochTest::host:8125$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8");
    } else {
        $response = $MolochTest::userAgent->post("http://$MolochTest::host:8125$url", Content => $content);
    }

    diag $url, " response:", $response->content if ($debug);
    my $tmp = $response->content;
    $tmp =~ s,/[-0-9A-Za-z./_]+tests/pcap/,/DIR/tests/pcap/,g;
    my $json = from_json($tmp);
    return ($json);
}
################################################################################
sub viewerPost2 {
my ($url, $content, $debug) = @_;

    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $MolochTest::userAgent->post("http://$MolochTest::host:8124$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8");
    } else {
        $response = $MolochTest::userAgent->post("http://$MolochTest::host:8124$url", Content => $content);
    }

    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub viewerPostToken {
my ($url, $content, $token, $debug) = @_;

    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $MolochTest::userAgent->post("http://$MolochTest::host:8123$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8", "x-arkime-cookie" => $token);
    } else {
        $response = $MolochTest::userAgent->post("http://$MolochTest::host:8123$url", Content => $content, "x-arkime-cookie" => $token);
    }
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub viewerPostToken2 {
my ($url, $content, $token, $debug) = @_;

    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $MolochTest::userAgent->post("http://$MolochTest::host:8124$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8", "x-arkime-cookie" => $token);
    } else {
        $response = $MolochTest::userAgent->post("http://$MolochTest::host:8124$url", Content => $content, "x-arkime-cookie" => $token);
    }
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub viewerPut {
my ($url, $content, $debug) = @_;

    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $MolochTest::userAgent->put("http://$MolochTest::host:8123$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8");
    } else {
        $response = $MolochTest::userAgent->put("http://$MolochTest::host:8123$url", Content => $content);
    }

    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub viewerPutToken {
my ($url, $content, $token, $debug) = @_;
    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $MolochTest::userAgent->put("http://$MolochTest::host:8123$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8", "x-arkime-cookie" => $token);
    } else {
        $response = $MolochTest::userAgent->put("http://$MolochTest::host:8123$url", Content => $content, "x-arkime-cookie" => $token);
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
    #print "$MolochTest::elasticsearch$url", " response:", $response->content;
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub esPost {
my ($url, $content) = @_;

    my $response = $MolochTest::userAgent->post("$MolochTest::elasticsearch$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8");
    #diag $url, " response:", $response->content;
    #print "$MolochTest::elasticsearch$url content:", $content,"\n response:", $response->content;
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub esPut {
my ($url, $content) = @_;

    my $response = $MolochTest::userAgent->put("$MolochTest::elasticsearch$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8");
    #diag $url, " response:", $response->content;
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
    my ($srci, $dsti) = @_;

    my $id = "";
    while (1) {
        my $url;
        if ($id eq "") {
            $url = "/$srci/_search?scroll=10m&size=500";
        } else {
            $url = "/_search/scroll?scroll=10m&scroll_id=$id";
        }

        my $incoming = esGet($url);
        my $out = "";
        last if (@{$incoming->{hits}->{hits}} == 0);

        foreach my $hit (@{$incoming->{hits}->{hits}}) {
            $out .= "{\"index\": {\"_index\": \"$dsti\", \"_id\": \"" . $hit->{_id} . "\"}}\n";
            $out .= to_json($hit->{_source}) . "\n";
        }

        $id = $incoming->{_scroll_id};

        esPost("/_bulk?refresh=wait_for", $out);
    }
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
sub countTestToken {
my ($token, $count, $test, $debug) = @_;
    local $Test::Builder::Level = $Test::Builder::Level + 1;
    my $json = viewerGetToken("/sessions.json?$test", $token);
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
sub countTestMulti {
my ($count, $test, $debug) = @_;
    local $Test::Builder::Level = $Test::Builder::Level + 1;
    my $json = multiGet("/sessions.json?$test");
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
    ok (exists $json->{error}, " bsqErr exists");
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

    $setCookie =~ /ARKIME-COOKIE=([^;]*)/;
    return uri_unescape($1);
}
################################################################################
sub getTokenCookie2 {
    my $setCookie = $MolochTest::userAgent->get("http://$MolochTest::host:8124/users")->{"_headers"}->{"set-cookie"};
    $setCookie =~ /ARKIME-COOKIE=([^;]*)/;
    return uri_unescape($1);
}
################################################################################
sub getCont3xtTokenCookie {
my ($userId) = @_;

    my $setCookie;
    if ($userId) {
        $setCookie = $MolochTest::userAgent->get("http://$MolochTest::host:3218/makeToken?molochRegressionUser=$userId")->{"_headers"}->{"set-cookie"};
    } else {
        $setCookie = $MolochTest::userAgent->get("http://$MolochTest::host:3218/makeToken")->{"_headers"}->{"set-cookie"};
    }

    $setCookie =~ /CONT3XT-COOKIE=([^;]*)/;
    return uri_unescape($1);
}
################################################################################
sub getParliamentTokenCookie {
my ($userId) = @_;

    my $setCookie;
    if ($userId) {
        $setCookie = $MolochTest::userAgent->get("http://$MolochTest::host:8009/parliament/api/regressionTests/makeToken/?molochRegressionUser=$userId")->{"_headers"}->{"set-cookie"};
    } else {
        $setCookie = $MolochTest::userAgent->get("http://$MolochTest::host:8009/parliament/api/regressionTests/makeToken")->{"_headers"}->{"set-cookie"};
    }

    $setCookie =~ /PARLIAMENT-COOKIE=([^;]*)/;
    return uri_unescape($1);
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
sub parliamentGetToken2 {
my ($url, $token, $debug) = @_;
    my $response = $MolochTest::userAgent->get("http://$MolochTest::host:8009$url", "x-access-token" => $token);
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
sub parliamentPostToken2 {
my ($url, $content, $token, $debug) = @_;
    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $MolochTest::userAgent->post("http://$MolochTest::host:8009$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8", "x-parliament-cookie" => $token);
    } else {
        $response = $MolochTest::userAgent->post("http://$MolochTest::host:8009$url", Content => $content, "x-parliament-cookie" => $token);
    }
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
sub parliamentPutToken2 {
my ($url, $content, $token, $debug) = @_;
    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $MolochTest::userAgent->put("http://$MolochTest::host:8009$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8", "x-parliament-cookie" => $token);
    } else {
        $response = $MolochTest::userAgent->put("http://$MolochTest::host:8009$url", Content => $content, "x-parliament-cookie" => $token);
    }
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
sub parliamentDeleteToken2 {
my ($url, $token, $debug) = @_;
    my $response = $MolochTest::userAgent->request(HTTP::Request::Common::_simple_req("DELETE", "http://$MolochTest::host:8009$url", "x-parliament-cookie" => $token));
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub cont3xtGet {
my ($url, $debug) = @_;

    my $response = $MolochTest::userAgent->get("http://$MolochTest::host:3218$url");
    diag $url, " response:", $response->content if ($debug);
    my $tmp = $response->content;
    my $json = from_json($tmp);
    return ($json);
}
################################################################################
sub cont3xtGetToken {
my ($url, $token, $debug) = @_;
    my $response = $MolochTest::userAgent->request(HTTP::Request::Common::GET("http://$MolochTest::host:3218$url", "x-cont3xt-cookie" => $token));
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub cont3xtPut {
my ($url, $content, $debug) = @_;
    my $response = $MolochTest::userAgent->request(HTTP::Request::Common::PUT("http://$MolochTest::host:3218$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8"));
    diag $url, " response:", $response->content if ($debug);
    return $response->content if ($response->content =~ /^[^[{]/);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub cont3xtPutToken {
my ($url, $content, $token, $debug) = @_;
    my $response = $MolochTest::userAgent->request(HTTP::Request::Common::PUT("http://$MolochTest::host:3218$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8", "x-cont3xt-cookie" => $token));
    diag $url, " response:", $response->content if ($debug);
    return $response->content if ($response->content =~ /^[^\[{]/);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub cont3xtPost {
my ($url, $content, $debug) = @_;
    my $response = $MolochTest::userAgent->request(HTTP::Request::Common::POST("http://$MolochTest::host:3218$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8"));
    diag $url, " response:", $response->content if ($debug);
    return $response->content if ($response->content =~ /^[^\[{]/);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub cont3xtPostToken {
my ($url, $content, $token, $debug) = @_;
    my $response = $MolochTest::userAgent->request(HTTP::Request::Common::POST("http://$MolochTest::host:3218$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8", "x-cont3xt-cookie" => $token));
    diag $url, " response:", $response->content if ($debug);
    return $response->content if ($response->content =~ /^[^\[{]/);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub cont3xtDelete {
my ($url, $content, $debug) = @_;
    my $response = $MolochTest::userAgent->request(HTTP::Request::Common::DELETE("http://$MolochTest::host:3218$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8"));
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub cont3xtDeleteToken {
my ($url, $content, $token, $debug) = @_;
    my $response = $MolochTest::userAgent->request(HTTP::Request::Common::DELETE("http://$MolochTest::host:3218$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8", "x-cont3xt-cookie" => $token));
    diag $url, " response:", $response->content if ($debug);
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub waitFor {
my ($host, $port, $extraSleep) = @_;
    print "Connecting $host:$port\n" if ($main::debug);
    while (1) {
        my $sock = IO::Socket::INET->new(
	    PeerAddr => $host,
	    PeerPort => $port,
	    Proto    => 'tcp'
	);
        if ($sock) {
            print "Success\n" if ($main::debug);
            close($sock);
            return;
        };
        sleep 1;
    }
    sleep ($extraSleep) if (defined $extraSleep);
}

return 1;
