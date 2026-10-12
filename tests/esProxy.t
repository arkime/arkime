# ESProxy
use Test::More tests => 196;
use ArkimeTest;
use Cwd;
use URI::Escape;
use Data::Dumper;
use Test::Differences;
use JSON -support_by_pp;
use HTTP::Request;
use IO::Socket::INET;
use MIME::Base64;
use IO::Compress::Gzip;
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

# Bulk - _index must be a single sessions index name, same check as the _search and _doc paths
foreach my $test (
    ["tests_sessions3-2024,tests_users", "comma list"],
    ["tests_sessions3-*", "wildcard"],
    ["tests_sessions3-2024:tests_users", "remote cluster"],
    ["tests_sessions3-2024/../tests_users", "slash"],
    ["tests_sessions3-", "empty suffix"],
) {
    my ($index, $name) = @$test;
    $req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/_bulk");
    $req->header('Content-Type' => 'application/x-ndjson');
    $req->content(qq({"index":{"_index":"$index","_id":"1"}}\n{"field":"value"}\n));
    $response = $ArkimeTest::userAgent->request($req);
    is ($response->code, 400, "bulk index with $name rejected");
}

$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/_bulk");
$req->header('Content-Type' => 'application/x-ndjson');
$req->content(qq({"create":{"_index":1,"_id":"1"}}\n{"field":"value"}\n));
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "bulk create with non string _index rejected");

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
my $bulk_update = qq({"update":{"_index":"tests_fields","_id":"1"}}\n{"doc":{"help":"value"}}\n);
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/_bulk");
$req->header('Content-Type' => 'application/x-ndjson');
$req->content($bulk_update);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 200, "bulk update to fields index succeeds");

# Field definitions control how viewers compile queries, a sensor can add fields but not redefine them
sub bulkPost {
    my ($user, $body, $gzip) = @_;
    my $r = HTTP::Request->new('POST', "http://$user:$user\@$ArkimeTest::host:7200/_bulk");
    $r->header('Content-Type' => 'application/x-ndjson');
    if ($gzip) {
        my $gz;
        IO::Compress::Gzip::gzip(\$body => \$gz);
        $r->header('Content-Encoding' => 'gzip');
        $body = $gz;
    }
    $r->content($body);
    return $ArkimeTest::userAgent->request($r);
}
my $nodeField = esGet("/tests_fields/_doc/node")->{_source};

# capture logs any error, so a bulk with nothing left after dropping keys is answered as a success
$response = bulkPost("test", qq({"update":{"_index":"tests_fields","_id":"node"}}\n{"doc":{"dbField2":"esproxyhack"}}\n));
is ($response->code, 200, "bulk field update of only dbField2 answered");
like ($response->content, qr/"items":\[\]/, "bulk field update of only dbField2 not applied");
$response = bulkPost("test", qq({"update":{"_index":"tests_fields","_id":"node"}}\n{"doc":{"help":"esproxy test help","transform":"utf8ToHex"}}\n));
is ($response->code, 200, "bulk field update keeps allowed keys");
$response = bulkPost("test", qq({"index":{"_index":"tests_fields","_id":"node"}}\n{"friendlyName":"x","group":"general","help":"x","type":"termfield","dbField2":"esproxyhack"}\n), 1);
like ($response->content, qr/"errors":\s*true/, "bulk field index of existing field not applied");
esGet("/_refresh");
my $nodeAfter = esGet("/tests_fields/_doc/node")->{_source};
is ($nodeAfter->{dbField2}, "node", "existing field dbField2 unchanged");
ok (!exists $nodeAfter->{transform}, "existing field transform unchanged");
is ($nodeAfter->{help}, "esproxy test help", "existing field help updated");
esPut("/tests_fields/_doc/node?refresh=true", to_json($nodeField));

# capture's first add of a field goes through with every key, only later changes are limited
$response = bulkPost("test", qq({"index":{"_index":"tests_fields","_id":"esproxy.newfield"}}\n{"friendlyName":"x","group":"general","help":"x","type":"termfield","dbField2":"esproxynewfield","transform":"dash2Colon"}\n), 1);
like ($response->content, qr/"errors":\s*false/, "bulk field index of new field allowed");
esGet("/_refresh");
is (esGet("/tests_fields/_doc/esproxy.newfield")->{_source}->{transform}, "dash2Colon", "new field created with its transform");
$response = bulkPost("test", qq({"delete":{"_index":"tests_fields","_id":"esproxy.newfield"}}\n));
is ($response->code, 200, "bulk field delete allowed");

$response = bulkPost("test", qq({"delete":{"_index":"tests_fieldsevil","_id":"1"}}\n));
is ($response->code, 400, "bulk delete to index starting with fields rejected");

$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_fields/_doc/node");
$req->header('Content-Type' => 'application/json');
$req->content(to_json({%$nodeField, dbField2 => "esproxyhack"}));
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "POST fields doc rejected");

# A small gzip body that inflates past the limit is refused instead of blocking esProxy
$response = bulkPost("test", "\n" x (40 * 1024 * 1024), 1);
is ($response->code, 400, "bulk gzip bomb rejected");

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

# Sessions search - every index must be a sessions index and the ids body must be a single id lookup.
# The bodies, query params and %2C encoded index lists are what the sensor viewer's es client sends,
# Db.getSession and getEntirePCAP through Db.search/Db.searchSessions.
my $search_ids = qq({"query":{"ids":{"values":["240101-abc"]}},"_source":["cert","dns","zeekintel"],"fields":["*"],"profile":false});
my $search_entire = qq({"size":1000,"_source":["rootId"],"sort":{"lastPacket":{"order":"asc"}},"query":{"bool":{"filter":[{"term":{"rootId":"240101-abc"}}]}},"profile":false});
my $search_ids_aggs = qq({"query":{"ids":{"values":["240101-abc"]}},"size":0,"aggs":{"u":{"terms":{"field":"userId"}}}});
my $search_ids_two = qq({"query":{"ids":{"values":["240101-abc"]},"match_all":{}}});
my $session_params = "rest_total_hits_as_int=true";
my $entire_params = "preference=primaries&ignore_unavailable=true&rest_total_hits_as_int=true";
foreach my $test (
    ["tests_sessions3-2024/_search?$session_params", $search_ids, 200, "sessions search by id allowed"],
    ["tests_sessions2-2024%2Ctests_sessions3-2024/_search?$session_params", $search_ids, 200, "sessions search by id in sessions2 and sessions3 allowed"],
    ["partial-tests_sessions3-2024/_search?$session_params", $search_ids, 200, "sessions search by id in partial index allowed"],
    ["tests_sessions2-*%2Ctests_sessions3-*/_search?$entire_params", $search_entire, 200, "sessions search by rootId across all sessions allowed"],
    ["tests_sessions2-*%2Ctests_sessions3-*/_search?$entire_params&max_concurrent_shard_requests=5", $search_entire, 200, "sessions search by rootId with max_concurrent_shard_requests allowed"],
    ["tests_sessions3-2024,tests_users/_search", $search_ids, 400, "sessions search with non sessions index rejected"],
    ["tests_sessions3-2024%2Ctests_users/_search", $search_ids, 400, "sessions search with encoded non sessions index rejected"],
    ["tests_sessions3-2024%252Ctests_users/_search", $search_ids, 400, "sessions search with double encoded comma rejected"],
    ["tests_sessions3-2024:tests_users/_search", $search_ids, 400, "sessions search with remote cluster rejected"],
    ["tests_sessions3-2024%2C-tests_sessions3-2024/_search", $search_ids, 400, "sessions search with index exclusion rejected"],
    ["tests_users%2Ctests_sessions3-2024/_search", $search_entire, 400, "sessions search starting with non sessions index rejected"],
    ["tests_sessions3-2024/_search", $search_ids_aggs, 400, "sessions search by id with aggs rejected"],
    ["tests_sessions3-2024/_search", $search_ids_two, 400, "sessions search by id with extra query clause rejected"],
) {
    my ($path, $body, $code, $name) = @$test;
    $req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/$path");
    $req->header('Content-Type' => 'application/json');
    $req->content($body);
    $response = $ArkimeTest::userAgent->request($req);
    is ($response->code, $code, $name);
}

# path confusion: if the guard checks a path decoded by Express
# (req.params['0']) while the proxied request uses the raw, still-encoded url
# (req.url), a %3f/%23 makes the two resolve to different endpoints.

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

# startsWith() allowlist entries, the %3f/%23 lands in the guard's search/hash
$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tagger%3f/../../tests_users/_search");
is ($response->code, 400, "GET /tagger path confusion via %3f rejected");

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tagger%23/../../tests_users/_search");
is ($response->code, 400, "GET /tagger path confusion via %23 rejected");

# GET /tagger must be the tagger index only, not a multi-index expression
$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tagger,tests_users/_search");
is ($response->code, 400, "GET /tagger multi-index rejected");

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tagger*/_search");
is ($response->code, 400, "GET /tagger wildcard rejected");

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tagger/_mapping");
is ($response->code, 400, "GET /tagger other api rejected");

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tagger/_search?_source=md5&size=999");
is ($response->code, 200, "GET /tagger/_search allowed");

$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tagger%3f/../../tests_users/_count");
$req->header('Content-Type' => 'application/json');
$req->content('{}');
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "POST /tagger path confusion via %3f rejected");

# capture never POSTs to tagger
foreach my $path ("/tagger/_search", "/tagger,tests_users/_delete_by_query", "/tagger/_bulk") {
    $req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200$path");
    $req->header('Content-Type' => 'application/json');
    $req->content('{}');
    $response = $ArkimeTest::userAgent->request($req);
    is ($response->code, 400, "POST $path rejected");
}

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tests_users/_doc/%3f/../../../_cluster/settings");
is ($response->code, 400, "GET users doc path confusion into _cluster rejected");

# A sensor gets a user doc for s2s auth, but never its secrets
esPut("/tests_users/_doc/esproxysecrets?refresh=true", '{"userId":"esproxysecrets","userName":"esproxysecrets","enabled":true,"roles":["arkimeAdmin"],"passStore":"SECRETPASS","totpSecret":"SECRETTOTP","cont3xt":{"keys":"SECRETKEYS"}}');
$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tests_users/_doc/esproxysecrets");
is ($response->code, 200, "GET users doc allowed");
my $userDoc = eval { from_json($response->content)->{_source} } // {};
is ($userDoc->{userId}, "esproxysecrets", "users doc has userId");
eq_or_diff ($userDoc->{roles}, ["arkimeAdmin"], "users doc has roles");
ok (!exists $userDoc->{passStore}, "users doc has no passStore");
ok (!exists $userDoc->{totpSecret}, "users doc has no totpSecret");
ok (!exists $userDoc->{cont3xt}, "users doc has no cont3xt");

foreach my $param ("_source=passStore", "_source=passStore,totpSecret,cont3xt", "_source=*", "filter_path=_source.passStore") {
    $response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tests_users/_doc/esproxysecrets?$param");
    is ($response->code, 200, "GET users doc with $param allowed");
    unlike ($response->content, qr/SECRET/, "users doc with $param has no secrets");
}

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tests_users/_doc/esproxysecrets/_source");
is ($response->code, 400, "GET users doc with extra segment rejected");
esDelete("/tests_users/_doc/esproxysecrets?refresh=true");

# raw socket, LWP would escape the backslashes
sub rawStatus {
    my ($method, $path) = @_;
    my $sock = IO::Socket::INET->new(PeerAddr => $ArkimeTest::host, PeerPort => 7200, Proto => 'tcp') or return 0;
    print $sock "$method $path HTTP/1.0\r\nHost: $ArkimeTest::host\r\nAuthorization: Basic " . encode_base64("test:test", "") . "\r\nConnection: close\r\n\r\n";
    local $/;
    my $resp = <$sock>;
    close $sock;
    return ($resp =~ m{^HTTP/\S+ (\d+)})[0];
}

is (rawStatus("GET", "/tests_files/_doc/test-1"), 200, "raw socket own files doc allowed");

# History writes only to our own prefix's weekly history index
foreach my $test (["tests_history_v1-26w41", 1], ["other_history_v1-26w41", 0], ["tests_history_v1-x", 0], ["tests_users", 0]) {
    my ($index, $allowed) = @$test;
    $req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/$index/_doc");
    $req->header('Content-Type' => 'application/json');
    $req->content('{"userId":"esproxyhistory","timestamp":1,"api":"/esproxy"}');
    $response = $ArkimeTest::userAgent->request($req);
    if ($allowed) {
        ok ($response->code == 200 || $response->code == 201, "history write to $index allowed") or diag($response->code);
        my $docId = eval { from_json($response->content)->{_id} };
        esDelete("/$index/_doc/$docId?refresh=true") if $docId;
    } else {
        is ($response->code, 400, "history write to $index rejected");
    }
}

# A content-length the client never sends must not be passed on, es would wait for it with a shared socket
{
    my $sock = IO::Socket::INET->new(PeerAddr => $ArkimeTest::host, PeerPort => 7200, Proto => 'tcp', Timeout => 10);
    print $sock "GET /tests_stats/_doc/test HTTP/1.1\r\nHost: $ArkimeTest::host\r\nAuthorization: Basic " . encode_base64("test:test", "") . "\r\nContent-Length: 1\r\nConnection: close\r\n\r\n";
    my $resp = '';
    eval {
        local $SIG{ALRM} = sub { die "timeout\n" };
        alarm 10;
        local $/;
        $resp = <$sock> // '';
        alarm 0;
    };
    close $sock;
    like ($resp, qr{^HTTP/\S+ 200}, "GET with unsent content-length answered");
}

# A body over the limit is refused rather than truncated
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/_bulk");
$req->header('Content-Type' => 'application/x-ndjson');
$req->content("\n" x (32 * 1024 * 1024 + 1));
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 413, "bulk over the body limit rejected");

# Files search, only the node filter shapes capture and the sensor viewer send
sub filesSearch {
    my ($user, $body, $path) = @_;
    my $r = HTTP::Request->new('POST', "http://$user:$user\@$ArkimeTest::host:7200/" . ($path // "tests_files/_search"));
    $r->header('Content-Type' => 'application/json');
    $r->content($body);
    return $ArkimeTest::userAgent->request($r)->code;
}
is (filesSearch("test", '{"size":1,"sort":[{"num":"desc"}],"query":{"bool":{"filter":[{"terms":{"node":["test"]}},{"term":{"name":"/x"}}]}}}'), 200, "files search for own node allowed");
is (filesSearch("test", '{"size":1,"query":{"bool":{"filter":[{"terms":{"node":["test"]}}]}},"aggs":{"all":{"global":{},"aggs":{"n":{"terms":{"field":"node"}}}}}}'), 400, "files search with aggs rejected");
is (filesSearch("test", '{"size":1,"query":{"bool":{"filter":[{"terms":{"node":["test"]}}]}}}', "tests_files/_search/template"), 400, "files search template path rejected");
is (filesSearch("esproxyhost", '{"size":1,"query":{"bool":{"filter":[{"terms":{"node":["esproxyhost","test"]}}]}}}'), 400, "files search for a node on another host rejected");
is (filesSearch("esproxyhost", '{"size":1,"query":{"bool":{"filter":[{"terms":{"node":["esproxyhost"]}}]}}}'), 200, "files search for own node with host allowed");
# WHATWG URL turns the backslashes into slashes, isOwnFilesDoc only looked for /
is (rawStatus("DELETE", "/tests_files/_doc/test-5%3f\\..\\..\\..\\tests_esproxy_nosuchindex"), 400, "DELETE backslash path confusion rejected");

# query strings: capture sends these exact urls through esProxy and the guard
# must keep allowing them. Only query param names capture and the sensor viewer
# send are allowed; these catch a guard change that would lock capture out.

# GET - getExact entries, see arkime_db_health_check, arkime_db_load_fields,
# and the template _meta check in capture/db.c
$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/_cat/health?format=json");
is ($response->code, 200, "GET _cat/health with format query allowed");

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tests_fields/_search?size=3000");
is ($response->code, 200, "GET fields _search with size query allowed");

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/_template/tests_sessions3_template?filter_path=**._meta");
is ($response->code, 200, "GET sessions3 template with filter_path query allowed");

# A ; isn't a param separator to the guard, so it must not become one to es either
$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tests_fields/_search?size=1;q=_id:esproxy-nosuchid");
# esProxy passes es errors through with a 200, so check es rejected it rather than searched,
# the error text differs by backend
unlike ($response->content, qr/"hits"/, "GET fields _search with ; smuggled q not searched");

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

# A sensor with host set can only put that hostname in its stats doc, viewers
# use it to decide which nodes are on their host
foreach my $body ('{"hostname":"some-other-host","ver":"test"}', '{"ver":"test"}', '[]') {
    $req = HTTP::Request->new('POST', "http://esproxyhost:esproxyhost\@$ArkimeTest::host:7200/tests_stats/_doc/esproxyhost");
    $req->header('Content-Type' => 'application/json');
    $req->content($body);
    $response = $ArkimeTest::userAgent->request($req);
    is ($response->code, 400, "POST stats doc with wrong hostname rejected $body");
}

$req = HTTP::Request->new('POST', "http://esproxyhost:esproxyhost\@$ArkimeTest::host:7200/tests_stats/_doc/esproxyhost");
$req->header('Content-Type' => 'application/json');
$req->content('{"hostname":"esproxy-test-host","ver":"test"}');
$response = $ArkimeTest::userAgent->request($req);
ok ($response->code == 200 || $response->code == 201, "POST stats doc with pinned hostname allowed") or diag($response->code);

# capture gzips the stats doc when compressES is set
my $statsGz;
IO::Compress::Gzip::gzip(\'{"hostname":"some-other-host","ver":"test"}' => \$statsGz);
$req->header('Content-Encoding' => 'gzip');
$req->content($statsGz);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "POST gzip stats doc with wrong hostname rejected");

IO::Compress::Gzip::gzip(\'{"hostname":"esproxy-test-host","ver":"test"}' => \$statsGz);
$req->content($statsGz);
$response = $ArkimeTest::userAgent->request($req);
ok ($response->code == 200 || $response->code == 201, "POST gzip stats doc with pinned hostname allowed") or diag($response->code);

esDelete("/tests_stats/_doc/esproxyhost?refresh=true");

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

# query params that replace or widen the validated request are rejected
$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tests_users/_count?q=userId:admin");
is ($response->code, 400, "GET users _count with q query rejected");

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tests_users/_count?%71=userId:admin");
is ($response->code, 400, "GET users _count with encoded q query rejected");

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/tagger/_search?source=%7B%7D&source_content_type=application/json");
is ($response->code, 400, "GET tagger _search with source query rejected");

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/_cluster/health?wait_for_status=green");
is ($response->code, 400, "GET cluster health with wait_for query rejected");

$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_sessions3-2024/_search?q=*&size=10000");
$req->header('Content-Type' => 'application/json');
$req->content($search_ids);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "POST sessions _search with valid body and q query rejected");

# Files _update - capture updates its own file docs as they close
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_files/_update/test-99999");
$req->header('Content-Type' => 'application/json');
$req->content(qq({"doc": {"filesize": 1234, "packets": 5}}));
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 200, "POST own files update allowed") or diag($response->content);

my $updateGz;
IO::Compress::Gzip::gzip(\'{"doc": {"filesize": 1235}}' => \$updateGz);
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_files/_update/test-99999");
$req->header('Content-Type' => 'application/json');
$req->header('Content-Encoding' => 'gzip');
$req->content($updateGz);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 200, "POST gzip own files update allowed") or diag($response->content);

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

# The files doc body must describe the sensor's own file in the path, another
# node's viewer would otherwise expire (unlink) whatever name it points at
foreach my $body (qq({"node":"test2","num":99998,"name":"/tmp/esProxy-other.pcap","first":0,"locked":0}),
                  qq({"num":99998,"name":"/tmp/esProxy-other.pcap","first":0,"locked":0}),
                  qq({"node":"test","num":1,"name":"/tmp/esProxy-other.pcap","first":0,"locked":0}),
                  qq({"node":"test","num":"99998","name":"/tmp/esProxy-other.pcap","first":0,"locked":0})) {
    $req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_files/_doc/test-99998?refresh=true");
    $req->header('Content-Type' => 'application/json');
    $req->content($body);
    $response = $ArkimeTest::userAgent->request($req);
    is ($response->code, 400, "POST files doc not matching node/num rejected $body");
}

# capture gzips larger bodies when compressES is set
my $gz;
IO::Compress::Gzip::gzip(\qq({"node":"test","num":99998,"name":"/tmp/esProxy-gzip.pcap","first":0,"locked":0}) => \$gz);
$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_files/_doc/test-99998?refresh=true");
$req->header('Content-Type' => 'application/json');
$req->header('Content-Encoding' => 'gzip');
$req->content($gz);
$response = $ArkimeTest::userAgent->request($req);
ok ($response->code == 200 || $response->code == 201, "POST gzip files doc allowed") or diag($response->code);

IO::Compress::Gzip::gzip(\qq({"node":"test2","num":99998,"name":"/tmp/esProxy-gzip.pcap","first":0,"locked":0}) => \$gz);
$req->content($gz);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "POST gzip files doc for other node rejected");

$response = $ArkimeTest::userAgent->request(HTTP::Request::Common::DELETE("http://test:test\@$ArkimeTest::host:7200/tests_files/_doc/test-99998"));
is ($response->code, 200, "DELETE gzip files doc allowed");

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

$response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/partial-tests_sessions3-2024/_doc/nonexistent-id");
isnt ($response->content, "Not authorized for API", "GET session doc in partial index is proxied to ES, not blocked by guard");

# Session doc get/update must be one sessions index, not a multi-index expression
foreach my $index ("tests_sessions3-2024,tests_users", "tests_sessions3-*", "tests_sessions3-2024:tests_users", "tests_sessions3-2024%2Ftests_users") {
    $response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/$index/_doc/1");
    is ($response->code, 400, "GET session doc in $index rejected");
}

$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_sessions3-2024,tests_users/_update/1");
$req->header('Content-Type' => 'application/json');
$req->content($evil_update);
$response = $ArkimeTest::userAgent->request($req);
is ($response->code, 400, "POST session update in multi-index expression rejected");

# The sensor viewer's es client encodes the commas in index lists, these are its startup and not found retry calls
foreach my $test (
    ["_nodes/stats/jvm%2Cprocess%2Cfs%2Cos%2Cindices%2Cthread_pool", 200, "GET nodes stats with encoded commas allowed"],
    ["tests_sessions2-*%2Cpartial-tests_sessions3-*%2Ctests_sessions3-*/_alias", 200, "GET viewer sessions alias list allowed"],
    ["tests_sessions3-2024/_refresh", 200, "GET sessions index refresh allowed"],
    ["tests_sessions2-2024%2Ctests_sessions3-2024/_refresh", 200, "GET sessions2 and sessions3 index refresh allowed"],
    ["tests_*/_refresh", 200, "GET capture exit refresh allowed"],
    ["tests_queries/_doc/primary-viewer", 200, "GET primary viewer doc allowed"],
    ["tests_queries/_doc/other", 400, "GET other queries doc rejected"],
    ["tests_users/_refresh", 400, "GET users index refresh rejected"],
    ["tests_sessions3-*/_refresh", 400, "GET sessions wildcard refresh rejected"],
    ["tests_sessions3-2024%2Ctests_users/_refresh", 400, "GET sessions and users index refresh rejected"],
) {
    my ($path, $code, $name) = @$test;
    $response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/$path");
    is ($response->code, $code, $name);
}
