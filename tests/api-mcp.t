use Test::More tests => 87;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $MCP = "http://$ArkimeTest::host:8123/mcp";

# Raw POST so we can assert on status codes and headers, which the shared
# viewerPost helper hides
sub mcpRaw {
my ($body, $user) = @_;
    # superAdmin expands to mcpUser, see systemRolesMapping
    $user = "superAdmin" if (!defined $user);
    return $ArkimeTest::userAgent->post("$MCP?arkimeRegressionUser=$user",
        Content => $body, "Content-Type" => "application/json");
}

sub mcp {
my ($body, $user) = @_;
    my $response = mcpRaw($body, $user);
    return from_json($response->content);
}

sub rpc {
my ($method, $params, $user) = @_;
    my $body = $params ? "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"$method\",\"params\":$params}"
                       : "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"$method\"}";
    return mcp($body, $user);
}

sub callTool {
my ($name, $args, $user) = @_;
    $args = "{}" if (!defined $args);
    return rpc("tools/call", "{\"name\":\"$name\",\"arguments\":$args}", $user);
}

################################################################################
# auth - the gateway contract: 401 + WWW-Authenticate, never a redirect
################################################################################
# sac- prefixed users are never auto created, so this is a genuinely unknown user
my $response = mcpRaw('{"jsonrpc":"2.0","id":1,"method":"ping"}', "sac-mcp-nobody");
is($response->code, 401, "unknown user gets 401");
is($response->header("WWW-Authenticate"), 'Bearer realm="arkime-viewer"', "401 carries WWW-Authenticate");
ok(!$response->is_redirect, "auth failure is not a redirect");

# wiseUser exists but has no arkimeUser role
viewerGet("/regressionTests/deleteAllUsers");
my $adminToken = getTokenCookie();
viewerPostToken("/api/user", '{"userId": "sac-mcp-norole", "userName": "sac-mcp-norole", "enabled":true, "password":"password", "roles":["wiseUser"]}', $adminToken);
$response = mcpRaw('{"jsonrpc":"2.0","id":1,"method":"ping"}', "sac-mcp-norole");
is($response->code, 403, "user without arkimeUser role gets 403");

# arkimeUser alone is not enough, MCP access is a separate explicit grant
viewerPostToken("/api/user", '{"userId": "sac-mcp-nomcp", "userName": "sac-mcp-nomcp", "enabled":true, "password":"password", "roles":["arkimeUser"]}', $adminToken);
$response = mcpRaw('{"jsonrpc":"2.0","id":1,"method":"ping"}', "sac-mcp-nomcp");
is($response->code, 403, "arkimeUser without mcpUser gets 403");
like($response->content, qr/mcpUser/, "403 says which role is missing");

# and adding mcpUser lets the same user in
viewerPostToken("/api/user/sac-mcp-nomcp", '{"userId": "sac-mcp-nomcp", "userName": "sac-mcp-nomcp", "enabled":true, "roles":["arkimeUser","mcpUser"]}', $adminToken);
$response = mcpRaw('{"jsonrpc":"2.0","id":1,"method":"ping"}', "sac-mcp-nomcp");
is($response->code, 200, "arkimeUser plus mcpUser is allowed");

################################################################################
# protocol
################################################################################
my $json = rpc("initialize", '{"protocolVersion":"2025-06-18"}');
is($json->{jsonrpc}, "2.0", "initialize jsonrpc version");
is($json->{id}, 1, "initialize echoes id");
is($json->{result}->{protocolVersion}, "2025-06-18", "initialize echoes the client protocol version");
is($json->{result}->{serverInfo}->{name}, "arkime-viewer", "initialize serverInfo name");
ok(defined $json->{result}->{capabilities}->{tools}, "initialize advertises tools");

$json = rpc("initialize", '{"protocolVersion":"1999-01-01"}');
is($json->{result}->{protocolVersion}, "2025-06-18", "unknown protocol version falls back to ours");

$json = rpc("ping");
eq_or_diff($json->{result}, from_json("{}"), "ping returns empty result");

$json = rpc("resources/list");
eq_or_diff($json->{result}->{resources}, from_json("[]"), "resources/list is empty");

$json = rpc("prompts/list");
eq_or_diff($json->{result}->{prompts}, from_json("[]"), "prompts/list is empty");

$json = rpc("totally/unknown");
is($json->{error}->{code}, -32601, "unknown method is -32601");

$json = mcp('{"id":1,"method":"ping"}');
is($json->{error}->{code}, -32600, "missing jsonrpc member is -32600");

$json = mcp('{not json');
is($json->{error}->{code}, -32700, "malformed json is -32700");

$json = mcp('[{"jsonrpc":"2.0","id":1,"method":"ping"}]');
is($json->{error}->{code}, -32600, "batch requests are rejected");

# a notification has no id: acknowledge with 202 and no body
$response = mcpRaw('{"jsonrpc":"2.0","method":"notifications/initialized"}');
is($response->code, 202, "notification gets 202");
is($response->content, "", "notification has an empty body");

# we implement only the POST half of streamable http
$response = $ArkimeTest::userAgent->get("$MCP?arkimeRegressionUser=anonymous");
is($response->code, 405, "GET /mcp is 405");
is($response->header("Allow"), "POST", "405 advertises Allow: POST");

################################################################################
# tools/list
################################################################################
$json = rpc("tools/list");
my $tools = $json->{result}->{tools};
ok(scalar @{$tools} >= 20, "tools/list returns the full tool set");

my $bad = 0;
my %names;
foreach my $tool (@{$tools}) {
    $bad++ if (!$tool->{name} || !$tool->{description} || !$tool->{inputSchema});
    $bad++ if ($tool->{inputSchema}->{type} ne "object");
    $names{$tool->{name}} = $tool;
}
is($bad, 0, "every tool has a name, description and object inputSchema");

ok(defined $names{"arkime_fields"}, "arkime_fields is present");
ok(defined $names{"arkime_sessions"}, "arkime_sessions is present");
ok($names{"arkime_sessions"}->{annotations}->{readOnlyHint}, "arkime_sessions is marked read only");
ok(!$names{"arkime_add_tags"}->{annotations}->{readOnlyHint}, "arkime_add_tags is not marked read only");

# pcap and admin surfaces must never be exposed
my $forbidden = 0;
foreach my $name (keys %names) {
    $forbidden++ if ($name =~ /pcap|packets|upload|delete|scrub|esadmin|user/i);
}
is($forbidden, 0, "no pcap, upload, delete, scrub, esadmin or user admin tools are exposed");

################################################################################
# tools/call
################################################################################
$json = callTool("arkime_nope");
is($json->{error}->{code}, -32602, "unknown tool is -32602");

$json = callTool("arkime_fields");
is($json->{result}->{isError}, JSON::false, "arkime_fields succeeds");
ok(scalar @{$json->{result}->{structuredContent}} > 100, "arkime_fields returns the field list");
ok(defined $json->{result}->{content}->[0]->{text}, "tool result has a text content block");

$json = callTool("arkime_sessions", '{"date":-1,"length":2}');
is($json->{result}->{isError}, JSON::false, "arkime_sessions succeeds");
ok($json->{result}->{structuredContent}->{recordsFiltered} > 0, "arkime_sessions finds sessions");
is(scalar @{$json->{result}->{structuredContent}->{data}}, 2, "arkime_sessions honors length");

$json = callTool("arkime_sessions", '{"date":-1,"length":1,"expression":"ip.dst == 10.0.0.1"}');
is($json->{result}->{isError}, JSON::false, "arkime_sessions with an expression succeeds");

# a bad expression must fail the tool, not the http request
$response = mcpRaw('{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"arkime_sessions","arguments":{"expression":"this is not valid &&"}}}');
is($response->code, 200, "a failing tool is still http 200");
$json = from_json($response->content);
is($json->{result}->{isError}, JSON::true, "a failing tool sets isError");
ok(defined $json->{result}->{content}->[0]->{text}, "a failing tool explains itself");

$json = callTool("arkime_spigraph", '{"date":-1,"exp":"ip.dst","size":3}');
is($json->{result}->{isError}, JSON::false, "arkime_spigraph succeeds");
ok(scalar @{$json->{result}->{structuredContent}->{items}} > 0, "arkime_spigraph returns items");

$json = callTool("arkime_unique", '{"date":-1,"exp":"ip.dst","counts":1}');
is($json->{result}->{isError}, JSON::false, "arkime_unique succeeds");
ok(scalar @{$json->{result}->{structuredContent}->{values}} > 0, "arkime_unique returns values");

$json = callTool("arkime_multiunique", '{"date":-1,"exp":"ip.src,ip.dst","counts":1}');
is($json->{result}->{isError}, JSON::false, "arkime_multiunique succeeds");

$json = callTool("arkime_spiview", '{"date":-1,"spi":"destination.ip:3"}');
is($json->{result}->{isError}, JSON::false, "arkime_spiview succeeds");

$json = callTool("arkime_connections", '{"date":-1}');
is($json->{result}->{isError}, JSON::false, "arkime_connections succeeds");
ok(scalar @{$json->{result}->{structuredContent}->{nodes}} > 0, "arkime_connections returns nodes");

$json = callTool("arkime_buildquery", '{"date":-1,"expression":"ip.dst == 10.0.0.1"}');
is($json->{result}->{isError}, JSON::false, "arkime_buildquery succeeds");
ok(defined $json->{result}->{structuredContent}->{esquery}, "arkime_buildquery returns the es query");

$json = callTool("arkime_esindices");
is($json->{result}->{isError}, JSON::false, "arkime_esindices succeeds");

$json = callTool("arkime_files", '{"length":2}');
is($json->{result}->{isError}, JSON::false, "arkime_files succeeds");

$json = callTool("arkime_histories", '{"length":2}');
is($json->{result}->{isError}, JSON::false, "arkime_histories succeeds");

$json = callTool("arkime_views");
is($json->{result}->{isError}, JSON::false, "arkime_views succeeds");

$json = callTool("arkime_shortcuts");
is($json->{result}->{isError}, JSON::false, "arkime_shortcuts succeeds");

################################################################################
# access scoping - tools must not escape the user's own permissions
################################################################################
viewerPostToken("/api/user", '{"userId": "sac-mcp-limited", "userName": "sac-mcp-limited", "enabled":true, "password":"password", "roles":["arkimeUser","mcpUser"], "packetSearch":false, "hideStats":true, "hideFiles":true}', $adminToken);

$json = callTool("arkime_hunts", "{}", "sac-mcp-limited");
is($json->{result}->{isError}, JSON::true, "arkime_hunts denied without packetSearch");

$json = callTool("arkime_stats", "{}", "sac-mcp-limited");
is($json->{result}->{isError}, JSON::true, "arkime_stats denied with hideStats");

$json = callTool("arkime_files", "{}", "sac-mcp-limited");
is($json->{result}->{isError}, JSON::true, "arkime_files denied with hideFiles");

$json = callTool("arkime_create_hunt", '{"name":"nope","search":"x","searchType":"ascii","startTime":1,"stopTime":2}', "sac-mcp-limited");
is($json->{result}->{isError}, JSON::true, "arkime_create_hunt denied without packetSearch");

# the same tools work for a user that does have the permissions
$json = callTool("arkime_hunts", "{}", "superAdmin");
is($json->{result}->{isError}, JSON::false, "arkime_hunts allowed with packetSearch");

# a user's forced expression must be applied to their queries
viewerPostToken("/api/user", '{"userId": "sac-mcp-forced", "userName": "sac-mcp-forced", "enabled":true, "password":"password", "roles":["arkimeUser","mcpUser"], "expression":"ip.dst == 10.0.0.1"}', $adminToken);
$json = callTool("arkime_buildquery", '{"date":-1}', "sac-mcp-forced");
my $esquery = to_json($json->{result}->{structuredContent}->{esquery});
ok($esquery =~ /10\.0\.0\.1/, "the user's forced expression is applied to MCP queries");

################################################################################
# write tools
################################################################################
viewerGet("/regressionTests/deleteAllViews");

$json = callTool("arkime_create_view", '{"name":"mcpview","expression":"ip.dst == 9.9.9.9"}');
is($json->{result}->{isError}, JSON::false, "arkime_create_view succeeds");

$json = callTool("arkime_views");
is(scalar @{$json->{result}->{structuredContent}->{data}}, 1, "the created view is listed");
is($json->{result}->{structuredContent}->{data}->[0]->{name}, "mcpview", "the created view has the right name");

# tagging nothing is a tool error, not an http error
$json = callTool("arkime_add_tags", '{"date":-1,"expression":"ip.dst == 199.199.199.199","tags":"mcpnomatch"}');
is($json->{result}->{isError}, JSON::true, "arkime_add_tags with no matches is a tool error");

$json = callTool("arkime_create_hunt", '{"name":"mcphunt","search":"zzz","searchType":"ascii","startTime":1,"stopTime":2,"expression":"ip.dst == 199.199.199.199"}');
is($json->{result}->{isError}, JSON::true, "arkime_create_hunt with no matching sessions is a tool error");

$json = callTool("arkime_create_hunt", '{"name":"mcphunt","search":"zzz","searchType":"ascii"}');
is($json->{result}->{isError}, JSON::true, "arkime_create_hunt requires a time window");

viewerGet("/regressionTests/deleteAllViews");

################################################################################
# mcpMaxQueryDays - the test2 viewer on 8124 sets nothing, so this exercises the
# shipped default of 7 days. The test viewer on 8123 sets it to -1 explicitly,
# which is why every query above can use date=-1.
################################################################################
sub mcp2 {
my ($name, $args) = @_;
    my $body = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"$name\",\"arguments\":$args}}";
    my $r = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8124/mcp?arkimeRegressionUser=superAdmin",
        Content => $body, "Content-Type" => "application/json");
    return from_json($r->content);
}

$json = mcp2("arkime_sessions", '{"date":24,"length":1}');
is($json->{result}->{isError}, JSON::false, "1 day is under the default 7 day cap");

$json = mcp2("arkime_sessions", '{"date":144,"length":1}');
is($json->{result}->{isError}, JSON::false, "6 days is under the default 7 day cap");

$json = mcp2("arkime_sessions", '{"date":240,"length":1}');
is($json->{result}->{isError}, JSON::true, "10 days exceeds the default 7 day cap");
like($json->{result}->{content}->[0]->{text}, qr/mcpMaxQueryDays is 7/, "the cap error names the setting and the default");

$json = mcp2("arkime_sessions", '{"date":-1,"length":1}');
is($json->{result}->{isError}, JSON::true, "searching all data is refused under the default cap");

# an explicit window is checked too, not just date
$json = mcp2("arkime_sessions", '{"startTime":1386000000,"stopTime":1387000000,"length":1}');
is($json->{result}->{isError}, JSON::true, "an explicit window of 11 days is refused");

$json = mcp2("arkime_sessions", '{"startTime":1386000000,"stopTime":1386300000,"length":1}');
is($json->{result}->{isError}, JSON::false, "an explicit window of 3 days is allowed");

# the cap applies to aggregations too, not just session search
$json = mcp2("arkime_spigraph", '{"date":-1,"exp":"ip.dst"}');
is($json->{result}->{isError}, JSON::true, "the cap also applies to arkime_spigraph");

################################################################################
# mcpAuthMode=header - the test3 viewer on 8126 runs authMode=header, which is
# the production shape when apache/nginx verifies the token and passes the user
# name through. Users are auto created by autocreate.ini, which only grants
# mcpUser when the x-test-mcp header says so.
################################################################################
my $MCP3 = "http://$ArkimeTest::host:8126/mcp";
my $PING = '{"jsonrpc":"2.0","id":1,"method":"ping"}';

# no username header at all
$response = $ArkimeTest::userAgent->post($MCP3, Content => $PING, "Content-Type" => "application/json");
is($response->code, 401, "header auth with no username header gets 401");
is($response->header("WWW-Authenticate"), 'Bearer realm="arkime-viewer"', "and still sends WWW-Authenticate");
ok(!$response->is_redirect, "and does not redirect");

# a valid header user, but autocreate did not grant mcpUser
$response = $ArkimeTest::userAgent->post($MCP3, Content => $PING, "Content-Type" => "application/json",
    ':arkime_user' => 'sac-mcp-hdr1', ':arkime_user_name' => 'sac-mcp-hdr1');
is($response->code, 403, "header user without mcpUser gets 403");
like($response->content, qr/mcpUser/, "and is told which role is missing");

# same flow, but x-test-mcp makes the role mapping grant mcpUser
$response = $ArkimeTest::userAgent->post($MCP3, Content => $PING, "Content-Type" => "application/json",
    ':arkime_user' => 'sac-mcp-hdr2', ':arkime_user_name' => 'sac-mcp-hdr2', ':x-test-mcp' => 'yes');
is($response->code, 200, "header user with mcpUser is allowed");
$json = from_json($response->content);
eq_or_diff($json->{result}, from_json("{}"), "and gets a real JSON-RPC result");

# and a real tool call authenticated purely by the proxy set header
$response = $ArkimeTest::userAgent->post($MCP3,
    Content => '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"arkime_fields","arguments":{}}}',
    "Content-Type" => "application/json",
    ':arkime_user' => 'sac-mcp-hdr2', ':arkime_user_name' => 'sac-mcp-hdr2', ':x-test-mcp' => 'yes');
is($response->code, 200, "tools/call over header auth is 200");
$json = from_json($response->content);
is($json->{result}->{isError}, JSON::false, "the tool ran");
ok(scalar @{$json->{result}->{structuredContent}} > 100, "and returned the field list");

# header auth auto created users, clean up after everything
viewerGet("/regressionTests/deleteAllViews");
viewerGet("/regressionTests/deleteAllUsers");
