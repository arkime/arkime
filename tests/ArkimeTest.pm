package ArkimeTest;
use Exporter;
use Carp;
use strict;
use Test::More;
@ArkimeTest::ISA = qw(Exporter);
@ArkimeTest::EXPORT = qw (esGet esPost esPut esDelete esCopy viewerGet viewerGetToken viewerGet2 viewerDelete viewerDeleteToken viewerDeleteToken2 viewerPost viewerPost2 viewerPostToken viewerPostToken2 countTest countTestToken countTest2 countTestMulti errTest bin2hex mesGet mesPost multiGet multiPost getTokenCookie getTokenCookie2 parliamentGet parliamentGetToken parliamentPost parliamentPostToken parliamentPut parliamentPutToken parliamentDelete parliamentDeleteToken getParliamentTokenCookie waitFor viewerPutToken viewerPut getCont3xtTokenCookie cont3xtGet cont3xtGetToken cont3xtPut cont3xtPutToken cont3xtDelete cont3xtDeleteToken cont3xtPost cont3xtPostToken addUser clearIndex);

use LWP::UserAgent;
use HTTP::Request::Common;
use JSON;
use URI::Escape;
use Data::Dumper;
use IO::Socket::INET;
use Try::Tiny;

$ArkimeTest::userAgent = LWP::UserAgent->new(timeout => 120);
$ArkimeTest::host = "127.0.0.1";
$ArkimeTest::elasticsearch = $ENV{ELASTICSEARCH} || "http://127.0.0.1:9200";
$ArkimeTest::es = "-o 'elasticsearch=$ArkimeTest::elasticsearch' -o 'usersElasticsearch=$ArkimeTest::elasticsearch' $ENV{INSECURE}";

if ($ENV{INSECURE} eq "--insecure") {
    $ArkimeTest::userAgent->ssl_opts(
        SSL_verify_mode => 0,
        verify_hostname=> 0
    )
}

################################################################################
sub my_from_json
{
my ($url, $json, $debug) = @_;

    return try { from_json($json); } catch { Carp::confess "$url - not JSON - '$json'"; };
}
################################################################################
sub viewerGet {
my ($url, $debug) = @_;

    my $response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123$url");
    diag $url, " response:", $response->content if ($debug);
    my $tmp = $response->content;
    $tmp =~ s,/[-0-9A-Za-z./_]+tests/pcap/,/DIR/tests/pcap/,g;
    return my_from_json($url, $tmp, $debug);
}
################################################################################
sub viewerGetToken {
my ($url, $token, $debug) = @_;

    my $response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123$url", "x-arkime-cookie" => $token);
    diag $url, " response:>", $response->content, "<:\n" if ($debug);
    my $tmp = $response->content;
    $tmp =~ s,/[-0-9A-Za-z./_]+tests/pcap/,/DIR/tests/pcap/,g;
    return my_from_json($url, $tmp, $debug);
}
################################################################################
sub viewerGet2 {
my ($url, $debug) = @_;

    my $response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8124$url");
    diag $url, " response:", $response->content if ($debug);
    my $tmp = $response->content;
    $tmp =~ s,/[-0-9A-Za-z./_]+tests/pcap/,/DIR/tests/pcap/,g;
    return my_from_json($url, $tmp, $debug);
}
################################################################################
sub multiGet {
my ($url, $debug) = @_;

    my $response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8125$url");
    diag $url, " response:", $response->content if ($debug);
    my $tmp = $response->content;
    $tmp =~ s,/[-0-9A-Za-z./_]+tests/pcap/,/DIR/tests/pcap/,g;
    return my_from_json($url, $tmp, $debug);
}
################################################################################
sub viewerDelete {
my ($url, $debug) = @_;

    my $response = $ArkimeTest::userAgent->request(HTTP::Request::Common::_simple_req("DELETE", "http://$ArkimeTest::host:8123$url"));
    diag $url, " response:", $response->content if ($debug);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub viewerDeleteToken {
my ($url, $token, $debug) = @_;

    my $response = $ArkimeTest::userAgent->request(HTTP::Request::Common::_simple_req("DELETE", "http://$ArkimeTest::host:8123$url", "x-arkime-cookie" => $token));
    diag $url, " response:", $response->content if ($debug);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub viewerDeleteToken2 {
my ($url, $token, $debug) = @_;

    my $response = $ArkimeTest::userAgent->request(HTTP::Request::Common::_simple_req("DELETE", "http://$ArkimeTest::host:8124$url", "x-arkime-cookie" => $token));
    diag $url, " response:", $response->content if ($debug);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub viewerPost {
my ($url, $content, $debug) = @_;

    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8123$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8");
    } else {
        $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8123$url", Content => $content);
    }

    diag $url, " response:", $response->content if ($debug);
    my $tmp = $response->content;
    $tmp =~ s,/[-0-9A-Za-z./_]+tests/pcap/,/DIR/tests/pcap/,g;
    return my_from_json($url, $tmp, $debug);
}
################################################################################
sub multiPost {
my ($url, $content, $debug) = @_;

    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8125$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8");
    } else {
        $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8125$url", Content => $content);
    }

    diag $url, " response:", $response->content if ($debug);
    my $tmp = $response->content;
    $tmp =~ s,/[-0-9A-Za-z./_]+tests/pcap/,/DIR/tests/pcap/,g;
    return my_from_json($url, $tmp, $debug);
}
################################################################################
sub viewerPost2 {
my ($url, $content, $debug) = @_;

    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8124$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8");
    } else {
        $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8124$url", Content => $content);
    }

    diag $url, " response:", $response->content if ($debug);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub viewerPostToken {
my ($url, $content, $token, $debug) = @_;

    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8123$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8", "x-arkime-cookie" => $token);
    } else {
        $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8123$url", Content => $content, "x-arkime-cookie" => $token);
    }
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub viewerPostToken2 {
my ($url, $content, $token, $debug) = @_;

    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8124$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8", "x-arkime-cookie" => $token);
    } else {
        $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8124$url", Content => $content, "x-arkime-cookie" => $token);
    }
    diag $url, " response:", $response->content if ($debug);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub viewerPut {
my ($url, $content, $debug) = @_;

    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $ArkimeTest::userAgent->put("http://$ArkimeTest::host:8123$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8");
    } else {
        $response = $ArkimeTest::userAgent->put("http://$ArkimeTest::host:8123$url", Content => $content);
    }

    diag $url, " response:", $response->content if ($debug);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub viewerPutToken {
my ($url, $content, $token, $debug) = @_;
    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $ArkimeTest::userAgent->put("http://$ArkimeTest::host:8123$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8", "x-arkime-cookie" => $token);
    } else {
        $response = $ArkimeTest::userAgent->put("http://$ArkimeTest::host:8123$url", Content => $content, "x-arkime-cookie" => $token);
    }
    diag $url, " response:", $response->content if ($debug);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub mesGet {
my ($url, $debug) = @_;

    my $response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8200$url");
    diag $url, " response:", $response->content if ($debug);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub mesPost {
my ($url, $content, $debug) = @_;

    my $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8200$url", Content => $content);
    #print $url, " response:", $response->content, "\n";
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub esGet {
my ($url, $debug) = @_;

    my $response = $ArkimeTest::userAgent->get("$ArkimeTest::elasticsearch$url");
    #print "$ArkimeTest::elasticsearch$url", " response:", $response->content;
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub esPost {
my ($url, $content, $debug) = @_;

    my $response = $ArkimeTest::userAgent->post("$ArkimeTest::elasticsearch$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8");
    #diag $url, " response:", $response->content;
    #print "$ArkimeTest::elasticsearch$url content:", $content,"\n response:", $response->content;
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub esPut {
my ($url, $content, $debug) = @_;

    my $response = $ArkimeTest::userAgent->put("$ArkimeTest::elasticsearch$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8");
    #diag $url, " response:", $response->content;
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub esDelete {
my ($url, $debug) = @_;

    my $response = $ArkimeTest::userAgent->request(HTTP::Request::Common::_simple_req("DELETE", "$ArkimeTest::elasticsearch$url"));
    #print $url, " response:", $response->content;
    return my_from_json($url, $response->content, $debug);
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
        $setCookie = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/regressionTests/makeToken?arkimeRegressionUser=$userId")->{"_headers"}->{"set-cookie"};
    } else {
        $setCookie = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/regressionTests/makeToken")->{"_headers"}->{"set-cookie"};
    }

    $setCookie =~ /ARKIME-COOKIE=([^;]*)/;
    return uri_unescape($1);
}
################################################################################
sub getTokenCookie2 {
    my $setCookie = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8124/users")->{"_headers"}->{"set-cookie"};
    $setCookie =~ /ARKIME-COOKIE=([^;]*)/;
    return uri_unescape($1);
}
################################################################################
sub getCont3xtTokenCookie {
my ($userId) = @_;

    my $setCookie;
    if ($userId) {
        $setCookie = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:3218/makeToken?arkimeRegressionUser=$userId")->{"_headers"}->{"set-cookie"};
    } else {
        $setCookie = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:3218/makeToken")->{"_headers"}->{"set-cookie"};
    }

    $setCookie =~ /CONT3XT-COOKIE=([^;]*)/;
    return uri_unescape($1);
}
################################################################################
sub getParliamentTokenCookie {
my ($userId) = @_;

    my $setCookie;
    if ($userId) {
        $setCookie = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8008/parliament/api/regressionTests/makeToken?arkimeRegressionUser=$userId")->{"_headers"}->{"set-cookie"};
    } else {
        $setCookie = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8008/parliament/api/regressionTests/makeToken")->{"_headers"}->{"set-cookie"};
    }

    $setCookie =~ /PARLIAMENT-COOKIE=([^;]*)/;
    return uri_unescape($1);
}
################################################################################
sub parliamentGet {
my ($url, $debug) = @_;
    my $response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8008$url");
    diag $url, " response:", $response->content if ($debug);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub parliamentGetToken {
my ($url, $token, $debug) = @_;
    my $response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8008$url", "x-parliament-token" => $token);
    diag $url, " response:", $response->content if ($debug);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub parliamentPost {
my ($url, $content, $debug) = @_;
    my $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8008$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8");
    diag $url, " response:", $response->content if ($debug);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub parliamentPostToken {
my ($url, $content, $token, $debug) = @_;
    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8008$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8", "x-parliament-cookie" => $token);
    } else {
        $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8008$url", Content => $content, "x-parliament-cookie" => $token);
    }
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub parliamentPut {
my ($url, $content, $debug) = @_;
    my $response = $ArkimeTest::userAgent->request(HTTP::Request::Common::PUT("http://$ArkimeTest::host:8008$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8"));
    diag $url, " response:", $response->content if ($debug);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub parliamentPutToken {
my ($url, $content, $token, $debug) = @_;
    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $ArkimeTest::userAgent->put("http://$ArkimeTest::host:8008$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8", "x-parliament-cookie" => $token);
    } else {
        $response = $ArkimeTest::userAgent->put("http://$ArkimeTest::host:8008$url", Content => $content, "x-parliament-cookie" => $token);
    }
    diag $url, " response:", $response->content if ($debug);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub parliamentDelete {
my ($url, $debug) = @_;
    my $response = $ArkimeTest::userAgent->request(HTTP::Request::Common::DELETE("http://$ArkimeTest::host:8008$url"));
    diag $url, " response:", $response->content if ($debug);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub parliamentDeleteToken {
my ($url, $token, $debug) = @_;
    my $response = $ArkimeTest::userAgent->request(HTTP::Request::Common::DELETE("http://$ArkimeTest::host:8008$url", "x-parliament-cookie" => $token));
    diag $url, " response:", $response->content if ($debug);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub cont3xtGet {
my ($url, $debug) = @_;
    my $response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:3218$url");
    diag $url, " response:", $response->content if ($debug);
    my $tmp = $response->content;
    return my_from_json($url, $tmp, $debug);
}
################################################################################
sub cont3xtGetToken {
my ($url, $token, $debug) = @_;
    my $response = $ArkimeTest::userAgent->request(HTTP::Request::Common::GET("http://$ArkimeTest::host:3218$url", "x-cont3xt-cookie" => $token));
    diag $url, " response:", $response->content if ($debug);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub cont3xtPut {
my ($url, $content, $debug) = @_;
    my $response = $ArkimeTest::userAgent->request(HTTP::Request::Common::PUT("http://$ArkimeTest::host:3218$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8"));
    diag $url, " response:", $response->content if ($debug);
    return $response->content if ($response->content =~ /^[^[{]/);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub cont3xtPutToken {
my ($url, $content, $token, $debug) = @_;
    my $response = $ArkimeTest::userAgent->request(HTTP::Request::Common::PUT("http://$ArkimeTest::host:3218$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8", "x-cont3xt-cookie" => $token));
    diag $url, " response:", $response->content if ($debug);
    return $response->content if ($response->content =~ /^[^\[{]/);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub cont3xtPost {
my ($url, $content, $debug) = @_;
    my $response = $ArkimeTest::userAgent->request(HTTP::Request::Common::POST("http://$ArkimeTest::host:3218$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8"));
    diag $url, " response:", $response->content if ($debug);
    return $response->content if ($response->content =~ /^[^\[{]/);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub cont3xtPostToken {
my ($url, $content, $token, $debug) = @_;
    my $response = $ArkimeTest::userAgent->request(HTTP::Request::Common::POST("http://$ArkimeTest::host:3218$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8", "x-cont3xt-cookie" => $token));
    diag $url, " response:", $response->content if ($debug);
    return $response->content if ($response->content =~ /^[^\[{]/);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub cont3xtDelete {
my ($url, $content, $debug) = @_;
    my $response = $ArkimeTest::userAgent->request(HTTP::Request::Common::DELETE("http://$ArkimeTest::host:3218$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8"));
    diag $url, " response:", $response->content if ($debug);
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub cont3xtDeleteToken {
my ($url, $content, $token, $debug) = @_;
    my $response = $ArkimeTest::userAgent->request(HTTP::Request::Common::DELETE("http://$ArkimeTest::host:3218$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8", "x-cont3xt-cookie" => $token));
    diag $url, " response:", $response->content if ($debug);
    return my_from_json($url, $response->content, $debug);
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
################################################################################
sub addUser {
    return system("cd ../viewer ; node addUser.js $ArkimeTest::es -c ../tests/config.test.ini $_[0]");
}
################################################################################
sub clearIndex {
my ($index) = @_;
    esGet("/_flush");
    esGet("/_refresh");
    return esPost("/$index/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }');
}

return 1;
