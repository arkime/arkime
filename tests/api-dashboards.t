# System dashboards: the *.dashboard.json files in the dashboardsDir
# directories (config.test.ini lists release/dashboards, then tests/dashboards,
# then capture/plugins, whose plugin dashboards stay hidden until the plugin's
# fields exist)
use Test::More tests => 62;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $token = getTokenCookie();

my $info = viewerGet("/api/dashboards/system");
my @list = @{$info->{data}};
my %byId = map { $_->{id} => $_ } @list;
my %byName = map { $_->{name} => $_ } @list;

# the shipped ones plus the extra test one, in directory then file name order
ok(scalar(@list) >= 53, "lists the shipped dashboards");
is($list[0]->{id}, "system-adb", "first dashboard is the first file name of the first directory");
my ($zzIndex) = grep { $list[$_]->{id} eq "system-zz-test" } 0..$#list;
my ($wsIndex) = grep { $list[$_]->{id} eq "system-websocket" } 0..$#list;
ok(defined $zzIndex && $zzIndex > $wsIndex, "a later directory's dashboards come after the first directory's");
is(scalar(keys %byId), scalar(@list), "ids are unique");
ok((grep { $_->{id} !~ /^system-[a-z0-9_-]+$/i } @list) == 0, "every id is system-<file name>");
ok((grep { $_->{type} ne "summaryConfig" } @list) == 0, "every entry is a summaryConfig");
ok((grep { !$_->{system} } @list) == 0, "every entry is marked system");
ok((grep { $_->{canEdit} || $_->{canDelete} } @list) == 0, "none can be edited or deleted");
ok((grep { ref($_->{data}->{widgets}) ne "ARRAY" || !@{$_->{data}->{widgets}} } @list) == 0, "every entry has widgets");
ok((grep { $_->{data}->{version} != 1 } @list) == 0, "every entry is dashboard version 1");

# a shipped protocol dashboard
my $http = $byId{"system-http"};
ok($http, "system-http is listed");
is($http->{name}, "HTTP", "http name");
is($http->{data}->{folder}, "Protocols", "http is in the Protocols folder");
is($http->{data}->{colorScheme}, "rainbow", "http color scheme");
ok((grep { $_->{viewMode} eq "navigation" } @{$http->{data}->{widgets}}) == 1, "http has one navigation widget");
is($http->{data}->{expression}, "protocols == http", "http filters on the protocol with a dashboard filter");
ok((grep { $_->{expression} } @{$http->{data}->{widgets}}) == 0, "no http widget has its own filter");
my ($sessions) = grep { $_->{viewMode} eq "sessions" } @{$http->{data}->{widgets}};
ok($sessions, "http has a sessions widget");
eq_or_diff($sessions->{fields}, ["ip.src", "ip.dst", "http.method", "host.http", "http.uri", "http.statuscode"], "http sessions widget columns");

# the ungrouped ones and the ICS protocols
ok($byId{"system-overview"}, "system-overview is listed");
ok(!exists $byId{"system-overview"}->{data}->{folder}, "overview has no folder");
ok($byId{"system-connections"}, "system-connections is listed");
is($byId{"system-security"}->{data}->{folder}, "Analysis", "security is in the Analysis folder");
ok((grep { $_->{viewMode} eq "tree" } @{$byId{"system-ssh"}->{data}->{widgets}}) == 1, "ssh has a tree widget");
is($byId{"system-modbus"}->{data}->{folder}, "Protocols", "modbus is in the Protocols folder");
is($byId{"system-ot"}->{name}, "OT", "ot name");
is($byId{"system-ot"}->{data}->{folder}, "Analysis", "ot is in the Analysis folder");
my %folders = map { ($_->{data}->{folder} // "") => 1 } @list;
eq_or_diff([sort keys %folders], ["", "Analysis", "Protocols", "Test"], "folders");

# tests/dashboards: an extra one is added, a duplicate name loses to the
# earlier directory, a broken file and one without a name are skipped
my $extra = $byId{"system-zz-test"};
ok($extra, "system-zz-test from tests/dashboards is listed");
is($extra->{name}, "Test extra", "extra name");
is($extra->{data}->{folder}, "Test", "extra folder");
is($http->{name}, "HTTP", "tests/dashboards/http.dashboard.json is ignored, release/dashboards wins");
ok(!exists $byName{"Duplicate HTTP"}, "the duplicate's name is not listed");
ok(!exists $byId{"system-broken"}, "a file that isn't JSON is skipped");
ok(!exists $byId{"system-noname"}, "a file without a name is skipped");
ok(!exists $byId{"system-badfolder"}, "a file whose folder breaks the folder rule is skipped");
ok(!exists $byId{"system-README"}, "only *.dashboard.json files are read");

# requires: hidden until every listed field exists
ok(!exists $byId{"system-requires-missing"}, "a dashboard requiring an unknown field is hidden");
ok($byId{"system-requires-known"}, "a dashboard whose required fields all exist is listed");
eq_or_diff($byId{"system-requires-known"}->{requires}, ["protocols", "ip.src"], "requires is listed");

# the cross-protocol dashboards
for my $key ("routing", "multicast", "databases") {
    is($byId{"system-$key"}->{data}->{folder}, "Analysis", "$key is in the Analysis folder");
}
ok((grep { $_->{viewMode} eq "bar" && ($_->{orientation} // "") eq "horizontal" } @{$byId{"system-multicast"}->{data}->{widgets}}) == 1, "multicast has a horizontal bar");
ok((grep { $_->{viewMode} eq "area" } @{$byId{"system-connections"}->{data}->{widgets}}) == 1, "connections has an area");
ok((grep { $_->{viewMode} eq "combo" } @{$byId{"system-connections"}->{data}->{widgets}}) == 1, "connections has a combo");
ok((grep { $_->{viewMode} eq "scatter" } @{$byId{"system-connections"}->{data}->{widgets}}) == 1, "connections has a scatter");

# a dashboard id that isn't stored is a 404 (the page drops it from the url)
my $missing = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/shareable/does-not-exist");
is($missing->code, 404, "an unknown shareable id is a 404");
$info = viewerGet("/api/shareable/does-not-exist");
is($info->{i18n}, "api.shareables.notFound", "an unknown shareable id has the not found error");
$info = viewerPutToken("/api/shareable/does-not-exist", '{"name":"x"}', $token);
is($info->{i18n}, "api.shareables.notFound", "updating an unknown shareable id is not found");
$info = viewerDeleteToken("/api/shareable/does-not-exist", $token);
is($info->{i18n}, "api.shareables.notFound", "deleting an unknown shareable id is not found");

# a system id is not a shareable
$info = viewerGet("/api/shareable/system-http");
ok(!$info->{success}, "system-http is not a shareable");
$info = viewerPutToken("/api/shareable/system-http", '{"name":"x"}', $token);
ok(!$info->{success}, "a system dashboard can't be updated as a shareable");
$info = viewerDeleteToken("/api/shareable/system-http", $token);
ok(!$info->{success}, "a system dashboard can't be deleted as a shareable");

# the dashboard version a saved dashboard carries must be a positive integer
$info = viewerPostToken("/api/shareable", '{"name":"dash versioned","type":"summaryConfig","data":{"version":1,"widgets":[{"id":"w1","viewMode":"stats"}]}}', $token);
ok($info->{success}, "create summaryConfig with version 1 succeeds");
viewerDeleteToken("/api/shareable/$info->{id}", $token);
$info = viewerPostToken("/api/shareable", '{"name":"dash bad version","type":"summaryConfig","data":{"version":"old","widgets":[{"id":"w1","viewMode":"stats"}]}}', $token);
is($info->{i18n}, "api.shareables.invalidVersion", "create summaryConfig with a bad version fails");

# the folder: '' or up to 64 visible characters; the dashboard expression is capped
my $widgets = '"widgets":[{"id":"w1","viewMode":"stats"}]';
$info = viewerPostToken("/api/shareable", '{"name":"dash foldered","type":"summaryConfig","data":{"folder":"Ops / NOC","expression":"protocols == dns",' . $widgets . '}}', $token);
ok($info->{success}, "create summaryConfig with a folder and an expression succeeds");
viewerDeleteToken("/api/shareable/$info->{id}", $token);
$info = viewerPostToken("/api/shareable", '{"name":"dash bad folder","type":"summaryConfig","data":{"folder":{"x":1},' . $widgets . '}}', $token);
is($info->{i18n}, "api.shareables.invalidFolder", "create summaryConfig with a non-string folder fails");
$info = viewerPostToken("/api/shareable", '{"name":"dash long folder","type":"summaryConfig","data":{"folder":"' . ("f" x 65) . '",' . $widgets . '}}', $token);
is($info->{i18n}, "api.shareables.invalidFolder", "create summaryConfig with a 65 character folder fails");
$info = viewerPostToken("/api/shareable", '{"name":"dash ctrl folder","type":"summaryConfig","data":{"folder":"Ops\u0007","' . 'widgets":[{"id":"w1","viewMode":"stats"}]}}', $token);
is($info->{i18n}, "api.shareables.invalidFolder", "create summaryConfig with a control character in the folder fails");
$info = viewerPostToken("/api/shareable", '{"name":"dash long expression","type":"summaryConfig","data":{"expression":"' . ("x" x 5000) . '",' . $widgets . '}}', $token);
is($info->{i18n}, "api.shareables.expressionTooLong", "create summaryConfig with a 5000 character expression fails");
$info = viewerPostToken("/api/shareable", '{"name":"dash bad expression","type":"summaryConfig","data":{"expression":5,' . $widgets . '}}', $token);
is($info->{i18n}, "api.shareables.invalidExpression", "create summaryConfig with a non-string expression fails");
