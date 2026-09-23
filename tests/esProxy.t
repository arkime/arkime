# ESProxy
use Test::More tests => 89;
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

# A sensor name that resolves on Object.prototype is not a configured sensor. It
# has no pass and no ip, so a plain [] lookup would authenticate it with any
# password and proxy its requests (user docs, session docs) to elasticsearch.
foreach my $name ("__proto__", "constructor", "toString", "hasOwnProperty") {
    $response = $ArkimeTest::userAgent->get("http://$name:anything\@$ArkimeTest::host:7200/tests_users/_doc/formtestuser");
    is ($response->code, 401, "sensor '$name' is not authenticated");
}

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

# Sessions search - rootId term query is allowed
my $search_rootid = qq({"size":1000,"_source":["rootId"],"query":{"term":{"rootId":"240101-abc"}}});
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_sessions3-2024/_search");
$req->header('Content-Type' => 'application/json');
$req->content($search_rootid);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 200, "sessions search by rootId allowed");

# Sessions search - rootId filter with the user's forced expression ANDed on is allowed
my $search_rootid_expr = qq({"size":1000,"_source":["rootId"],"query":{"bool":{"filter":[{"term":{"rootId":"240101-abc"}},{"term":{"tags":"corp"}}]}}});
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_sessions3-2024/_search");
$req->header('Content-Type' => 'application/json');
$req->content($search_rootid_expr);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 200, "sessions search by rootId filter allowed");

# Sessions search - a filter not starting with the rootId term is rejected
my $search_no_rootid = qq({"query":{"bool":{"filter":[{"term":{"node":"foo"}}]}}});
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_sessions3-2024/_search");
$req->header('Content-Type' => 'application/json');
$req->content($search_no_rootid);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "sessions search by filter without rootId rejected");
is ($response->content, "Not authorized for API");

# Sessions search - a should next to the rootId filter could widen it, so is rejected
my $search_should = qq({"query":{"bool":{"filter":[{"term":{"rootId":"240101-abc"}}],"should":[{"term":{"node":"foo"}}]}}});
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_sessions3-2024/_search");
$req->header('Content-Type' => 'application/json');
$req->content($search_should);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "sessions search with should next to rootId filter rejected");
is ($response->content, "Not authorized for API");

# Sessions search - term on any other field is rejected
my $search_other = qq({"query":{"term":{"node":"foo"}}});
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_sessions3-2024/_search");
$req->header('Content-Type' => 'application/json');
$req->content($search_other);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "sessions search by other term rejected");
is ($response->content, "Not authorized for API");

# Sessions search - rootId alongside another clause is rejected
my $search_extra = qq({"query":{"term":{"rootId":"240101-abc"},"match_all":{}}});
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_sessions3-2024/_search");
$req->header('Content-Type' => 'application/json');
$req->content($search_extra);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "sessions search with extra query clause rejected");
is ($response->content, "Not authorized for API");

# path confusion: the guard checks a path decoded by Express
# (req.params['0']) while the proxied request uses the raw, still-encoded url
# (req.url); a %3f/%23 makes the two resolve to different endpoints.

# GET - guard sees the decoded string terminate at %3f, matching the
# allowlisted /_cat/health; the raw url actually collapses to /_search
$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/_cat/health%3fz/../../_search");
is ($response->code, 400, "GET path confusion via %3f rejected");

# GET - same bypass using %23 (#) as the terminator
$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/_cat/health%23z/../../_search");
is ($response->code, 400, "GET path confusion via %23 rejected");

# POST - guard matches the exact-match allowlist entry /tests_stats/_search;
# the raw url actually collapses to /_bulk, skipping validateBulk() entirely
my $bulk_bad_index = qq({"index":{"_index":"evil_index","_id":"1"}}\n{"field":"value"}\n);
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_stats/_search%3f/../../_bulk");
$req->header('Content-Type' => 'application/x-ndjson');
$req->content($bulk_bad_index);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "POST path confusion into _bulk rejected");

# DELETE - a startsWith() guard is fooled the same way: the decoded id looks
# like it starts with the sensor's own doc prefix, but the raw url collapses
# to a different index entirely
$response = $ArkimeTest::userAgent->request(HTTP::Request::Common::DELETE("http://test:test\@$ArkimeTest::host:7200/tests_files/_doc/test-%3fz/../../../tests_sessions3-2024"));
is ($response->code, 400, "DELETE path confusion to a different index rejected");

# query strings: capture sends these exact urls through esProxy and the guard
# must keep allowing them. Express strips the query before the wildcard param,
# so normalizeUrlPath() only sees a ?/# when it arrived percent-encoded; these
# catch a guard change (say, checking req.url) that would lock capture out.

# GET - getExact entries, see arkime_db_health_check, arkime_db_load_fields,
# and the template _meta check in capture/db.c
$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/_cat/health?format=json");
is ($response->code, 200, "GET _cat/health with format query allowed");

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tests_fields/_search?size=3000");
is ($response->code, 200, "GET fields _search with size query allowed");

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/_template/tests_sessions3_template?filter_path=**._meta");
is ($response->code, 200, "GET sessions3 template with filter_path query allowed");

# POST - exact path compare; capture writes its stats doc with an external
# version that it bumps from the current one, so do the same and re-post the
# existing source to leave the doc unchanged
$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tests_stats/_doc/test");
is ($response->code, 200, "GET stats doc allowed");
my $stats = from_json($response->content);
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_stats/_doc/test?version_type=external&version=" . ($stats->{_version} + 1));
$req->header('Content-Type' => 'application/json');
$req->content(to_json($stats->{_source}));
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 200, "POST stats doc with version query allowed");

# POST - exact path compare; capture creates the fn sequence with version 100,
# ES answers 409 once the sequence has advanced past that, but never the
# guard's 400
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_sequence/_doc/fn-test?version_type=external&version=100");
$req->header('Content-Type' => 'application/json');
$req->content("{}");
$response = $ArkimeTest::userAgent->request($req);
ok ($response->code == 200 || $response->code == 201 || $response->code == 409, "POST sequence doc with version query allowed") or diag($response->code);
isnt ($response->content, "Not authorized for API", "POST sequence doc with version query not rejected by guard");

# POST/DELETE - startsWith compare; capture creates file docs with refresh=true
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_files/_doc/test-99999?refresh=true");
$req->header('Content-Type' => 'application/json');
$req->content(qq({"node":"test","num":99999,"name":"/tmp/esProxy-query.pcap","first":0,"locked":0}));
$response = $ArkimeTest::userAgent->request($req);
ok ($response->code == 200 || $response->code == 201, "POST files doc with refresh query allowed") or diag($response->code);

# Files _update - capture updates its own file docs as they close
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_files/_update/test-99999");
$req->header('Content-Type' => 'application/json');
$req->content(qq({"doc": {"filesize": 1234, "packets": 5}}));
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 200, "POST own files update allowed") or diag($response->content);

$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_files/_update/test2-99999");
$req->header('Content-Type' => 'application/json');
$req->content(qq({"doc": {"filesize": 1234}}));
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "POST files update for other node rejected");
is ($response->content, "Not authorized for API");

$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_files/_update/test-99999");
$req->header('Content-Type' => 'application/json');
$req->content(qq({"doc": {"node": "test2"}}));
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "POST files update changing node rejected");
is ($response->content, "Not authorized for API");

$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_files/_update/test-99999");
$req->header('Content-Type' => 'application/json');
$req->content(qq({"script": {"source": "ctx._source.node='test2'"}}));
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "POST files update with script rejected");
is ($response->content, "Not authorized for API");

$response = $ArkimeTest::userAgent->request(HTTP::Request::Common::DELETE("http://test:test\@$ArkimeTest::host:7200/tests_files/_doc/test-99999"));
is ($response->code, 200, "DELETE files doc allowed");

# Node "test" must not touch files docs of a node whose name starts with "test"
$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tests_files/_doc/test2-1");
is ($response->code, 400, "GET file doc for sibling node name (test2) rejected");
is ($response->content, "Not authorized for API");

$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_files/_doc/test2-1");
$req->header('Content-Type' => 'application/json');
$req->content(qq({"node":"test2","name":"evil.pcap"}));
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "POST file doc replace for sibling node name (test2) rejected");
is ($response->content, "Not authorized for API");

# Same for dstats docs
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_dstats/_doc/test2-59-60");
$req->header('Content-Type' => 'application/json');
$req->content(qq({"node":"test2","interval":60}));
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "POST dstats doc for sibling node name (test2) rejected");
is ($response->content, "Not authorized for API");

$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_dstats/_doc/test-59-60?refresh=true");
$req->header('Content-Type' => 'application/json');
$req->content(qq({"node":"test","interval":60}));
$response = $ArkimeTest::userAgent->request($req);
ok ($response->code == 200 || $response->code == 201, "POST own dstats doc allowed") or diag($response->code);

# Or a node name that is dash-joined onto it
$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tests_files/_doc/test-x-1");
is ($response->code, 400, "GET file doc for dash-joined sibling name (test-x) rejected");
is ($response->content, "Not authorized for API");

$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_files/_doc/test-x-1");
$req->header('Content-Type' => 'application/json');
$req->content(qq({"node":"test-x","name":"evil.pcap"}));
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "POST file doc replace for dash-joined sibling name (test-x) rejected");
is ($response->content, "Not authorized for API");

# Session doc get/update must use the configured prefix
$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/other_sessions3-2024/_doc/1");
is ($response->code, 400, "GET session doc under unrelated sessions3 prefix rejected");
is ($response->content, "Not authorized for API");

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/other_sessions2-2024/_doc/1");
is ($response->code, 400, "GET session doc under unrelated sessions2 prefix rejected");
is ($response->content, "Not authorized for API");

my $evil_update = qq({"script":{"source":"ctx._source.marker='evil'"}});
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/other_sessions3-2024/_update/1");
$req->header('Content-Type' => 'application/json');
$req->content($evil_update);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "POST session update under unrelated sessions3 prefix rejected");
is ($response->content, "Not authorized for API");

# Correct prefix is still proxied
$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tests_sessions3-2024/_doc/nonexistent-id");
isnt ($response->content, "Not authorized for API", "GET session doc under correct prefix is proxied to ES, not blocked by guard");
