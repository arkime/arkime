use Test::More tests => 97;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $MCP = "http://$ArkimeTest::host:3218/mcp";
my $token = getCont3xtTokenCookie('superAdmin');

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
is(scalar @{$tools}, 8, "tools/list returns every cont3xt tool");

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
ok(defined $names{"cont3xt_ui_link"}, "cont3xt_ui_link is present");

################################################################################
# tools/call
################################################################################
$json = callTool("cont3xt_classify", '{"query":"8.8.8.8"}');
is($json->{result}->{isError}, JSON::false, "cont3xt_classify succeeds");
is($json->{result}->{structuredContent}->{itype}, "ip", "cont3xt_classify detects an ip");
# web ui links use arkimeWebURL from the test config; classify only prefills
is($json->{result}->{structuredContent}->{uiUrl}, "http://localhost:3218/?b=OC44LjguOA%3D%3D", "cont3xt_classify links to the prefilled search page");

# b is latin1 base64, wider chars have no link
$json = callTool("cont3xt_classify", '{"query":"bücher.de"}');
is($json->{result}->{structuredContent}->{uiUrl}, "http://localhost:3218/?b=YvxjaGVyLmRl", "cont3xt_classify encodes b the way the ui does");
$json = callTool("cont3xt_classify", '{"query":"例え.jp"}');
ok(!defined $json->{result}->{structuredContent}->{uiUrl}, "cont3xt_classify has no link for a query the ui cannot encode");

$json = callTool("cont3xt_classify", '{"query":"example.com"}');
is($json->{result}->{structuredContent}->{itype}, "domain", "cont3xt_classify detects a domain");

$json = callTool("cont3xt_classify", '{"query":""}');
is($json->{result}->{isError}, JSON::true, "cont3xt_classify rejects an empty query");

$json = callTool("cont3xt_list_integrations");
is($json->{result}->{isError}, JSON::false, "cont3xt_list_integrations succeeds");
ok(defined $json->{result}->{structuredContent}->{integrations}, "cont3xt_list_integrations returns integrations");

# mcpDefaultView (mcpdefault in the test config) applies to every search that
# names neither a view nor doIntegrations, so until the view exists those fail
# loudly instead of quietly running every integration
$json = callTool("cont3xt_search", '{"query":"8.8.8.8","skipChildren":true}');
is($json->{result}->{isError}, JSON::true, "cont3xt_search fails when mcpDefaultView names a missing view");
like($json->{result}->{content}->[0]->{text}, qr/mcpDefaultView/, "the error names the setting");

# no integrations list means unrestricted, matching the web ui
$json = cont3xtPostToken('/api/view?arkimeRegressionUser=superAdmin', to_json({ name => "mcpdefault" }), $token);
is($json->{success}, JSON::true, "created the default view for the mcp tests");
my $defaultViewId = $json->{view}->{_id};
esGet("/_refresh");

# the search handler streams newline delimited json, the tool must buffer it
# all and fold it into a single result
$json = callTool("cont3xt_search", '{"query":"8.8.8.8","skipChildren":true}');
is($json->{result}->{isError}, JSON::false, "cont3xt_search succeeds");
my $search = $json->{result}->{structuredContent};
is($search->{indicators}->[0]->{itype}, "ip", "cont3xt_search classified the indicator");
ok(scalar @{$search->{results}} > 0, "cont3xt_search returns integration results");
ok(!defined $search->{partial}, "cont3xt_search completed rather than timing out");
is($search->{view}->{name}, "mcpdefault", "cont3xt_search used mcpDefaultView");
is($search->{uiUrl}, "http://localhost:3218/?b=OC44LjguOA%3D%3D&submit=y&view=$defaultViewId&skipChildren=true", "cont3xt_search links to the same search in the web ui, default view included");

# an explicit doIntegrations restriction has no representation in the web ui's
# url (only a view does), so the link must be omitted rather than pointing at
# a search that could run a different, wider set of integrations
$json = callTool("cont3xt_search", '{"query":"8.8.8.8","doIntegrations":["test"],"skipChildren":true}');
is($json->{result}->{isError}, JSON::false, "cont3xt_search with an explicit doIntegrations succeeds");
ok(!defined $json->{result}->{structuredContent}->{uiUrl}, "cont3xt_search has no link when doIntegrations is set without a view");

# search with a view
$json = cont3xtPostToken('/api/view?arkimeRegressionUser=superAdmin', to_json({ name => "mcpview", integrations => ["nosuchintegration"] }), $token);
is($json->{success}, JSON::true, "created a view for the mcp tests");
my $viewId = $json->{view}->{_id};
esGet("/_refresh");

$json = callTool("cont3xt_search", '{"query":"8.8.8.8","view":"mcpview","skipChildren":true}');
is($json->{result}->{isError}, JSON::false, "cont3xt_search with a view by name succeeds");
is(scalar @{$json->{result}->{structuredContent}->{results}}, 0, "cont3xt_search only ran the view's integrations");
is($json->{result}->{structuredContent}->{view}->{name}, "mcpview", "cont3xt_search reports the view it used");
is($json->{result}->{structuredContent}->{uiUrl}, "http://localhost:3218/?b=OC44LjguOA%3D%3D&submit=y&view=$viewId&skipChildren=true", "cont3xt_search link selects the view");

$json = callTool("cont3xt_search", '{"query":"8.8.8.8","view":"nosuchview"}');
is($json->{result}->{isError}, JSON::true, "cont3xt_search rejects an unknown view");

$json = callTool("cont3xt_ui_link", '{"query":"example.com"}');
is($json->{result}->{isError}, JSON::false, "cont3xt_ui_link succeeds");
is($json->{result}->{structuredContent}->{uiUrl}, "http://localhost:3218/?b=ZXhhbXBsZS5jb20%3D", "cont3xt_ui_link builds a prefill link by default");

$json = callTool("cont3xt_ui_link", '{"query":"example.com","submit":true,"view":"mcpview"}');
is($json->{result}->{structuredContent}->{uiUrl}, "http://localhost:3218/?b=ZXhhbXBsZS5jb20%3D&submit=y&view=mcpview", "cont3xt_ui_link passes the view through for the ui to resolve");

$json = cont3xtDeleteToken("/api/view/$viewId?arkimeRegressionUser=superAdmin", '{}', $token);
is($json->{success}, JSON::true, "removed the mcp test view");

# a view with no integrations field at all means "no restriction" (matching
# how the web ui treats it), not "restrict to zero integrations"
$json = cont3xtPostToken('/api/view?arkimeRegressionUser=superAdmin', to_json({ name => "mcpnointegrations" }), $token);
is($json->{success}, JSON::true, "created a view with no integrations list");
my $noIntegrationsViewId = $json->{view}->{_id};
esGet("/_refresh");

$json = callTool("cont3xt_search", "{\"query\":\"8.8.8.8\",\"view\":\"$noIntegrationsViewId\",\"skipChildren\":true}");
is($json->{result}->{isError}, JSON::false, "cont3xt_search with an unrestricted view succeeds");
ok(scalar @{$json->{result}->{structuredContent}->{results}} > 0, "cont3xt_search with a view lacking an integrations list runs unrestricted, not zero integrations");

$json = cont3xtDeleteToken("/api/view/$noIntegrationsViewId?arkimeRegressionUser=superAdmin", '{}', $token);
is($json->{success}, JSON::true, "removed the no-integrations test view");

$json = callTool("cont3xt_search", '{"query":"","doIntegrations":["csv:rir"]}');
is($json->{result}->{isError}, JSON::true, "cont3xt_search rejects an empty query");

################################################################################
# overview (the default) vs full detail
################################################################################
# the default answer is the overview per indicator, results only says which
# integrations answered, the raw data is left out
$json = callTool("cont3xt_search", '{"query":"10.20.30.50","doIntegrations":["csv:rir","json:ipwise"],"skipChildren":true}');
is($json->{result}->{isError}, JSON::false, "cont3xt_search overview detail succeeds");
$search = $json->{result}->{structuredContent};
is(scalar @{$search->{results}}, 2, "overview detail still lists every integration that answered");
my ($rir) = grep { $_->{integration} eq "csv:rir" } @{$search->{results}};
ok(!exists $rir->{data}, "overview detail leaves the raw integration data out");
is($rir->{count}, 1, "overview detail reports each integration's result count");
is(scalar @{$search->{overviews}}, 1, "overview detail has one overview per indicator");
is($search->{overviews}->[0]->{indicator}->{query}, "10.20.30.50", "the overview names its indicator");
ok(ref $search->{overviews}->[0]->{fields} eq "ARRAY", "the overview has a fields array");

$json = callTool("cont3xt_search", '{"query":"10.20.30.50","doIntegrations":["csv:rir","json:ipwise"],"skipChildren":true,"detail":"full"}');
is($json->{result}->{isError}, JSON::false, "cont3xt_search full detail succeeds");
$search = $json->{result}->{structuredContent};
($rir) = grep { $_->{integration} eq "csv:rir" } @{$search->{results}};
is($rir->{data}->{data}->{Designation}, "IANA - Private Use", "full detail returns the raw integration data");
ok(!defined $search->{overviews}, "full detail has no overviews");

$json = callTool("cont3xt_search", '{"query":"10.20.30.50","doIntegrations":["csv:rir"],"detail":"nope"}');
is($json->{result}->{isError}, JSON::true, "cont3xt_search rejects an unknown detail");

# an overview pulls linked card fields or its own custom fields out of each
# integration's data, exactly what the web ui's overview card shows
$json = cont3xtPutToken('/api/overview?arkimeRegressionUser=superAdmin', to_json({
    name => "mcpoverview", title => "MCP %{query}", iType => "ip",
    viewRoles => ["cont3xtUser"], editRoles => ["superAdmin"],
    fields => [
        { type => "linked", from => "json:ipwise", field => "data", alias => "IPWise" },
        { type => "custom", from => "csv:rir", custom => { label => "Designation", field => "data.Designation" } },
        { type => "linked", from => "csv:rir", field => "nosuchfield" },
        { type => "custom", from => "csv:rir", custom => { label => "Empty", field => "data.WHOIS" } }
    ]
}), $token);
is($json->{success}, JSON::true, "created an overview for the mcp tests");
esGet("/_refresh");

$json = callTool("cont3xt_search", '{"query":"10.20.30.50","doIntegrations":["csv:rir","json:ipwise"],"skipChildren":true,"overview":"mcpoverview"}');
is($json->{result}->{isError}, JSON::false, "cont3xt_search with an overview by name succeeds");
my $ov = $json->{result}->{structuredContent}->{overviews}->[0];
is($ov->{overview}->{name}, "mcpoverview", "the requested overview was used");
is(scalar @{$ov->{fields}}, 2, "empty fields and fields whose integration didn't answer are left out");
is($ov->{fields}->[0]->{label}, "IPWise", "a linked field is labelled with its alias");
is($ov->{fields}->[0]->{integration}, "json:ipwise", "a field names the integration it came from");
is($ov->{fields}->[0]->{value}->{tag}, "ipwise-array", "a linked json card field carries the integration's data");
is($ov->{fields}->[1]->{label}, "Designation", "a custom field is labelled by its own label");
is($ov->{fields}->[1]->{value}, "IANA - Private Use", "a custom field reads its path out of the data");
is(scalar @{$ov->{warnings}}, 1, "a linked field that doesn't exist is reported");
like($ov->{warnings}->[0], qr/nosuchfield/, "the warning names the missing field");

$json = callTool("cont3xt_search", '{"query":"10.20.30.50","doIntegrations":["csv:rir"],"overview":"nosuchoverview"}');
is($json->{result}->{isError}, JSON::true, "cont3xt_search rejects an unknown overview");

$json = cont3xtGet('/api/overview?arkimeRegressionUser=superAdmin');
my ($mcpOverview) = grep { $_->{name} eq "mcpoverview" } @{$json->{overviews}};
$json = cont3xtDeleteToken("/api/overview/$mcpOverview->{_id}?arkimeRegressionUser=superAdmin", '{}', $token);
is($json->{success}, JSON::true, "removed the mcp test overview");

################################################################################
# tags
################################################################################
$json = callTool("cont3xt_search", '{"query":"10.20.30.51","view":"mcpdefault","doIntegrations":["csv:rir"],"skipChildren":true,"tags":["mcptag","case-42"]}');
is($json->{result}->{isError}, JSON::false, "cont3xt_search with tags succeeds");
is($json->{result}->{structuredContent}->{uiUrl}, "http://localhost:3218/?b=MTAuMjAuMzAuNTE%3D&submit=y&view=$defaultViewId&tags=mcptag%2Ccase-42&skipChildren=true", "the web ui link carries the tags");
# the audit is written after the response is sent, give it a moment to land
sleep(1);
esGet("/_flush");
esGet("/_refresh");
$json = cont3xtGet('/api/audits?searchTerm=mcptag&arkimeRegressionUser=superAdmin');
is(scalar @{$json->{audits}}, 1, "the tags were recorded in the search history");
eq_or_diff($json->{audits}->[0]->{tags}, ["mcptag", "case-42"], "the history has every tag");
$json = cont3xtDeleteToken("/api/audit/$json->{audits}->[0]->{_id}?arkimeRegressionUser=superAdmin", '{}', $token);
is($json->{success}, JSON::true, "removed the tagged history entry");

$json = callTool("cont3xt_search", '{"query":"10.20.30.51","doIntegrations":["csv:rir"],"tags":"mcptag"}');
is($json->{result}->{isError}, JSON::true, "cont3xt_search rejects tags that aren't an array");

$json = callTool("cont3xt_ui_link", '{"query":"example.com","tags":["a","b"],"submit":true}');
is($json->{result}->{structuredContent}->{uiUrl}, "http://localhost:3218/?b=ZXhhbXBsZS5jb20%3D&submit=y&tags=a%2Cb", "cont3xt_ui_link carries the tags");

################################################################################
# mcpMaxResultBytes (2048 in the test config)
################################################################################
# the largest entries are emptied and marked, never silently cut. The view
# makes the web ui link part of the result, and the cap must hold with it
$json = callTool("cont3xt_search", '{"query":"10.20.30.50 10.20.30.51 10.20.30.52 10.20.30.53","view":"mcpdefault","doIntegrations":["csv:rir","json:ipwise","Maxmind"],"skipChildren":true,"detail":"full"}');
is($json->{result}->{isError}, JSON::false, "an oversized full search still succeeds");
$search = $json->{result}->{structuredContent};
like($search->{uiUrl}, qr/^http:\/\/localhost:3218\/\?b=.*&view=$defaultViewId&/, "the oversized result still carries its web ui link");
ok(defined $search->{truncated}, "an oversized result is marked truncated");
ok($search->{truncated}->{bytes} > 2048, "truncated reports the original size");
is($search->{truncated}->{limit}, 2048, "truncated reports the limit");
my @omitted = grep { $_->{omitted} } @{$search->{results}};
is(scalar @omitted, $search->{truncated}->{omitted}, "every omitted entry is counted");
ok(scalar @omitted > 0 && scalar @omitted < scalar @{$search->{results}}, "only as many entries as needed were omitted");
ok(!exists $omitted[0]->{data} && $omitted[0]->{bytes} > 0, "an omitted entry has no data but says how big it was");
ok(length(to_json($search)) <= 2048, "the result is within the limit");

$json = callTool("cont3xt_search", '{"query":"10.20.30.50","doIntegrations":["csv:rir"],"skipChildren":true,"detail":"full"}');
ok(!defined $json->{result}->{structuredContent}->{truncated}, "a result within the limit is untouched");

$json = cont3xtDeleteToken("/api/view/$defaultViewId?arkimeRegressionUser=superAdmin", '{}', $token);
is($json->{success}, JSON::true, "removed the mcp default view");

$json = callTool("cont3xt_views");
is($json->{result}->{isError}, JSON::false, "cont3xt_views succeeds");
