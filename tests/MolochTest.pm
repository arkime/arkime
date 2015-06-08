package MolochTest;
use Exporter;
use strict;
use Test::More;
@MolochTest::ISA = qw(Exporter);
@MolochTest::EXPORT = qw (esGet esPost esDelete esCopy viewerGet viewerGet2 viewerPost viewerPost2 countTest countTest2 errTest bin2hex getToken getToken2 mesGet mesPost multiGet);

use LWP::UserAgent;
use HTTP::Request::Common;
use JSON;
use URI::Escape;
use Data::Dumper;

$MolochTest::userAgent = LWP::UserAgent->new(timeout => 120);
$MolochTest::host = "127.0.0.1";


################################################################################
sub viewerGet {
my ($url, $debug) = @_;

    my $response = $MolochTest::userAgent->get("http://$MolochTest::host:8123$url");
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

    my $response = $MolochTest::userAgent->get("http://$MolochTest::host:9200$url");
    #print $url, " response:", $response->content;
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub esPost {
my ($url, $content) = @_;

    my $response = $MolochTest::userAgent->post("http://$MolochTest::host:9200$url", Content => $content);
    #print $url, " response:", $response->content;
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub esDelete {
my ($url) = @_;

    my $response = $MolochTest::userAgent->request(HTTP::Request::Common::_simple_req("DELETE", "http://$MolochTest::host:9200$url"));
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
            $url = "/$srci/$type/_search?scroll=10m&scroll_id=$id&size=500";
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
    my $json = viewerGet("/sessions.json?$test");
    diag Dumper($json) if ($debug);
    is ($json->{recordsFiltered}, $count, uri_unescape($test) . " recordsFiltered");
    is (scalar @{$json->{data}}, $count, uri_unescape($test) . " data count");
}
################################################################################
sub countTest2 {
my ($count, $test, $debug) = @_;
    my $json = viewerGet2("/sessions.json?$test");
    diag Dumper($json) if ($debug);
    is ($json->{recordsFiltered}, $count, uri_unescape($test) . " recordsFiltered");
    is (scalar @{$json->{data}}, $count, uri_unescape($test) . " data count");
}
################################################################################
sub errTest {
my ($test, $debug) = @_;
    my $json = viewerGet("/sessions.json?$test");
    diag Dumper($json) if ($debug);
    ok (exists $json->{bsqErr}, uri_unescape($test) . " bsqErr exists");
}
################################################################################
sub bin2hex {
    my ($data) = @_;

    return unpack("H*", $data);
}
################################################################################
sub getToken {
    my $usersPage = $MolochTest::userAgent->get("http://$MolochTest::host:8123/users")->content;
    $usersPage =~ /token.*value: "(.*)"/;
    return $1;
}
################################################################################
sub getToken2 {
    my $usersPage = $MolochTest::userAgent->get("http://$MolochTest::host:8124/users")->content;
    $usersPage =~ /token.*value: "(.*)"/;
    return $1;
}
################################################################################

