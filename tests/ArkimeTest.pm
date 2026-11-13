package ArkimeTest;
use Exporter;
use Carp;
use strict;
use Test::More;
@ArkimeTest::ISA = qw(Exporter);
@ArkimeTest::EXPORT = qw (timedSystem hungReport sessionsPost sessionsDeleteByTag sessionsDeleteByNode sessionGet sessionDelete esGet esPost esPut esDelete clusterSearch clusterPut clusterDelete clusterDeleteWhere clusterCopy viewerGet viewerGetToken viewerGet2 viewerDelete viewerDeleteToken viewerDeleteToken2 multiDeleteToken viewerPost viewerPost2 viewerPostToken viewerPostToken2 countTest countTestToken countTest2 countTestMulti errTest bin2hex mesGet mesPost multiGet multiGetToken multiPost multiPostToken getTokenCookie getTokenCookie2 parliamentGet parliamentGetToken parliamentPost parliamentPostToken parliamentPut parliamentPutToken parliamentDelete parliamentDeleteToken getParliamentTokenCookie waitFor viewerPutToken viewerPut getCont3xtTokenCookie cont3xtGet cont3xtGetToken cont3xtPut cont3xtPutToken cont3xtDelete cont3xtDeleteToken cont3xtPost cont3xtPostToken addUser changePasswordSecret clearIndex generate_totp waitUntil);

use LWP::UserAgent;
use HTTP::Request::Common;
use JSON;
use URI::Escape;
use Data::Dumper;
use IO::Socket::INET;
use Time::HiRes ();
use Try::Tiny;
use Text::ParseWords qw(shellwords);
use POSIX ();

$ArkimeTest::userAgent = LWP::UserAgent->new(timeout => 120, keep_alive => 10);
$ArkimeTest::host = "127.0.0.1";
$ArkimeTest::elasticsearch = $ENV{ELASTICSEARCH} || "http://127.0.0.1:9200";
$ArkimeTest::usersElasticsearch = $ENV{USERSELASTICSEARCH} || "http://127.0.0.1:9200";
$ArkimeTest::es = "-o 'elasticsearch=" . ($ENV{ARKIME_TEST_NOES} ? "" : $ArkimeTest::elasticsearch) . "' -o 'usersElasticsearch=$ArkimeTest::usersElasticsearch' $ENV{INSECURE}";

if ($ENV{INSECURE} eq "--insecure") {
    $ArkimeTest::userAgent->ssl_opts(
        SSL_verify_mode => 0,
        verify_hostname=> 0
    )
}

# Where session documents live: ClickHouse when the environment (tests_ch.sh)
# or config.test.ini has a clickhouse sessionsDbUrl, otherwise Elasticsearch.
$ArkimeTest::sessionsDbUrl = $ENV{ARKIME_test__sessionsDbUrl} // "";
if (!$ArkimeTest::sessionsDbUrl && open my $cfgFh, '<', "config.test.ini") {
    while (<$cfgFh>) {
        if (/^sessionsDbUrl\s*=\s*(.*?)\s*$/) { $ArkimeTest::sessionsDbUrl = $1; last; }
    }
    close $cfgFh;
}
# Any sessions db that isn't OpenSearch/Elasticsearch, ClickHouse or a dbPlugins scheme. A dbPlugins one
# needs ARKIME_TEST_SESSIONS_HELPER, a script that can wipe, post, deleteByTag and find its sessions.
$ArkimeTest::sessionsDbNonES = $ArkimeTest::sessionsDbUrl ne "" && $ArkimeTest::sessionsDbUrl !~ m{^(https?|elasticsearchs?|opensearchs?)://}i;
$ArkimeTest::sessionsDbCH = $ArkimeTest::sessionsDbUrl =~ m{^(clickhouses?|chttps?)://}i;

################################################################################
# Runs the sessions helper of a dbPlugins sessions db
sub sessionsHelper {
    my (@args) = @_;
    die "ARKIME_TEST_SESSIONS_HELPER isn't set for sessionsDbUrl $ArkimeTest::sessionsDbUrl\n" if (!$ENV{ARKIME_TEST_SESSIONS_HELPER});
    open(my $fh, '-|', 'node', $ENV{ARKIME_TEST_SESSIONS_HELPER}, $ArkimeTest::sessionsDbUrl, @args) or return 0;
    my $out = do { local $/; <$fh> };
    close($fh);
    diag $out if ($? != 0);
    return $? == 0;
}

# The sessions helper's json output, undef when it fails
sub sessionsHelperJson {
    my (@args) = @_;
    open(my $fh, '-|', 'node', $ENV{ARKIME_TEST_SESSIONS_HELPER}, $ArkimeTest::sessionsDbUrl, @args) or return undef;
    my $out = do { local $/; <$fh> };
    close($fh);
    if ($? != 0) { diag $out; return undef; }
    return from_json($out);
}

# A dbUrl that isn't OpenSearch/Elasticsearch from the environment or config.test.ini, like postgres:// or a
# dbPlugins scheme, otherwise capture's stats, dstats, files, fields and sequence are in Elasticsearch.
# Only the sessions follow sessionsDbUrl, so such a dbUrl keeps them in Elasticsearch.
$ArkimeTest::dbUrl = $ENV{ARKIME_default__dbUrl} // "";
if (!$ArkimeTest::dbUrl && open my $cfgFh, '<', "config.test.ini") {
    while (<$cfgFh>) {
        if (/^dbUrl\s*=\s*(.*?)\s*$/) { $ArkimeTest::dbUrl = $1; last; }
    }
    close $cfgFh;
}
$ArkimeTest::clusterDbNonES = $ArkimeTest::dbUrl ne "" && $ArkimeTest::dbUrl !~ m{^(https?|elasticsearchs?|opensearchs?)://}i;
# No OpenSearch/Elasticsearch at all (tests_noes.sh): the es* helpers answer {}
$ArkimeTest::noEs = $ENV{ARKIME_TEST_NOES};
# The db options for the node scripts, saved now since a test may clear the ARKIME_ environment
@ArkimeTest::dbArgs = $ArkimeTest::noEs
    ? ('-o', "elasticsearch=", '-o', "dbUrl=$ENV{ARKIME_default__dbUrl}", '-o', "sessionsDbUrl=$ENV{ARKIME_default__sessionsDbUrl}")
    : ('-o', "elasticsearch=$ArkimeTest::elasticsearch");
push @ArkimeTest::dbArgs, '-o', "dbPlugins=$ENV{ARKIME_default__dbPlugins}" if ($ENV{ARKIME_default__dbPlugins});
$ENV{ARKIME_default__sessionsDbUrl} //= $ArkimeTest::elasticsearch if ($ArkimeTest::clusterDbNonES);

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
sub multiGetToken {
my ($url, $token, $debug) = @_;

    my $response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8125$url", "x-arkime-cookie" => $token);
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
sub multiDeleteToken {
my ($url, $token, $debug) = @_;

    my $response = $ArkimeTest::userAgent->request(HTTP::Request::Common::_simple_req("DELETE", "http://$ArkimeTest::host:8125$url", "x-arkime-cookie" => $token));
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
sub multiPostToken {
my ($url, $content, $token, $debug) = @_;

    my $response;
    if (substr($content, 0, 2) eq '{"') {
        $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8125$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8", "x-arkime-cookie" => $token);
    } else {
        $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8125$url", Content => $content, "x-arkime-cookie" => $token);
    }
    return my_from_json($url, $response->content, $debug);
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

    return {} if ($ArkimeTest::noEs);

    my $response = $ArkimeTest::userAgent->get("$ArkimeTest::elasticsearch$url");
    #print "$ArkimeTest::elasticsearch$url", " response:", $response->content;
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub esPost {
my ($url, $content, $debug) = @_;

    return {} if ($ArkimeTest::noEs);

    my $response = $ArkimeTest::userAgent->post("$ArkimeTest::elasticsearch$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8");
    #diag $url, " response:", $response->content;
    #print "$ArkimeTest::elasticsearch$url content:", $content,"\n response:", $response->content;
    return my_from_json($url, $response->content, $debug);
}
################################################################################
# Insert a session document into whichever store holds sessions (ES or
# ClickHouse). $index is the ES-style index name (e.g. tests_sessions3-26m04);
# for ClickHouse the day suffix is dropped and the doc goes into the table.
sub sessionsPost {
my ($index, $id, $doc, $debug) = @_;

    if (!$ArkimeTest::sessionsDbNonES) {
        return esPost("/$index/_doc/$id?refresh=true", $doc, $debug);
    }
    return sessionsHelper("post", $index, $id, $doc) if (!$ArkimeTest::sessionsDbCH);
    $ArkimeTest::sessionsDbUrl =~ m{^(clickhouses?|chttps?)://(.+)$};

    my $chUrl = ($1 eq "clickhouses" || $1 eq "chttps" ? "https" : "http") . "://$2";
    my $obj = from_json($doc);
    $obj->{'_id'} = $id;
    (my $table = $index) =~ s/-[^-]*$//;
    my $sql = "INSERT INTO arkime.$table (fields, \`_id\`, lastPacket) SELECT data, CAST(data.\`_id\` AS String), fromUnixTimestamp64Milli(toInt64OrZero(toString(data.lastPacket))) FROM input('data JSON') FORMAT JSONAsObject " . to_json($obj);
    my $response = $ArkimeTest::userAgent->post("$chUrl/", Content => $sql, "Content-Type" => "text/plain;charset=UTF-8");
    diag $response->content if ($debug || !$response->is_success);
    return $response->is_success;
}
################################################################################
# A session as stored in whichever store holds sessions, through the viewer on port
# so it is that viewer's sessions db and prefix. Answers like an ES doc get.
sub sessionGet {
my ($port, $sid) = @_;

    my $response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:$port/regressionTests/session/" . uri_escape($sid));
    return my_from_json("session $sid", $response->content);
}

sub sessionDelete {
my ($port, $sid) = @_;

    my $response = $ArkimeTest::userAgent->request(HTTP::Request::Common::_simple_req("DELETE", "http://$ArkimeTest::host:$port/regressionTests/session/" . uri_escape($sid)));
    return my_from_json("session $sid", $response->content);
}
################################################################################
# Delete session documents carrying a tag from whichever store holds sessions.
sub sessionsDeleteByTag {
my ($index, $tag, $debug) = @_;

    if (!$ArkimeTest::sessionsDbNonES) {
        return esPost("/$index/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "term": { "tags": "' . $tag . '" } } }', $debug);
    }
    return sessionsHelper("deleteByTag", $index, $tag) if (!$ArkimeTest::sessionsDbCH);
    $ArkimeTest::sessionsDbUrl =~ m{^(clickhouses?|chttps?)://(.+)$};

    my $chUrl = ($1 eq "clickhouses" || $1 eq "chttps" ? "https" : "http") . "://$2";
    (my $table = $index) =~ s/-[^-]*$//;
    my $sql = "DELETE FROM arkime.$table WHERE has(fields.tags, '$tag')";
    my $response = $ArkimeTest::userAgent->post("$chUrl/", Content => $sql, "Content-Type" => "text/plain;charset=UTF-8");
    diag $response->content if ($debug || !$response->is_success);
    return $response->is_success;
}
################################################################################
# Delete the sessions of these nodes from a sessions table/index that follows [default] sessionsDbUrl
sub sessionsDeleteByNode {
my ($index, @nodes) = @_;

    if (!$ArkimeTest::noEs) {
        return esPost("/$index/_delete_by_query?conflicts=proceed&refresh", to_json({ query => { terms => { node => \@nodes } } }));
    }
    $ENV{ARKIME_default__sessionsDbUrl} =~ m{^(clickhouses?|chttps?)://(.+)$} or die "sessionsDeleteByNode only knows ClickHouse with no ES\n";
    my $chUrl = ($1 eq "clickhouses" || $1 eq "chttps" ? "https" : "http") . "://$2";
    (my $table = $index) =~ s/-[^-]*$//;
    my $sql = "DELETE FROM arkime.$table WHERE toString(fields.node) IN (" . join(",", map { "'$_'" } @nodes) . ")";
    my $response = $ArkimeTest::userAgent->post("$chUrl/", Content => $sql, "Content-Type" => "text/plain;charset=UTF-8");
    diag $response->content if (!$response->is_success);
    return $response->is_success;
}
################################################################################
sub esPut {
my ($url, $content, $debug) = @_;

    return {} if ($ArkimeTest::noEs);

    my $response = $ArkimeTest::userAgent->put("$ArkimeTest::elasticsearch$url", Content => $content, "Content-Type" => "application/json;charset=UTF-8");
    #diag $url, " response:", $response->content;
    return my_from_json($url, $response->content, $debug);
}
################################################################################
sub esDelete {
my ($url, $debug) = @_;

    return {} if ($ArkimeTest::noEs);

    esPut("/_cluster/settings", '{ "transient": { "action.destructive_requires_name": false}}');

    my $response = $ArkimeTest::userAgent->request(HTTP::Request::Common::_simple_req("DELETE", "$ArkimeTest::elasticsearch$url"));
    #print $url, " response:", $response->content;

    esPut("/_cluster/settings", '{ "transient": { "action.destructive_requires_name": null}}');

    return my_from_json($url, $response->content, $debug);
}
################################################################################
# capture's collections (files, stats, dstats, fields, sequence) in whatever the
# cluster db is, through a viewer: 8123 for the tests prefix, 8124 for tests2
sub clusterPost {
    my ($port, $path, $body) = @_;

    my $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:$port/regressionTests/clusterDb/$path", Content => to_json($body), "Content-Type" => "application/json;charset=UTF-8");
    return my_from_json($path, $response->content);
}

# { where, sort, size }, the result is shaped like an ES search
sub clusterSearch {
    my ($port, $coll, $query) = @_;
    my $result = clusterPost($port, "$coll/find", $query // {});
    return {hits => {total => $result->{total}, hits => [map { {_id => $_->{id}, _source => $_->{doc}} } @{$result->{data}}]}};
}

sub clusterPut {
    my ($port, $coll, $id, $doc) = @_;
    return clusterPost($port, "$coll/put", [{id => $id, doc => (ref $doc ? $doc : from_json($doc))}]);
}

sub clusterDelete {
    my ($port, $coll, $id) = @_;
    my $response = $ArkimeTest::userAgent->request(HTTP::Request::Common::_simple_req("DELETE", "http://$ArkimeTest::host:$port/regressionTests/clusterDb/$coll/$id"));
    return my_from_json("$coll/$id", $response->content);
}

sub clusterDeleteWhere {
    my ($port, $coll, $where) = @_;
    return clusterPost($port, "$coll/deleteWhere", {where => $where});
}

sub clusterCopy {
    my ($srcPort, $dstPort, $coll) = @_;
    my $docs = clusterPost($srcPort, "$coll/find", {})->{data};
    return clusterPost($dstPort, "$coll/put", [map { {id => $_->{id}, doc => $_->{doc}} } @{$docs}]);
}

################################################################################
sub countTest {
my ($count, $test, $debug) = @_;
    local $Test::Builder::Level = $Test::Builder::Level + 1;
    my $json = viewerGet("/sessions.json?$test");
    if ($json->{recordsFiltered} != $count || scalar @{$json->{data}} != $count) {
        diag "Failed Test " . uri_unescape($test);
    }
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
    my $response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8008$url", "x-parliament-cookie" => $token);
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
# Call $check every 100ms until it returns true or $timeout seconds (default 10)
# pass, use instead of a fixed sleep when waiting on something the server
# writes after it responds
sub waitUntil {
my ($check, $timeout) = @_;
    $timeout //= 10;
    my $end = Time::HiRes::time() + $timeout;
    while (1) {
        return 1 if ($check->());
        last if (Time::HiRes::time() >= $end);
        Time::HiRes::sleep(0.1);
    }
    diag "waitUntil timed out after ${timeout}s";
    return 0;
}
################################################################################
sub waitFor {
my ($host, $port, $extraSleep) = @_;
    print "Connecting $host:$port\n" if ($main::debug);
    my $deadline = time() + 20;
    while (1) {
        die "Nothing listening on $host:$port after 20s, it probably failed to start\n" if (time() > $deadline);
        my $sock = IO::Socket::INET->new(
	    PeerAddr => $host,
	    PeerPort => $port,
	    Proto    => 'tcp'
	);
        if ($sock) {
            print "Success\n" if ($main::debug);
            close($sock);
            sleep ($extraSleep) if (defined $extraSleep);
            return;
        };
        sleep 1;
    }
}
################################################################################
sub addUser {
    #diag ("cd ../viewer ; node addUser.js $ArkimeTest::es -c ../tests/config.test.ini $_[0]\n");
    my @es_args = (
        @ArkimeTest::dbArgs,
        '-o', "usersElasticsearch=$ArkimeTest::usersElasticsearch",
    );
    push @es_args, $ENV{INSECURE} if defined $ENV{INSECURE} && $ENV{INSECURE} ne '';
    my @extras = shellwords($_[0] // '');
    my $pid = fork();
    die "fork: $!" if !defined $pid;
    if ($pid == 0) {
        chdir('../viewer') or die "chdir ../viewer: $!";
        exec('node', 'addUser.js', @es_args, '-c', '../tests/config.test.ini', @extras);
        die "exec: $!";
    }
    waitpid($pid, 0);
    diag("addUser.js $_[0] exited " . ($? >> 8)) if $?;
    return $?;
}

# Run common/changePasswordSecret.js with the old/new secret given on the
# command line and return its combined stdout/stderr so callers can check the
# UPDATE/SKIP log lines.
sub changePasswordSecret {
    my ($old, $new) = @_;
    my @es_args = (
        @ArkimeTest::dbArgs,
        '-o', "usersElasticsearch=$ArkimeTest::usersElasticsearch",
        '-o', "prefix=tests",
    );
    push @es_args, $ENV{INSECURE} if defined $ENV{INSECURE} && $ENV{INSECURE} ne '';

    my $pid = open(my $fh, "-|");
    die "fork: $!" if !defined $pid;
    if ($pid == 0) {
        chdir('../common') or die "chdir ../common: $!";
        open(STDERR, ">&", STDOUT);
        exec('node', 'changePasswordSecret.js', @es_args, '-c', '../tests/config.test.ini', $old, $new);
        die "exec: $!";
    }
    local $/;
    my $out = <$fh>;
    close($fh);
    return $out;
}
################################################################################
sub clearIndex {
my ($index) = @_;
    # A cluster db collection, wherever the cluster db is
    if ($ArkimeTest::clusterDbNonES && $index =~ /^(tests2?)_(files|stats|dstats|fields|sequence|hunts|queries)$/) {
        return clusterDeleteWhere($1 eq "tests2" ? 8124 : 8123, $2, {});
    }
    # tests.pl --splitprefix gives the cluster db indices <prefix>cl_
    $index =~ s/^(tests2?)_(files|stats|dstats|fields|sequence|hunts|queries|history|featherprint)/$1cl_$2/ if ($ENV{ARKIME_TEST_SPLIT});
    esGet("/_flush");
    esGet("/_refresh");
    return esPost("/$index/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }');
}
################################################################################
# TOTP helper functions
use Digest::SHA qw(hmac_sha1);

sub _decode_base32 {
    my ($input) = @_;
    my $alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ234567';
    my %lookup = map { substr($alphabet, $_, 1) => $_ } 0..31;
    $input = uc($input);
    $input =~ s/=+$//;
    my $bits = '';
    for my $char (split //, $input) {
        $bits .= sprintf('%05b', $lookup{$char});
    }
    my $bytes = '';
    while (length($bits) >= 8) {
        $bytes .= chr(oct('0b' . substr($bits, 0, 8, '')));
    }
    return $bytes;
}

sub generate_totp {
    my ($secret) = @_;
    my $time = int(time() / 30);
    my $key = _decode_base32($secret);
    my $msg = pack('N*', 0, $time);
    my $hash = hmac_sha1($msg, $key);
    my $offset = ord(substr($hash, -1)) & 0x0F;
    my $code = (unpack('N', substr($hash, $offset, 4)) & 0x7FFFFFFF) % 1000000;
    return sprintf('%06d', $code);
}

################################################################################
# A timedSystem command still running after this many seconds has hung
$ArkimeTest::hungSecs = $ENV{ARKIME_TEST_HUNG_SECS} // 120;
# Optional sub a test sets to add to the report of a hung command
$ArkimeTest::hungExtra = undef;

# Says where a hung command is stuck: its log and each of its processes'
# threads, since nothing else survives a CI job timeout
sub hungReport {
my ($pgid, $cmd) = @_;
    diag "HUNG [>${ArkimeTest::hungSecs}s] $cmd";
    my ($log) = $cmd =~ m{> (/tmp/\S+) 2>&1};
    diag "--- last of $log\n" . `tail -40 $log 2>&1` if ($log && -s $log);
    $ArkimeTest::hungExtra->($pgid, $cmd) if ($ArkimeTest::hungExtra);

    foreach my $proc (hungProcs($pgid)) {
        my ($pid, $etime, $args) = @$proc;
        diag "--- pid $pid running $etime: " . substr($args, 0, 200);
        # thread name, state, the syscall it sits in (202 futex: waiting on a lock, 7/271 poll) and kernel wait
        foreach my $task (glob("/proc/$pid/task/*")) {
            my $comm = hungRead("$task/comm"); chomp $comm;
            my ($state) = hungRead("$task/stat") =~ /.*\) (\S)/s;
            my $syscall = hungRead("$task/syscall"); chomp $syscall;
            my $wchan = hungRead("$task/wchan");
            diag sprintf("    %-16s %s syscall %s%s", $comm, $state // '?', $syscall, ($wchan && $wchan ne '0') ? " wchan $wchan" : '');
        }
        hungSockets($pid);
        next if ($args !~ m{^\S*capture/capture\b});
        if (system("command -v gdb > /dev/null 2>&1") == 0) {
            diag `gdb -p $pid -batch -ex 'thread apply all bt' 2>&1 | grep -v '^\\[New LWP' | tail -150`;
        } elsif (system("command -v eu-stack > /dev/null 2>&1") == 0) {
            diag `eu-stack -p $pid 2>&1 | tail -200`;
        }
    }
}

sub hungRead {
    open(my $fh, '<', $_[0]) or return '';
    local $/;
    return <$fh> // '';
}

# The processes in a process group. /proc where there is one, ps isn't in every CI image
sub hungProcs {
my ($pgid) = @_;
    my @procs;
    if (!-d "/proc/self/task") {
        foreach my $line (`ps -eo pid=,pgid=,etime=,args=`) {
            my ($pid, $pg, $etime, $args) = $line =~ /^\s*(\d+)\s+(\d+)\s+(\S+)\s+(.*)$/ or next;
            push @procs, [$pid, $etime, $args] if ($pg == $pgid);
        }
        return @procs;
    }

    my $tck = POSIX::sysconf(POSIX::_SC_CLK_TCK()) || 100;
    my ($uptime) = hungRead("/proc/uptime") =~ /^(\S+)/;
    foreach my $dir (glob("/proc/[0-9]*")) {
        my ($rest) = hungRead("$dir/stat") =~ /.*\) (.*)$/s or next;
        # after the comm: state ppid pgrp ... starttime is the 20th
        my @f = split(' ', $rest);
        next if ($f[2] != $pgid);
        (my $args = hungRead("$dir/cmdline")) =~ s/\0/ /g;
        my ($pid) = $dir =~ m{(\d+)$};
        push @procs, [$pid, int($uptime - $f[19] / $tck) . "s", $args];
    }
    return @procs;
}

my %hungTcpStates = ('01' => 'ESTABLISHED', '02' => 'SYN_SENT', '03' => 'SYN_RECV', '04' => 'FIN_WAIT1', '05' => 'FIN_WAIT2',
                     '06' => 'TIME_WAIT', '07' => 'CLOSE', '08' => 'CLOSE_WAIT', '09' => 'LAST_ACK', '0A' => 'LISTEN', '0B' => 'CLOSING');

# /proc/net/tcp addresses are hex, each 32 bit word in little endian order
sub hungAddr {
my ($hex) = @_;
    my ($ip, $port) = split(':', $hex);
    my $bytes = join('', map { scalar reverse(pack('H8', $_)) } $ip =~ /(.{8})/g);
    my $str = length($bytes) == 4 ? join('.', unpack('C4', $bytes)) : '[' . join(':', unpack('(H4)8', $bytes)) . ']';
    return "$str:" . hex($port);
}

# A process's tcp connections. /proc, ss isn't in every CI image
sub hungSockets {
my ($pid) = @_;
    my %inodes;
    foreach my $fd (glob("/proc/$pid/fd/*")) {
        my $link = readlink($fd) // next;
        $inodes{$1} = 1 if ($link =~ /^socket:\[(\d+)\]$/);
    }
    return if (!%inodes);
    foreach my $file ("/proc/$pid/net/tcp", "/proc/$pid/net/tcp6") {
        foreach my $line (split(/\n/, hungRead($file))) {
            my @f = split(' ', $line);
            next if (@f < 10 || !$inodes{$f[9]});
            diag sprintf("    tcp %s -> %s %s", hungAddr($f[1]), hungAddr($f[2]), $hungTcpStates{$f[3]} // $f[3]);
        }
    }
}

################################################################################
# system() that says when a command took over 3s, a slow CI runner then shows
# which one. A command that never returns is reported and killed, so the test
# fails instead of the CI job timing out
sub timedSystem {
my ($cmd) = @_;
    my $start = Time::HiRes::time();
    my $pid = fork();
    die "fork failed: $!" if (!defined $pid);
    if ($pid == 0) {
        setpgrp(0, 0);
        exec("/bin/sh", "-c", $cmd) or POSIX::_exit(127);
    }
    my $rc = eval {
        local $SIG{ALRM} = sub { die "hung\n" };
        alarm($ArkimeTest::hungSecs);
        waitpid($pid, 0);
        alarm(0);
        $?;
    };
    if (!defined $rc) {
        alarm(0);
        hungReport($pid, $cmd);
        kill('KILL', -$pid);
        waitpid($pid, 0);
        die "$0: command hung, see the report above\n";
    }
    my $ms = int((Time::HiRes::time() - $start) * 1000);
    if ($ms > 3000) {
        (my $short = $cmd) =~ s/^.*?(\.\.\/capture\/capture|aws |curl |grep |diff )/$1/;
        $short =~ s/ (-o '?(users)?[Ee]lasticsearch=\S+|-o disablePython=true|-c config\.test\.ini)//g;
        diag sprintf("SLOW [%dms] %s", $ms, substr($short, 0, 200));
    }
    return $rc;
}

return 1;
