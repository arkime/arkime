# Session-authenticated s2s bypass regression tests (#4108)
use Test::More tests => 14;
use ArkimeTest;
use JSON;
use strict;

my $host = $ArkimeTest::host;
my $port = 8128;

addUser("-n test5 formtestuser formtestuser formtestuser --roles arkimeUser");
esGet("/_refresh");

# Log in via form auth to get a real session cookie
my $loginResponse = $ArkimeTest::userAgent->post("http://$host:$port/api/login", { username => 'formtestuser', password => 'formtestuser' });
is ($loginResponse->code, 302, "form login redirects on success");
my $setCookie = $loginResponse->header('Set-Cookie');
ok (defined $setCookie && $setCookie =~ /^ARKIME-SID=/, "form login sets a session cookie");
my ($cookie) = $setCookie =~ /^([^;]+)/;

# Cookie should authenticate normal routes
my $response = $ArkimeTest::userAgent->get("http://$host:$port/", 'Cookie' => $cookie);
is ($response->code, 200, "session cookie authenticates normal routes");

# Session + token for a different path must be rejected, not just decrypt-checked
$response = $ArkimeTest::userAgent->get("http://$host:$port/receiveSession", 'Cookie' => $cookie, ':x-arkime-auth' => '{"path": "/", "user": "authtest2", "date": ' . time() * 1000 . '}');
is ($response->content, "receive session only allowed s2s", "session + bad url token rejected");
is ($response->code, 401, "session + bad url token rejected code");

# Session + stale token must be rejected
$response = $ArkimeTest::userAgent->get("http://$host:$port/receiveSession", 'Cookie' => $cookie, ':x-arkime-auth' => '{"path": "/receiveSession", "user": "authtest2", "date": 1}');
is ($response->content, "receive session only allowed s2s", "session + stale token rejected");
is ($response->code, 401, "session + stale token rejected code");

# Session + no token must be rejected
$response = $ArkimeTest::userAgent->get("http://$host:$port/receiveSession", 'Cookie' => $cookie);
is ($response->content, "receive session only allowed s2s", "session + missing token rejected");
is ($response->code, 401, "session + missing token rejected code");

# Session + a genuinely valid, current, correctly-scoped token still works
$response = $ArkimeTest::userAgent->post("http://$host:$port/receiveSession", 'Cookie' => $cookie, ':x-arkime-auth' => '{"path": "/receiveSession", "user": "authtest2", "date": ' . time() * 1000 . '}');
my $rjson = from_json($response->content);
is ($rjson->{success}, 0, "session + valid token reaches receiveSession handler");
is ($rjson->{i18n}, "api.sessions.missingSaveId", "session + valid token reaches receiveSession handler i18n");
is ($response->code, 200, "session + valid token reaches receiveSession handler code");

# A genuine s2s call with no session cookie must still work (passport used to crash
# session-serializing the s2s strategy's userId-less placeholder user)
$response = $ArkimeTest::userAgent->post("http://$host:$port/receiveSession", ':x-arkime-auth' => '{"path": "/receiveSession", "user": "authtest2", "date": ' . time() * 1000 . '}');
$rjson = from_json($response->content);
is ($rjson->{success}, 0, "no-session s2s call reaches receiveSession handler");
is ($response->code, 200, "no-session s2s call reaches receiveSession handler code");
