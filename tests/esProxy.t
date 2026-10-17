# ESProxy
use Test::More tests => 368;
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
is ($response->code, 200, "bulk field delete answered");

# A field delete is dropped, otherwise delete then create would redefine the field
$response = bulkPost("test", qq({"delete":{"_index":"tests_fields","_id":"esproxy.newfield"}}\n{"index":{"_index":"tests_fields","_id":"esproxy.newfield"}}\n{"friendlyName":"x","group":"general","help":"x","type":"termfield","dbField2":"esproxyredefined"}\n));
is ($response->code, 200, "bulk field delete then index answered");
like ($response->content, qr/"errors":\s*false/, "bulk field delete then index of the same id is dropped without an error");
esGet("/_refresh");
is (esGet("/tests_fields/_doc/esproxy.newfield")->{_source}->{dbField2}, "esproxynewfield", "bulk field delete then index does not redefine the field");
esDelete("/tests_fields/_doc/esproxy.newfield?refresh=true");

# Several field deletes in one bulk, as capture sends on start, are all dropped
esPut("/tests_fields/_doc/esproxy.del1?refresh=true", '{"friendlyName":"x","group":"general","help":"x","type":"termfield","dbField2":"esproxydel1"}');
esPut("/tests_fields/_doc/esproxy.del2?refresh=true", '{"friendlyName":"x","group":"general","help":"x","type":"termfield","dbField2":"esproxydel2"}');
$response = bulkPost("test", qq({"delete":{"_index":"tests_fields","_id":"esproxy.del1"}}\n{"delete":{"_index":"tests_fields","_id":"esproxy.del2"}}\n));
is ($response->code, 200, "bulk with several field deletes answered");
esGet("/_refresh");
ok (esGet("/tests_fields/_doc/esproxy.del1")->{found}, "first dropped field delete left the field");
ok (esGet("/tests_fields/_doc/esproxy.del2")->{found}, "second dropped field delete left the field");
esDelete("/tests_fields/_doc/esproxy.del1?refresh=true");
esDelete("/tests_fields/_doc/esproxy.del2?refresh=true");

# A new field has to be a definition viewers can use, dropped before it is stored
foreach my $test (
    ['esproxy.badfield', '{"friendlyName":"x","group":"general","help":"x","type":"termfield","dbField2":["a"]}', "array dbField2"],
    ['esproxy.badfield', '{"friendlyName":"x","group":"general","help":"x","type":"termfield","dbField2":"esproxybad","transform":"__proto__"}', "proto transform"],
    ['esproxy.badfield', '{"friendlyName":"x","group":"general","help":"x","type":"termfield","dbField2":"esproxybad","regex":"("}', "bad regex"],
    ['esproxy.badfield', '{"friendlyName":"x","group":"general","help":"x","type":"termfield","dbField2":"esproxybad","aliases":[1]}', "non string alias"],
    ['esproxy.badfield', '{"friendlyName":"x","group":"general","help":"x","type":"termfield","dbField2":"esproxybad","group":{"a":1}}', "object group"],
    ['esproxy.badfield', '[]', "array body"],
    ['__proto__', '{"friendlyName":"x","group":"general","help":"x","type":"termfield","dbField2":"esproxybad"}', "proto id"],
    ['esproxy.badfield', '{"friendlyName":"x","group":"general","help":"x","type":"termfield","dbField2":"esproxybad","portField2":5}', "number portField2"],
) {
    $response = bulkPost("test", qq({"index":{"_index":"tests_fields","_id":"$test->[0]"}}\n$test->[1]\n));
    is ($response->code, 200, "bulk field index with $test->[2] answered");
    like ($response->content, qr/"items":\[\]/, "bulk field index with $test->[2] not applied");
}
esGet("/_refresh");
ok (!esGet("/tests_fields/_doc/esproxy.badfield")->{found}, "bad field not created");

# A bad field doesn't lose the rest of capture's batch of new fields
$response = bulkPost("test", qq|{"index":{"_index":"tests_fields","_id":"esproxy.badfield"}}\n{"friendlyName":"x","group":"general","help":"x","type":"termfield","dbField2":"esproxybad","regex":"("}\n{"index":{"_index":"tests_fields","_id":"esproxy.goodfield"}}\n{"friendlyName":"x","group":"general","help":"x","type":"termfield","dbField2":"esproxygood"}\n|);
is ($response->code, 200, "bulk with a bad and a good field answered");
esGet("/_refresh");
ok (!esGet("/tests_fields/_doc/esproxy.badfield")->{found}, "bad field in mixed bulk not created");
ok (esGet("/tests_fields/_doc/esproxy.goodfield")->{found}, "good field in mixed bulk created");
esDelete("/tests_fields/_doc/esproxy.goodfield?refresh=true");

# A new field can't take over the db name of an existing field, viewers look fields up by it
foreach my $test (
    ['"dbField2":"srcIp"', "dbField2 of ip.src"],
    ['"dbField2":"source.ip"', "fieldECS of ip.src as dbField2"],
    ['"dbField2":"esproxycollide","fieldECS":"source.ip"', "fieldECS of ip.src"],
    ['"dbField2":"esproxycollide","dbField":"srcIp"', "dbField2 of ip.src as dbField"],
    ['"dbField2":"esproxycollide","aliases":["esproxy.alias"],"dbField":"node"', "dbField2 of node as dbField"],
) {
    $response = bulkPost("test", qq({"index":{"_index":"tests_fields","_id":"esproxy.collide"}}\n{"friendlyName":"x","group":"general","help":"x","type":"termfield",$test->[0]}\n));
    is ($response->code, 200, "bulk field index with $test->[1] answered");
    like ($response->content, qr/"items":\[\]/, "bulk field index with $test->[1] not applied");
}
esGet("/_refresh");
ok (!esGet("/tests_fields/_doc/esproxy.collide")->{found}, "colliding field not created");

# Unrelated db names in the same bulk still go through
$response = bulkPost("test", qq({"index":{"_index":"tests_fields","_id":"esproxy.collide"}}\n{"friendlyName":"x","group":"general","help":"x","type":"termfield","dbField2":"srcIp"}\n{"index":{"_index":"tests_fields","_id":"esproxy.nocollide"}}\n{"friendlyName":"x","group":"general","help":"x","type":"termfield","dbField2":"esproxynocollide"}\n));
is ($response->code, 200, "bulk with a colliding and a new field answered");
esGet("/_refresh");
ok (!esGet("/tests_fields/_doc/esproxy.collide")->{found}, "colliding field in mixed bulk not created");
ok (esGet("/tests_fields/_doc/esproxy.nocollide")->{found}, "new field in mixed bulk created");
esDelete("/tests_fields/_doc/esproxy.nocollide?refresh=true");

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

# Bulk - a command whose body line is missing or not JSON is rejected
foreach my $test (
    [qq({"update":{"_index":"tests_fields","_id":"node"}}\n), "update without body"],
    [qq({"update":{"_index":"tests_fields","_id":"node"}}\nnot json\n), "update with invalid JSON body"],
    [qq({"index":{"_index":"tests_fields","_id":"esproxy.nobody"}}\n), "fields index without body"],
    [qq({"index":{"_index":"tests_fields","_id":"esproxy.nobody"}}\nnot json\n), "fields index with invalid JSON body"],
) {
    $req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/_bulk");
    $req->header('Content-Type' => 'application/x-ndjson');
    $req->content($test->[0]);
    $response = $ArkimeTest::userAgent->request($req);
    is ($response->code, 400, "bulk $test->[1] rejected");
}

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

# Stats, fields and lookups searches can't run scripts or read other indices
sub exactSearch {
    my ($index, $body) = @_;
    my $r = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_$index/_search");
    $r->header('Content-Type' => 'application/json');
    $r->content($body) if (defined $body);
    return $ArkimeTest::userAgent->request($r)->code;
}
foreach my $test (
    ["stats", '{"query":{"match":{"hostname":"x"}}}', 200, "viewer hostname"],
    ["fields", '{"size":10000}', 200, "viewer fields"],
    ["lookups", '{"query":{"bool":{"filter":[{"bool":{"should":[{"terms":{"roles":["arkimeUser"]}},{"term":{"users":"u"}},{"term":{"userId":"u"}}]}}]}},"size":10000}', 200, "viewer shortcuts"],
    ["stats", undef, 200, "no body"],
    ["stats", '{"script_fields":{"x":{"script":{"source":"1"}}}}', 400, "script_fields"],
    ["stats", '{"sort":{"_script":{"type":"number","script":"1"}}}', 400, "script sort"],
    ["fields", '{"runtime_mappings":{"x":{"type":"keyword"}}}', 400, "runtime_mappings"],
    ["lookups", '{"aggs":{"a":{"scripted_metric":{"map_script":"1"}}}}', 400, "scripted_metric"],
    ["stats", '{"query":{"terms":{"hostname":{"index":"tests_users","id":"admin","path":"userId"}}}}', 400, "terms lookup"],
    ["stats", '{"query":{"more_like_this":{"like":[{"_index":"tests_users","_id":"admin"}]}}}', 400, "more_like_this"],
    ["stats", '{"query":{"wrapper":{"query":"eyJzY3JpcHRfc2NvcmUiOnsicXVlcnkiOnsibWF0Y2hfYWxsIjp7fX0sInNjcmlwdCI6eyJzb3VyY2UiOiIxIn19fQ=="}}}', 400, "wrapper hiding script_score"],
    ["stats", '{"suggest":{"s":{"text":"x","phrase":{"field":"hostname","collate":{"query":{"source":"{\\"script\\":{\\"script\\":\\"1\\"}}"}}}}}}', 400, "suggest collate hiding a script"],
    ["stats", '{"query":{"term":{"index":"x"}},"sort":{"index":"asc"},"_source":["index","_index"]}', 200, "a field named index"],
    ["stats", '{"query":{"pinned":{"docs":[{"_index":"tests_users","_id":"admin"}],"organic":{"match_all":{}}}}}', 400, "pinned docs from another index"],
    ["stats", '{"query":', 400, "bad json"],
    ["lookups", '{"query":{"match_all":{}},"sort":{"description":{"order":"asc"}},"size":50}', 200, "shortcuts sorted by description"],
    ["stats", '{"query":{"term":{"subscription":"x"}},"_source":["description","transcript"]}', 200, "field names containing script"],
    ["stats", '{"query":{"script":{"script":"1"}}}', 400, "script query"],
    ["stats", '{"query":{"bool":{"filter":[{"bool":{"should":[{"function_score":{"script_score":{"script":"1"}}}]}}]}}}', 400, "nested script_score"],
    ["stats", '{"query":{"terms_set":{"x":{"terms":["a"],"minimum_should_match_script":{"source":"1"}}}}}', 400, "minimum_should_match_script"],
    ["stats", '{"aggs":{"a":{"moving_fn":{"buckets_path":"x","window":2}}}}', 400, "moving_fn"],
    ["stats", '{"aggs":{"a":{"terms":{"field":"x"},"aggs":{"b":{"bucket_selector":{"buckets_path":{}}}}}}}', 400, "bucket_selector"],
    ["stats", '{"query":{"percolate":{"field":"q","index":"tests_users","id":"admin"}}}', 400, "percolate"],
    ["stats", '{"query":{"geo_shape":{"x":{"indexed_shape":{"index":"tests_users","id":"admin","path":"p"}}}}}', 400, "geo_shape indexed_shape"],
) {
    is (exactSearch($test->[0], $test->[1]), $test->[2], "POST $test->[0] _search with $test->[3]");
}

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

$req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tagger%3f/../../tests_users/_search");
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
    $req->content('{"userId":"esproxyhistory","timestamp":1,"api":"/esproxy","method":"GET"}');
    $response = $ArkimeTest::userAgent->request($req);
    if ($allowed) {
        ok ($response->code == 200 || $response->code == 201, "history write to $index allowed") or diag($response->code);
        my $docId = eval { from_json($response->content)->{_id} };
        esDelete("/$index/_doc/$docId?refresh=true") if $docId;
    } else {
        is ($response->code, 400, "history write to $index rejected");
    }
}

# and only with what the viewer's history logging always sends, so a sensor can't
# write just any audit row
sub historyPost {
    my ($body, $gzip) = @_;
    my $r = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_history_v1-26w41/_doc");
    $r->header('Content-Type' => 'application/json');
    if ($gzip) {
        my $gz;
        IO::Compress::Gzip::gzip(\$body => \$gz);
        $r->header('Content-Encoding' => 'gzip');
        $r->content($gz);
    } else {
        $r->content($body);
    }
    return $ArkimeTest::userAgent->request($r);
}
foreach my $test (
    ['{"timestamp":1,"api":"/esproxy","method":"GET"}', "no userId"],
    ['{"userId":"","timestamp":1,"api":"/esproxy","method":"GET"}', "empty userId"],
    ['{"userId":["esproxyhistory"],"timestamp":1,"api":"/esproxy","method":"GET"}', "userId not a string"],
    ['{"userId":"esproxyhistory","timestamp":"1","api":"/esproxy","method":"GET"}', "timestamp not a number"],
    ['{"userId":"esproxyhistory","timestamp":1,"method":"GET"}', "no api"],
    ['{"userId":"esproxyhistory","timestamp":1,"api":"/esproxy"}', "no method"],
    ['[]', "array"],
    ['"x"', "string"],
) {
    is (historyPost($test->[0])->code, 400, "history write with $test->[1] rejected");
}
$response = historyPost('{"userId":"esproxyhistory","timestamp":1,"api":"/esproxy","method":"GET"}', 1);
ok ($response->code == 200 || $response->code == 201, "gzip history write allowed") or diag($response->code);
my $historyId = eval { from_json($response->content)->{_id} };
esDelete("/tests_history_v1-26w41/_doc/$historyId?refresh=true") if $historyId;

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

# Without host set the first stats hostname sticks, so a sensor can't later
# claim a viewer's host
sub nowiseStats {
    my ($body) = @_;
    my $r = HTTP::Request->new('POST', "http://nowise:nowise\@$ArkimeTest::host:7200/tests_stats/_doc/nowise");
    $r->header('Content-Type' => 'application/json');
    $r->content($body);
    return $ArkimeTest::userAgent->request($r)->code;
}
like (nowiseStats('{"hostname":"esproxy-nowise-host","ver":"test"}'), qr/^20[01]$/, "POST stats doc without host pins first hostname");
is (nowiseStats('{"hostname":"some-other-host","ver":"test"}'), 400, "POST stats doc without host with new hostname rejected");
is (nowiseStats('{"ver":"test"}'), 400, "POST stats doc without host missing hostname rejected");
like (nowiseStats('{"hostname":"esproxy-nowise-host","ver":"test"}'), qr/^20[01]$/, "POST stats doc without host with pinned hostname allowed");
is (filesSearch("nowise", '{"size":1,"query":{"bool":{"filter":[{"terms":{"node":["nowise","esproxyhost"]}}]}}}'), 400, "files search with pinned host for a node on another host rejected");
is (filesSearch("nowise", '{"size":1,"query":{"bool":{"filter":[{"terms":{"node":["nowise"]}}]}}}'), 200, "files search with pinned host for own node allowed");
esDelete("/tests_stats/_doc/nowise?refresh=true");

# With several esProxy, a stats doc written through another one is found in es
# before a new hostname is trusted
sub sensorPost {
    my ($sensor, $path, $body) = @_;
    my $r = HTTP::Request->new('POST', "http://$sensor:$sensor\@$ArkimeTest::host:7200/$path");
    $r->header('Content-Type' => 'application/json');
    $r->content($body);
    return $ArkimeTest::userAgent->request($r)->code;
}
esPut("/tests_stats/_doc/esproxypin1?refresh=true", '{"hostname":"esproxy-pin-host","ver":"test"}');
is (sensorPost("esproxypin1", "tests_stats/_doc/esproxypin1", '{"hostname":"central-viewer-host","ver":"test"}'), 400, "POST stats doc with hostname differing from one another esProxy pinned rejected");
like (sensorPost("esproxypin1", "tests_stats/_doc/esproxypin1", '{"hostname":"esproxy-pin-host","ver":"test"}'), qr/^20[01]$/, "POST stats doc with hostname another esProxy pinned allowed");

# Files search, another node on the same host is found the same way
my $pinSearch = '{"size":1,"query":{"bool":{"filter":[{"terms":{"node":["esproxypin1","esproxypin2"]}}]}}}';
esPut("/tests_stats/_doc/esproxypin2?refresh=true", '{"hostname":"esproxy-pin-host","ver":"test"}');
is (filesSearch("esproxypin1", $pinSearch), 200, "files search with a same host node pinned by another esProxy allowed");

# A stats doc without a hostname isn't a missing pin, nothing is trusted from it
esPut("/tests_stats/_doc/esproxypin3?refresh=true", '{"ver":"test"}');
is (sensorPost("esproxypin3", "tests_stats/_doc/esproxypin3", '{"hostname":"esproxy-other-host","ver":"test"}'), 400, "POST stats doc when the stored stats doc has no hostname rejected");
is (filesSearch("esproxypin3", '{"size":1,"query":{"bool":{"filter":[{"terms":{"node":["esproxypin3"]}}]}}}'), 400, "files search when the stored stats doc has no hostname rejected");

esPut("/tests_stats/_doc/esproxypin3?refresh=true", '{"hostname":"esproxy-other-host","ver":"test"}');
is (filesSearch("esproxypin1", '{"size":1,"query":{"bool":{"filter":[{"terms":{"node":["esproxypin1","esproxypin3"]}}]}}}'), 400, "files search with a node on another host pinned by another esProxy rejected");
esDelete("/tests_stats/_doc/esproxypin3?refresh=true");

# A sensor with no stats doc anywhere isn't on any host yet, so only its own node
is (filesSearch("esproxypin4", '{"size":1,"query":{"bool":{"filter":[{"terms":{"node":["esproxypin4","test"]}}]}}}'), 400, "files search for another node before a first stats doc rejected");
is (filesSearch("esproxypin4", '{"size":1,"query":{"bool":{"filter":[{"terms":{"node":["esproxypin4"]}}]}}}'), 200, "files search for own node before a first stats doc allowed");
is (filesSearch("esproxypin1", '{"size":1,"query":{"bool":{"filter":[{"terms":{"node":["esproxypin1","esproxypin4"]}}]}}}'), 400, "files search with a node that has no stats doc rejected");

# A stats doc without a hostname is refused before any lookup
is (sensorPost("esproxypin4", "tests_stats/_doc/esproxypin4", '{"ver":"test"}'), 400, "POST stats doc of a new node without hostname rejected");

# A new node writes stats every 2 seconds, the first pins and the rest must match
like (sensorPost("esproxypin4", "tests_stats/_doc/esproxypin4", '{"hostname":"esproxy-new-host","ver":"test"}'), qr/^20[01]$/, "POST first stats doc of a new node pins it");
like (sensorPost("esproxypin4", "tests_stats/_doc/esproxypin4", '{"hostname":"esproxy-new-host","ver":"test"}'), qr/^20[01]$/, "POST next stats doc of a new node allowed");
is (sensorPost("esproxypin4", "tests_stats/_doc/esproxypin4", '{"hostname":"esproxy-pin-host","ver":"test"}'), 400, "POST stats doc of a new node with a new hostname rejected");
esDelete("/tests_stats/_doc/esproxypin1?refresh=true");
esDelete("/tests_stats/_doc/esproxypin2?refresh=true");
esDelete("/tests_stats/_doc/esproxypin4?refresh=true");

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

# POST users _count is the sensor viewer's "no users" check, esProxy drops the body
sub usersCount {
    my ($body, $query) = @_;
    my $r = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_users/_count" . ($query // ''));
    $r->header('Content-Type' => 'application/json');
    $r->content($body);
    return $ArkimeTest::userAgent->request($r);
}
my $usersTotal = esGet("/tests_users/_count")->{count};
$response = usersCount('{}', '?ignore_unavailable=true');
is ($response->code, 200, "POST users _count from the viewer allowed");
is (from_json($response->content)->{count}, $usersTotal, "POST users _count matches es");
$response = usersCount('{"query":{"prefix":{"passStore":"SECRET"}}}');
is (from_json($response->content)->{count}, $usersTotal, "POST users _count ignores the sent query");
$response = usersCount('{}', '?q=passStore:SECRET*');
is ($response->code, 400, "POST users _count with q query rejected");

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

# Viewers trust name, indexFilename and locked when reading and expiring the file
foreach my $field ('"name": "/opt/arkime/raw/../../../tmp/esProxy-x"', '"indexFilename": "/tmp/esProxy-x"', '"locked": 0') {
  $req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_files/_update/test-99999");
  $req->header('Content-Type' => 'application/json');
  $req->content(qq({"doc": {"filesize": 1234, $field}}));
  $response = $ArkimeTest::userAgent->request($req);
  is ($response->code, 400, "POST files update changing $field rejected");
  is ($response->content, "Not authorized for API");
}

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
$req->content(qq({"nodeName":"test","interval":60}));
$response = $ArkimeTest::userAgent->request($req);
ok ($response->code == 200 || $response->code == 201, "POST own dstats doc allowed") or diag($response->code);

# Viewers key dstats by nodeName, so it has to be the sensor's own
foreach my $body ('{"nodeName":"test2","interval":60}', '{"nodeName":"__proto__","interval":60}', '{"interval":60}') {
    $req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_dstats/_doc/test-59-60");
    $req->header('Content-Type' => 'application/json');
    $req->content($body);
    is ($ArkimeTest::userAgent->request($req)->code, 400, "POST own dstats doc with nodeName $body rejected");
}

# On ES 7 typed routes these ids are endpoints, _search would list every hunt or user
foreach my $test (
    ["tests_hunts/_doc/_search", 400], ["tests_hunts/_doc/%5Fsearch", 400], ["tests_hunts/_doc/_count", 400],
    ["tests_hunts/_doc/x/_source", 400],
    ["tests_users/_doc/_search", 400], ["tests_users/_doc/%5Fsearch", 400],
    ["tests_users/_doc/_mget", 400], ["tests_users/_doc/_msearch", 400], ["tests_users/_doc/_termvectors", 400],
    ["tests_users/_doc/_mapping", 400], ["tests_users/_doc/%5fcount", 400], ["tests_hunts/_doc/_mget", 400],
    ["tests_hunts/_doc/_validate", 400], ["tests_hunts/_doc/_rollup_search", 400],
    # es matches routes on the raw path, an encoded / is refused anyway in case one ever doesn't
    ["tests_users/_doc/admin%2F_source", 400], ["tests_hunts/_doc/_mapping%2Ffield%2F%2A", 400],
) {
    is ($ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/$test->[0]")->code, $test->[1], "GET $test->[0] rejected");
}
foreach my $path ("tests_hunts/_doc/AbC_-def0123456789xy", "tests_hunts/_doc/_abcdefghijklmnopqrs", "tests_users/_doc/_shared_user",
                  "tests_users/_doc/_viewer", "tests_users/_doc/role%3Aadmin", "tests_users/_doc/a%40b.com") {
    isnt ($ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/$path")->content, "Not authorized for API", "GET $path proxied");
}

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

# No viewer reads a session doc by GET, Db.getSession searches by id, so the path is
# closed with the right prefix too. On ES 7 typed routes _search would dump every session.
foreach my $path ("tests_sessions3-2024/_doc/nonexistent-id", "partial-tests_sessions3-2024/_doc/nonexistent-id",
                  "tests_sessions3-2024/_doc/_search?size=10000", "tests_sessions3-2024/_doc/260101-node%3A1-5-24") {
    $response = $ArkimeTest::userAgent->get("http://test:test\@$ArkimeTest::host:7200/$path");
    is ($response->code, 400, "GET session doc $path rejected");
}

# Session update only runs the hunt script a sensor viewer sends when a remote hunt matches
sub sessionUpdate {
    my ($body) = @_;
    my $r = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_sessions3-2024/_update/nonexistent-id");
    $r->header('Content-Type' => 'application/json');
    $r->content($body);
    return $ArkimeTest::userAgent->request($r);
}
my $addTags = 'if (ctx._source.tags != null) { for (int i = 0; i < params.tags.length; i++) { if (ctx._source.tags.indexOf(params.tags[i]) == -1) { ctx._source.tags.add(params.tags[i]); } } ctx._source.tagsCnt = ctx._source.tags.length; } else { ctx._source.tags = params.tags; ctx._source.tagsCnt = params.tags.length; }';
my $addHunt = 'if (ctx._source.huntId != null) { ctx._source.huntId.add(params.huntId); } else { ctx._source.huntId = [ params.huntId ]; } if (ctx._source.huntName != null) { ctx._source.huntName.add(params.huntName); } else { ctx._source.huntName = [ params.huntName ]; }';
isnt (sessionUpdate(to_json({script => {source => "\n   $addHunt\n", lang => "painless", params => {huntId => "h1", huntName => "hunt"}}}))->content, "Not authorized for API", "POST session update with reindented viewer add hunt script proxied");
sub sessionUpdateId {
    my ($id) = @_;
    my $r = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/tests_sessions3-2024/_update/$id");
    $r->header('Content-Type' => 'application/json');
    $r->content(to_json({script => {source => $addHunt, lang => "painless", params => {huntId => "h1", huntName => "hunt"}}}));
    return $ArkimeTest::userAgent->request($r);
}
isnt (sessionUpdateId("260101-node%3A1-5-24")->content, "Not authorized for API", "POST session update with encoded id proxied");
is (sessionUpdateId("_search")->code, 400, "POST session update with id _search rejected");
is (sessionUpdateId("%5Fsearch")->code, 400, "POST session update with id %5Fsearch rejected");
foreach my $test (
    [{script => {source => "ctx._source.marker='evil'"}}, "other script"],
    [{script => {source => "ctx.op='delete'"}}, "delete script"],
    [{script => {source => $addTags, lang => "painless", params => {tags => ["a"]}}}, "central only add tags script"],
    [{script => {source => $addHunt, params => {huntId => "h1", huntName => "hunt", x => 1}}}, "extra param"],
    [{script => {source => $addHunt, params => {huntId => ["h1"], huntName => "hunt"}}}, "huntId not a string"],
    [{script => {source => $addHunt, lang => "expression", params => {huntId => "h1", huntName => "hunt"}}}, "other lang"],
    [{script => {source => $addHunt, params => {huntId => "h1", huntName => "hunt"}}, scripted_upsert => JSON::true, upsert => {}}, "upsert"],
    [{script => {id => "stored", params => {tags => ["a"]}}}, "stored script"],
) {
    is (sessionUpdate(to_json($test->[0]))->code, 400, "POST session update with $test->[1] rejected");
}

# A sensor viewer only handles sessions from nodes on its host: searches are narrowed to
# those nodes and the hunt script leaves other hosts' sessions alone. test's stats doc
# puts it on this host, esproxyhost is configured on another one.
my %scopeDocs = (
    'esproxy-scope-own' => '{"node":"test","rootId":"esproxy-scope-root","lastPacket":1}',
    'esproxy-scope-other' => '{"node":"esproxyhost","rootId":"esproxy-scope-root","lastPacket":2}',
    'esproxy-scope-nosensor' => '{"node":"esproxy-nosuchnode","rootId":"esproxy-scope-root","lastPacket":3}',
    'esproxy-scope-nostats' => '{"node":"esproxypin5","rootId":"esproxy-scope-root","lastPacket":4}'
);
esPut("/tests_sessions3-2024/_doc/$_?refresh=true", $scopeDocs{$_}) foreach (keys %scopeDocs);

sub scopeSearch {
    my ($user, $body, $gzip) = @_;
    my $r = HTTP::Request->new('POST', "http://$user:$user\@$ArkimeTest::host:7200/tests_sessions3-2024/_search?rest_total_hits_as_int=true");
    $r->header('Content-Type' => 'application/json');
    if ($gzip) {
        my $gz;
        IO::Compress::Gzip::gzip(\$body => \$gz);
        $r->header('Content-Encoding' => 'gzip');
        $r->content($gz);
    } else {
        $r->content($body);
    }
    my $resp = $ArkimeTest::userAgent->request($r);
    return "code " . $resp->code if ($resp->code != 200);
    return join(",", sort map { $_->{_id} } @{from_json($resp->content)->{hits}->{hits}});
}
my $idsOwn = '{"query":{"ids":{"values":["esproxy-scope-own"]}},"_source":["node"]}';
my $idsOther = '{"query":{"ids":{"values":["esproxy-scope-other"]}},"_source":["node"]}';
my $rootTerm = '{"size":100,"_source":["rootId"],"query":{"term":{"rootId":"esproxy-scope-root"}}}';
my $rootFilter = '{"size":100,"_source":["rootId"],"sort":{"lastPacket":{"order":"asc"}},"query":{"bool":{"filter":[{"term":{"rootId":"esproxy-scope-root"}}]}}}';
my $rootForced = '{"size":100,"_source":["rootId"],"query":{"bool":{"filter":[{"term":{"rootId":"esproxy-scope-root"}},{"range":{"lastPacket":{"gte":0}}}]}}}';
my $rootLookup = '{"size":100,"_source":["rootId"],"query":{"bool":{"filter":[{"term":{"rootId":"esproxy-scope-root"}},{"terms":{"node":{"index":"tests_users","id":"anonymous","path":"userId"}}}]}}}';
my $rootScriptSort = '{"size":100,"_source":["rootId"],"sort":{"_script":{"type":"number","script":{"source":"1"}}},"query":{"bool":{"filter":[{"term":{"rootId":"esproxy-scope-root"}}]}}}';
is (scopeSearch("test", $idsOwn), "esproxy-scope-own", "sessions search by id finds a session on the sensor's host");
is (scopeSearch("test", $idsOther), "", "sessions search by id doesn't find a session on another host");
is (scopeSearch("test", $idsOwn, 1), "esproxy-scope-own", "gzip sessions search by id finds a session on the sensor's host");
is (scopeSearch("test", $rootTerm), "esproxy-scope-own", "sessions search by rootId term only finds the sensor's host");
is (scopeSearch("test", $rootFilter), "esproxy-scope-own", "sessions search by rootId filter only finds the sensor's host");
is (scopeSearch("test", $rootForced), "esproxy-scope-own", "sessions search by rootId with a forced expression clause only finds the sensor's host");
is (scopeSearch("test", $rootLookup), "code 400", "sessions search by rootId with a terms lookup clause rejected");
is (scopeSearch("test", $rootScriptSort), "code 400", "sessions search by rootId with a script sort rejected");
# A $shortcut in the forced expression is a terms lookup in the lookups index, which keeps working
esPut("/tests_lookups/_doc/esproxy-scope-shortcut?refresh=true", '{"name":"esproxyscope","string":["test"]}');
my $rootShortcut = '{"size":100,"_source":["rootId"],"query":{"bool":{"filter":[{"term":{"rootId":"esproxy-scope-root"}},{"terms":{"node":{"index":"tests_lookups","id":"esproxy-scope-shortcut","path":"string"}}}]}}}';
my $rootShortcutIndex = '{"size":100,"_source":["rootId"],"query":{"bool":{"filter":[{"term":{"rootId":"esproxy-scope-root"}},{"terms":{"node":{"_index":"tests_lookups","id":"esproxy-scope-shortcut","path":"string"}}}]}}}';
is (scopeSearch("test", $rootShortcut), "esproxy-scope-own", "sessions search by rootId with a shortcut clause finds the sensor's host");
is (scopeSearch("test", $rootShortcutIndex), "code 400", "sessions search by rootId with a _index lookup in lookups rejected");
my $rootShortcutScript = '{"size":100,"_source":["rootId"],"query":{"bool":{"filter":[{"term":{"rootId":"esproxy-scope-root"}},{"terms":{"node":{"index":"tests_lookups","id":"esproxy-scope-shortcut","path":"string","script":{"source":"1"}}}}]}}}';
is (scopeSearch("test", $rootShortcutScript), "code 400", "sessions search by rootId with a script next to a lookups lookup rejected");
esDelete("/tests_lookups/_doc/esproxy-scope-shortcut?refresh=true");
is (scopeSearch("esproxyhost", $idsOther), "esproxy-scope-other", "sessions search by id finds the other host's session for its own sensor");
is (scopeSearch("esproxyhost", $rootFilter), "esproxy-scope-other", "sessions search by rootId only finds the other host for its own sensor");
# Without a stats doc a sensor has no host, so only its own node. esproxypin5 never
# posts one, the other pin sensors were pinned in memory by their earlier tests.
my $idsNoStats = '{"query":{"ids":{"values":["esproxy-scope-nostats"]}},"_source":["node"]}';
is (scopeSearch("esproxypin5", $idsNoStats), "esproxy-scope-nostats", "sessions search by id finds the session of a sensor without a stats doc");
is (scopeSearch("esproxypin5", $idsOwn), "", "sessions search by id for a sensor without a stats doc doesn't find another node");

sub scopeUpdate {
    my ($user, $id, $gzip, $query) = @_;
    $query //= "";
    my $r = HTTP::Request->new('POST', "http://$user:$user\@$ArkimeTest::host:7200/tests_sessions3-2024/_update/$id?refresh=true$query");
    $r->header('Content-Type' => 'application/json');
    my $body = to_json({script => {source => $addHunt, lang => "painless", params => {huntId => "esproxy-scope-hunt", huntName => "scope"}}});
    if ($gzip) {
        my $gz;
        IO::Compress::Gzip::gzip(\$body => \$gz);
        $r->header('Content-Encoding' => 'gzip');
        $r->content($gz);
    } else {
        $r->content($body);
    }
    my $resp = $ArkimeTest::userAgent->request($r);
    return eval { from_json($resp->content)->{result} } // "code " . $resp->code;
}
is (scopeUpdate("test", "esproxy-scope-other"), "noop", "session hunt update leaves a session on another host alone");
is (scopeUpdate("test", "esproxy-scope-nosensor"), "noop", "session hunt update leaves a session of an unknown node alone");
is (scopeUpdate("test", "esproxy-scope-own"), "updated", "session hunt update changes a session on the sensor's host");
is (scopeUpdate("test", "esproxy-scope-other", 1), "noop", "gzip session hunt update leaves a session on another host alone");
is (scopeUpdate("esproxyhost", "esproxy-scope-other"), "updated", "session hunt update changes the other host's session for its own sensor");
is (scopeUpdate("esproxypin5", "esproxy-scope-own"), "noop", "session hunt update for a sensor without a stats doc leaves another node alone");
is (scopeUpdate("esproxypin5", "esproxy-scope-nostats"), "updated", "session hunt update for a sensor without a stats doc changes its own session");
# es returns the doc with _source even for a noop, which would read any session by id
is (scopeUpdate("test", "esproxy-scope-other", 0, "&_source=true"), "code 400", "session hunt update asking for _source rejected");
# A first stats doc is only accepted while nothing is pinned, so the tests above ran without a host
like (sensorPost("esproxypin5", "tests_stats/_doc/esproxypin5", '{"hostname":"esproxy-nostats-host","ver":"test"}'), qr/^20[01]$/, "sensor without a stats doc had no host during the scope tests");
esDelete("/tests_stats/_doc/esproxypin5?refresh=true");
is_deeply (esGet("/tests_sessions3-2024/_doc/esproxy-scope-own")->{_source}, {node => "test", rootId => "esproxy-scope-root", lastPacket => 1, huntId => ["esproxy-scope-hunt"], huntName => ["scope"]}, "own host session has the hunt");
is_deeply (esGet("/tests_sessions3-2024/_doc/esproxy-scope-other")->{_source}, {node => "esproxyhost", rootId => "esproxy-scope-root", lastPacket => 2, huntId => ["esproxy-scope-hunt"], huntName => ["scope"]}, "other host session only has its own sensor's hunt");
is_deeply (esGet("/tests_sessions3-2024/_doc/esproxy-scope-nosensor")->{_source}, {node => "esproxy-nosuchnode", rootId => "esproxy-scope-root", lastPacket => 3}, "unknown node session unchanged");
esDelete("/tests_sessions3-2024/_doc/$_?refresh=true") foreach (keys %scopeDocs);

# Session update must be one sessions index, not a multi-index expression, even with the allowed body
foreach my $index ("tests_sessions3-2024,tests_users", "tests_sessions3-*", "tests_sessions3-2024:tests_users", "tests_sessions3-2024%2Ftests_users") {
    $req = HTTP::Request->new('POST', "http://test:test\@$ArkimeTest::host:7200/$index/_update/1");
    $req->header('Content-Type' => 'application/json');
    $req->content(to_json({script => {source => $addHunt, lang => "painless", params => {huntId => "h1", huntName => "hunt"}}}));
    $response = $ArkimeTest::userAgent->request($req);
    is ($response->code, 400, "POST session update in $index rejected");
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
