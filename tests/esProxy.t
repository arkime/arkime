# ESProxy
use Test::More tests => 16;
use MolochTest;
use Cwd;
use URI::Escape;
use Data::Dumper;
use Test::Differences;
use JSON -support_by_pp;
use strict;

my $response;

$response = $MolochTest::userAgent->get("http://$MolochTest::host:7200");
is ($response->code, 401);

$response = $MolochTest::userAgent->get("http://test:wrong\@$MolochTest::host:7200");
is ($response->code, 401);

$response = $MolochTest::userAgent->get("http://test:test\@$MolochTest::host:7200");
is ($response->code, 200);

$response = $MolochTest::userAgent->get("http://test:test\@$MolochTest::host:7200/_search");
is ($response->code, 400);
is ($response->content, "Not authorized for API");

$response = $MolochTest::userAgent->post("http://test:test\@$MolochTest::host:7200/_search");
is ($response->code, 400);
is ($response->content, "Not authorized for API");

$response = $MolochTest::userAgent->put("http://test:test\@$MolochTest::host:7200/_search");
is ($response->code, 400);
is ($response->content, "Not authorized for API");

$response = $MolochTest::userAgent->request(HTTP::Request::Common::DELETE("http://test:test\@$MolochTest::host:7200/_search"));
is ($response->code, 400);
is ($response->content, "Not authorized for API");

$response = $MolochTest::userAgent->get("http://test:test\@$MolochTest::host:7200/_template/sessions3_template");
is ($response->code, 400);
is ($response->content, "Not authorized for API");

$response = $MolochTest::userAgent->get("http://test:test\@$MolochTest::host:7200/_template/arkime_sessions3_template");
is ($response->code, 400);
is ($response->content, "Not authorized for API");

$response = $MolochTest::userAgent->get("http://test:test\@$MolochTest::host:7200/_template/tests_sessions3_template");
is ($response->code, 200);
