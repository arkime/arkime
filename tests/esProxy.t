# ESProxy
use Test::More tests => 37;
use ArkimeTest;
use Cwd;
use URI::Escape;
use Data::Dumper;
use Test::Differences;
use JSON -support_by_pp;
use HTTP::Request;
use strict;

my $response;

$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:7200");
is ($response->code, 401);

$response = $ArkimeTest::userAgent->get("http://test:wrong\@$ArkimeTest::host:7200");
is ($response->code, 401);

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200");
is ($response->code, 200);

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/_search");
is ($response->code, 400);
is ($response->content, "Not authorized for API");

$response = $ArkimeTest::userAgent->post("http://test:test\@$ArkimeTest::host:7200/_search");
is ($response->code, 400);
is ($response->content, "Not authorized for API");

$response = $ArkimeTest::userAgent->put("http://test:test\@$ArkimeTest::host:7200/_search");
is ($response->code, 400);
is ($response->content, "Not authorized for API");

$response = $ArkimeTest::userAgent->request(HTTP::Request::Common::DELETE("http://test:test\@$ArkimeTest::host:7200/_search"));
is ($response->code, 400);
is ($response->content, "Not authorized for API");

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/_template/sessions3_template");
is ($response->code, 400);
is ($response->content, "Not authorized for API");

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/_template/arkime_sessions3_template");
is ($response->code, 400);
is ($response->content, "Not authorized for API");

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/_template/tests_sessions3_template");
is ($response->code, 200);

# Bulk - valid index with correct prefix
my $bulk_valid = qq({"index":{"_index":"tests_sessions3-2024","_id":"1"}}\n{"field":"value"}\n);
my $req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/_bulk");
$req->header('Content-Type' => 'application/x-ndjson');
$req->content($bulk_valid);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 200, "bulk with valid prefixed sessions3 index");

# Bulk - substring match should be rejected (index contains sessions2 but wrong prefix)
my $bulk_bad_substr = qq({"index":{"_index":"evil_sessions2_hack","_id":"1"}}\n{"field":"value"}\n);
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/_bulk");
$req->header('Content-Type' => 'application/x-ndjson');
$req->content($bulk_bad_substr);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "bulk with substring-matching bad index rejected");
is ($response->content, "Not authorized for API");

# Bulk - bad prefix should be rejected
my $bulk_bad_prefix = qq({"index":{"_index":"other_sessions3-2024","_id":"1"}}\n{"field":"value"}\n);
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/_bulk");
$req->header('Content-Type' => 'application/x-ndjson');
$req->content($bulk_bad_prefix);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "bulk with wrong prefix rejected");
is ($response->content, "Not authorized for API");

# Bulk - delete to valid fields index
my $bulk_delete = qq({"delete":{"_index":"tests_fields","_id":"1"}}\n);
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/_bulk");
$req->header('Content-Type' => 'application/x-ndjson');
$req->content($bulk_delete);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 200, "bulk delete to fields index succeeds");

# Bulk - delete followed by index (validates delete doesn't skip next action)
my $bulk_delete_then_index = qq({"delete":{"_index":"tests_fields","_id":"1"}}\n{"index":{"_index":"tests_sessions3-2024","_id":"2"}}\n{"field":"value"}\n);
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/_bulk");
$req->header('Content-Type' => 'application/x-ndjson');
$req->content($bulk_delete_then_index);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 200, "bulk delete then index works correctly");

# Bulk - delete to bad index should be rejected
my $bulk_delete_bad = qq({"delete":{"_index":"evil_fields_hack","_id":"1"}}\n);
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/_bulk");
$req->header('Content-Type' => 'application/x-ndjson');
$req->content($bulk_delete_bad);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "bulk delete to bad index rejected");
is ($response->content, "Not authorized for API");

# Bulk - create to valid sessions index
my $bulk_create = qq({"create":{"_index":"tests_sessions3-2024","_id":"3"}}\n{"field":"value"}\n);
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/_bulk");
$req->header('Content-Type' => 'application/x-ndjson');
$req->content($bulk_create);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 200, "bulk create to sessions3 index succeeds");

# Bulk - update to valid fields index
my $bulk_update = qq({"update":{"_index":"tests_fields","_id":"1"}}\n{"doc":{"field":"value"}}\n);
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/_bulk");
$req->header('Content-Type' => 'application/x-ndjson');
$req->content($bulk_update);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 200, "bulk update to fields index succeeds");

# Bulk - sessions index without dash should be rejected
my $bulk_no_dash = qq({"index":{"_index":"tests_sessions3evil","_id":"1"}}\n{"field":"value"}\n);
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/_bulk");
$req->header('Content-Type' => 'application/x-ndjson');
$req->content($bulk_no_dash);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "bulk sessions index without dash rejected");
is ($response->content, "Not authorized for API");

# Bulk - create to fields index should be rejected (create only allows sessions)
my $bulk_create_fields = qq({"create":{"_index":"tests_fields","_id":"1"}}\n{"field":"value"}\n);
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/_bulk");
$req->header('Content-Type' => 'application/x-ndjson');
$req->content($bulk_create_fields);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "bulk create to fields index rejected");
is ($response->content, "Not authorized for API");

# Bulk - update to sessions index should be rejected (update only allows fields)
my $bulk_update_sessions = qq({"update":{"_index":"tests_sessions3-2024","_id":"1"}}\n{"doc":{"field":"value"}}\n);
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/_bulk");
$req->header('Content-Type' => 'application/x-ndjson');
$req->content($bulk_update_sessions);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "bulk update to sessions index rejected");
is ($response->content, "Not authorized for API");

# Bulk - invalid JSON should be rejected
my $bulk_bad_json = qq(not valid json\n);
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/_bulk");
$req->header('Content-Type' => 'application/x-ndjson');
$req->content($bulk_bad_json);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "bulk with invalid JSON rejected");
is ($response->content, "Not authorized for API");

# Bulk - multiple operations in one action line should be rejected
my $bulk_multi_op = qq({"index":{"_index":"tests_sessions3-2024","_id":"1"},"create":{"_index":"tests_sessions3-2024","_id":"2"}}\n{"field":"value"}\n);
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/_bulk");
$req->header('Content-Type' => 'application/x-ndjson');
$req->content($bulk_multi_op);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "bulk with multiple ops in one line rejected");
is ($response->content, "Not authorized for API");
