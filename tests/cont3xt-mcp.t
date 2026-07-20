use Test::More tests => 30;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $MCP = "http://$ArkimeTest::host:3218/mcp";

# Raw POST so we can assert on status codes and headers
sub mcpRaw {
my ($body, $user) = @_;
    # superAdmin expands to mcpUser, see systemRolesMapping
    $user = "superAdmin" if (!defined $user);
    return $ArkimeTest::userAgent->post("$MCP?arkimeRegressionUser=$user",
        Content => $body, "Content-Type" => "application/json");
}

sub mcp {
my ($body, $user) = @_;
    return from_json(mcpRaw($body, $user)->content);
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
# auth
################################################################################
# sac- prefixed users are never auto created
my $response = mcpRaw('{"jsonrpc":"2.0","id":1,"method":"ping"}', "sac-cont3xt-nobody");
is($response->code, 401, "unknown user gets 401");
is($response->header("WWW-Authenticate"), 'Bearer realm="arkime-cont3xt"', "401 carries WWW-Authenticate");
ok(!$response->is_redirect, "auth failure is not a redirect");

################################################################################
# protocol
################################################################################
my $json = rpc("initialize", '{"protocolVersion":"2025-06-18"}');
is($json->{result}->{protocolVersion}, "2025-06-18", "initialize echoes the client protocol version");
is($json->{result}->{serverInfo}->{name}, "arkime-cont3xt", "initialize serverInfo name");

$json = rpc("ping");
eq_or_diff($json->{result}, from_json("{}"), "ping returns empty result");

$json = rpc("resources/list");
eq_or_diff($json->{result}->{resources}, from_json("[]"), "resources/list is empty");

$json = rpc("prompts/list");
eq_or_diff($json->{result}->{prompts}, from_json("[]"), "prompts/list is empty");

$json = rpc("totally/unknown");
is($json->{error}->{code}, -32601, "unknown method is -32601");

$json = mcp('{not json');
is($json->{error}->{code}, -32700, "malformed json is -32700");

$response = mcpRaw('{"jsonrpc":"2.0","method":"notifications/initialized"}');
is($response->code, 202, "notification gets 202");
is($response->content, "", "notification has an empty body");

$response = $ArkimeTest::userAgent->get("$MCP?arkimeRegressionUser=superAdmin");
is($response->code, 405, "GET /mcp is 405");
is($response->header("Allow"), "POST", "405 advertises Allow: POST");

################################################################################
# tools/list
################################################################################
$json = rpc("tools/list");
my $tools = $json->{result}->{tools};
is(scalar @{$tools}, 7, "tools/list returns every cont3xt tool");

my $bad = 0;
my %names;
foreach my $tool (@{$tools}) {
    $bad++ if (!$tool->{name} || !$tool->{description} || $tool->{inputSchema}->{type} ne "object");
    # cont3xt is read only for now
    $bad++ if (!$tool->{annotations}->{readOnlyHint});
    $names{$tool->{name}} = $tool;
}
is($bad, 0, "every tool has a name, description, object inputSchema and is read only");
ok(defined $names{"cont3xt_search"}, "cont3xt_search is present");
ok(defined $names{"cont3xt_classify"}, "cont3xt_classify is present");

################################################################################
# tools/call
################################################################################
$json = callTool("cont3xt_classify", '{"query":"8.8.8.8"}');
is($json->{result}->{isError}, JSON::false, "cont3xt_classify succeeds");
is($json->{result}->{structuredContent}->{itype}, "ip", "cont3xt_classify detects an ip");

$json = callTool("cont3xt_classify", '{"query":"example.com"}');
is($json->{result}->{structuredContent}->{itype}, "domain", "cont3xt_classify detects a domain");

$json = callTool("cont3xt_classify", '{"query":""}');
is($json->{result}->{isError}, JSON::true, "cont3xt_classify rejects an empty query");

$json = callTool("cont3xt_list_integrations");
is($json->{result}->{isError}, JSON::false, "cont3xt_list_integrations succeeds");
ok(defined $json->{result}->{structuredContent}->{integrations}, "cont3xt_list_integrations returns integrations");

# the search handler streams newline delimited json, the tool must buffer it
# all and fold it into a single result
$json = callTool("cont3xt_search", '{"query":"8.8.8.8","skipChildren":true}');
is($json->{result}->{isError}, JSON::false, "cont3xt_search succeeds");
my $search = $json->{result}->{structuredContent};
is($search->{indicators}->[0]->{itype}, "ip", "cont3xt_search classified the indicator");
ok(scalar @{$search->{results}} > 0, "cont3xt_search returns integration results");
ok(!defined $search->{partial}, "cont3xt_search completed rather than timing out");

$json = callTool("cont3xt_search", '{"query":""}');
is($json->{result}->{isError}, JSON::true, "cont3xt_search rejects an empty query");

$json = callTool("cont3xt_views");
is($json->{result}->{isError}, JSON::false, "cont3xt_views succeeds");
