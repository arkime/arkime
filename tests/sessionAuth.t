# Session-authenticated s2s bypass regression tests (#4108)
use Test::More tests => 25;
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

# remoteHunt is s2s only too, and had the same hole: it only checked that an
# x-arkime-auth header was present, never that it was valid. A session
# authenticated request skips the s2s passport strategy, so any logged in user
# with packetSearch could drive it with a junk header.
# The session user must hold packetSearch, otherwise the route's permission
# check rejects first and the s2s gate is never what is under test.
addUser("-n test5 hunttestuser hunttestuser hunttestuser --roles arkimeUser --packetSearch");
esGet("/_refresh");

my $huntLogin = $ArkimeTest::userAgent->post("http://$host:$port/api/login", { username => 'hunttestuser', password => 'hunttestuser' });
my ($huntCookie) = ($huntLogin->header('Set-Cookie') // '') =~ /^([^;]+)/;
ok (defined $huntCookie, "packetSearch user has a session cookie");

my $huntPath = "/api/hunt/test5/nosuchhunt/remote/nosuchsession";
my $huntUrl = "http://$host:$port$huntPath";

$response = $ArkimeTest::userAgent->get($huntUrl, 'Cookie' => $huntCookie, ':x-arkime-auth' => 'garbage');
is ($response->code, 401, "session + junk token rejected on remote hunt");

$response = $ArkimeTest::userAgent->get($huntUrl, 'Cookie' => $huntCookie, ':x-arkime-auth' => '{"path": "/", "user": "hunttestuser", "date": ' . time() * 1000 . '}');
is ($response->code, 401, "session + wrong path token rejected on remote hunt");

$response = $ArkimeTest::userAgent->get($huntUrl, 'Cookie' => $huntCookie, ':x-arkime-auth' => '{"path": "' . $huntPath . '", "user": "hunttestuser", "date": 1}');
is ($response->code, 401, "session + stale token rejected on remote hunt");

$response = $ArkimeTest::userAgent->get($huntUrl, 'Cookie' => $huntCookie);
is ($response->code, 401, "session + missing token rejected on remote hunt");

# ...and a genuine, correctly scoped s2s token must still reach the handler
$response = $ArkimeTest::userAgent->get($huntUrl, ':x-arkime-auth' => '{"path": "' . $huntPath . '", "user": "hunttestuser", "date": ' . time() * 1000 . '}');
is ($response->code, 200, "valid s2s token reaches remote hunt handler");
$rjson = from_json($response->content);
is ($rjson->{matched}, 0, "remote hunt handler ran for a hunt that does not exist");

# Open redirect: a protocol-relative ogurl must not be honored on login (only same-origin paths).
# Seed session.ogurl by hitting an unauthenticated protocol-relative path, then log in with that session.
# Use a non-redirect-following agent so we capture the 302 + Set-Cookie instead of following to /auth.
my $ua = LWP::UserAgent->new;
$ua->max_redirect(0);
my $seed = $ua->get("http://$host:$port//evil.example.com");
my ($seedCookie) = ($seed->header('Set-Cookie') // '') =~ /^(ARKIME-SID=[^;]+)/;
ok (defined $seedCookie, "unauthenticated request seeds a session");

my $redir = $ua->post("http://$host:$port/api/login", { username => 'formtestuser', password => 'formtestuser' }, 'Cookie' => $seedCookie);
is ($redir->code, 302, "login with seeded ogurl redirects");
my $loc = $redir->header('Location') // '';
isnt ($loc, '//evil.example.com', "login must not honor a protocol-relative (off-site) ogurl");
is ($loc, '/', "login falls back to a safe same-origin path");
