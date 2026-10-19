use Test::More tests => 59;
use ArkimeTest;
use JSON;
use strict;

# Auth settings that would leave authentication fail open must stop the process
# at startup, and config saves must not be able to inject ini structure. Driven
# with a small node runner, so this needs no cluster.

my $runner = "/tmp/arkime-auth-config-$$.js";
my $ini = "/tmp/arkime-auth-config-$$.ini";
my $db = "/tmp/arkime-auth-config-$$.sqlite";

END { unlink $runner if $runner; unlink $ini if $ini; unlink glob("$db*") if $db; }

open(my $rf, '>', $runner) or die "can't write $runner";
print $rf <<'JS';
const path = require('path');
const common = path.resolve(process.argv[2]);
const ArkimeConfig = require(path.join(common, 'arkimeConfig'));
const ArkimeUtil = require(path.join(common, 'arkimeUtil'));
const Auth = require(path.join(common, 'auth'));

const iniFile = process.argv[3];
const mode = process.argv[4];

ArkimeConfig.initialize({ defaultConfigFile: iniFile, defaultSections: ['default'] }).then(async () => {
  if (mode === 'save') {
    // What a WISE /config/save body can hold, including a non string value
    const config = JSON.parse(process.argv[5]);
    ArkimeConfig.replace(config);
    ArkimeConfig.save((err) => {
      console.log('SAVE', err ?? 'OK');
      console.log('PARSED', JSON.stringify(ArkimeUtil.parseIniSync(iniFile)));
      process.exit(0);
    });
    return;
  }

  const User = require(path.join(common, 'user'));
  User.initialize({ url: ArkimeConfig.get('usersElasticsearch'), noUsersCheck: true });

  // Same option viewer/config.js passes
  await Auth.initialize({ appAdminRole: 'arkimeAdmin', s2sRegressionTests: ArkimeConfig.getBool('s2sRegressionTests', false) });
  console.log('AUTH OK');

  if (mode === 'login') {
    await User.setUser('admin', { userId: 'admin', userName: 'admin', enabled: true, webEnabled: true, roles: ['arkimeUser'], passStore: Auth.pass2store('admin', 'secret') });
    const express = require(require.resolve('express', { paths: [common] }));
    const app = express();
    Auth.app(app);
    const srv = app.listen(0, '127.0.0.1', async () => {
      const base = `http://127.0.0.1:${srv.address().port}/api/login`;
      const headers = { 'content-type': 'application/x-www-form-urlencoded' };
      let r = await fetch(`${base}?username=admin&password=secret`, { method: 'POST', headers, body: '', redirect: 'manual' });
      console.log('QUERY', r.status);
      r = await fetch(base, { method: 'POST', headers, body: 'username=admin&password=secret', redirect: 'manual' });
      console.log('BODY', r.status);
      // the form strategy runs on every unauthenticated request, not just /api/login
      r = await fetch(`http://127.0.0.1:${srv.address().port}/api/user?username=admin&password=secret`, { redirect: 'manual' });
      console.log('OTHER', r.status, (await r.text()).replace(/\n/g, ' '));
      process.exit(0);
    });
    return;
  }

  if (mode === 'mcphdr') {
    // The MCP header verifier, with alice not an admin and bob an admin from his last login
    await User.setUser('alice', { userId: 'alice', userName: 'alice', enabled: true, webEnabled: true, headerAuthEnabled: true, roles: ['arkimeUser'], passStore: Auth.pass2store('alice', 'x') });
    await User.setUser('bob', { userId: 'bob', userName: 'bob', enabled: true, webEnabled: true, headerAuthEnabled: true, roles: ['arkimeUser', 'arkimeAdmin'], passStore: Auth.pass2store('bob', 'x') });
    const roles = (userId) => new Promise(resolve => User.getUser(userId, (err, user) => resolve(user ? [...user.roles].sort().join(',') : 'NONE')));
    for (const [userId, headers] of [['alice', { groups: 'admins' }], ['bob', {}], ['carol', {}]]) {
      try {
        await Auth.headerAuth({ headers: { 'x-user': userId, ...headers } });
        console.log('MCP', userId, 'OK', await roles(userId));
      } catch (e) {
        console.log('MCP', userId, 'ERR', await roles(userId));
      }
    }
    process.exit(0);
  }

  if (mode === 'ipgate') {
    await User.setUser('hdruser', { userId: 'hdruser', userName: 'hdruser', enabled: true, webEnabled: true, headerAuthEnabled: true, roles: ['arkimeUser'], passStore: Auth.pass2store('hdruser', 'secret') });
    const express = require(require.resolve('express', { paths: [common] }));
    const app = express();
    // Pretend the peer is whatever x-test-peer says, so we can test a remote client
    app.use((req, res, next) => {
      Object.defineProperty(req.socket, 'remoteAddress', { value: req.headers['x-test-peer'] ?? '127.0.0.1', configurable: true });
      next();
    });
    Auth.app(app);
    app.get('/whoami', (req, res) => res.send(req.user.userId));
    const srv = app.listen(0, '127.0.0.1', async () => {
      const url = `http://127.0.0.1:${srv.address().port}/whoami`;
      let r = await fetch(url, { headers: { 'x-user': 'hdruser', 'x-test-peer': '10.1.2.3' } });
      console.log('REMOTE', r.status, await r.text());
      r = await fetch(url, { headers: { 'x-user': 'hdruser' } });
      console.log('LOCAL', r.status, await r.text());
      process.exit(0);
    });
    return;
  }

  if (mode === 'basic') {
    await User.setUser('admin', { userId: 'admin', userName: 'admin', enabled: true, webEnabled: true, roles: ['arkimeUser'], passStore: Auth.pass2store('admin', 'secret') });
    const express = require(require.resolve('express', { paths: [common] }));
    const app = express();
    Auth.app(app);
    app.get('/whoami', (req, res) => res.send(req.user.userId));
    const srv = app.listen(0, '127.0.0.1', async () => {
      const url = `http://127.0.0.1:${srv.address().port}/whoami`;
      const b64 = (s) => Buffer.from(s).toString('base64');
      const cases = [
        ['NONE', undefined],
        ['SCHEME', 'Bearer abc'],
        ['NOSPACE', 'Basic'],
        ['NOCOLON', 'Basic ' + b64('admin')],
        ['NOUSER', 'Basic ' + b64(':secret')],
        ['BADPASS', 'Basic ' + b64('admin:wrong')],
        ['NOUSERDB', 'Basic ' + b64('nobody:secret')],
        ['GOOD', 'basic ' + b64('admin:secret')]
      ];
      for (const [label, authorization] of cases) {
        const r = await fetch(url, { headers: authorization ? { authorization } : {} });
        console.log('BASIC', label, r.status, r.headers.get('www-authenticate') ?? '-', (await r.text()).replace(/\n/g, ' '));
      }
      process.exit(0);
    });
    return;
  }

  process.exit(0);
});
JS
close($rf);

# Returns (exitCode, output) for a [default] section body
sub tryConfig {
    my ($body, @args) = @_;
    unlink glob("$db*");
    open(my $fh, '>', $ini) or die "can't write $ini";
    print $fh "[default]\npasswordSecret=testsecret\nusersElasticsearch=sqlite://$db\n$body\n";
    close($fh);
    my %clean = map { ($_ => $ENV{$_}) } grep { !/^ARKIME/ } keys %ENV;
    my $args = join(' ', map { "'$_'" } @args);
    my $out = do {
        local %ENV = %clean;
        `node $runner ../common $ini $args 2>&1`;
    };
    return ($? >> 8, $out);
}

my ($code, $out);

################################################################################
# baseline
################################################################################
($code, $out) = tryConfig("");
is($code, 0, "default config starts");
like($out, qr/AUTH OK/, "and auth initializes");

################################################################################
# mcpAuthMode defaults to header, which needs userNameHeader even when authMode isn't a header mode
################################################################################
($code, $out) = tryConfig("mcpEnabled=true");
is($code, 1, "mcpEnabled with the default header mcpAuthMode and no userNameHeader refuses to start");
like($out, qr/userNameHeader missing .* mcpAuthMode=header/, "and says why");

($code, $out) = tryConfig("mcpEnabled=true\nmcpAuthMode=jwt");
is($code, 0, "mcpAuthMode=jwt without userNameHeader starts");

($code, $out) = tryConfig("mcpEnabled=true\nauthMode=header\nuserNameHeader=x-user");
is($code, 0, "mcpAuthMode=header with userNameHeader starts");

($code, $out) = tryConfig("mcpEnabled=false");
is($code, 0, "mcp disabled without userNameHeader starts");

################################################################################
# requiredAuthHeader without values would skip the check entirely
################################################################################
($code, $out) = tryConfig("authMode=header\nuserNameHeader=x-user\nrequiredAuthHeader=x-group");
is($code, 1, "requiredAuthHeader without requiredAuthHeaderVal refuses to start");
like($out, qr/requiredAuthHeaderVal is missing or empty/, "and says why");

($code, $out) = tryConfig("authMode=header\nuserNameHeader=x-user\nrequiredAuthHeader=x-group\nrequiredAuthHeaderVal=");
is($code, 1, "an empty requiredAuthHeaderVal refuses to start");

($code, $out) = tryConfig("authMode=header\nuserNameHeader=x-user\nrequiredAuthHeader=x-group\nrequiredAuthHeaderVal=ok");
is($code, 0, "requiredAuthHeader with a value starts");

($code, $out) = tryConfig("authMode=header\nuserNameHeader=x-user\nrequiredAuthHeaderVal=ok");
is($code, 1, "requiredAuthHeaderVal without requiredAuthHeader refuses to start");
like($out, qr/requiredAuthHeaderVal is set but requiredAuthHeader is missing/, "and says why");

################################################################################
# s2sRegressionTests accepts plaintext s2s tokens, config alone must not turn it on
################################################################################
($code, $out) = tryConfig("s2sRegressionTests=true");
is($code, 1, "s2sRegressionTests from config alone refuses to start");
like($out, qr/s2sRegressionTests requires --regressionTests/, "and says why");

($code, $out) = tryConfig("s2sRegressionTests=true", "", "--regressionTests");
is($code, 0, "s2sRegressionTests with --regressionTests starts");

################################################################################
# authMode=regressionTests lets any caller pick their user, config alone must not turn it on
################################################################################
($code, $out) = tryConfig("authMode=regressionTests");
is($code, 1, "authMode=regressionTests from config alone refuses to start");
like($out, qr/authMode=regressionTests requires --regressionTests/, "and says why");

($code, $out) = tryConfig("authMode=regressionTests", "", "--regressionTests");
is($code, 0, "authMode=regressionTests with --regressionTests starts");

################################################################################
# credentials in the query string are refused, they end up in access and proxy logs
################################################################################
($code, $out) = tryConfig("authMode=form", "login");
is($code, 0, "form mode starts");
like($out, qr/QUERY 403/, "login with credentials in the query string is refused");
like($out, qr/BODY 302/, "login with credentials in the body works");
like($out, qr/OTHER 403 .*POST body/, "credentials in the query string of any other url are refused");

################################################################################
# basic+header modes get the same loopback userAuthIps default as other header modes
################################################################################
foreach my $mode ("header", "header+basic", "basic+header", "basic+headerOnly") {
    ($code, $out) = tryConfig("authMode=$mode\nuserNameHeader=x-user", "ipgate");
    like($out, qr/^REMOTE 403 /m, "$mode refuses the user header from a remote peer by default");
    like($out, qr/^LOCAL 200 hdruser$/m, "$mode accepts the user header from loopback");
}

($code, $out) = tryConfig("authMode=basic+header\nuserNameHeader=x-user\nuserAuthIps=10.0.0.0/8", "ipgate");
like($out, qr/^REMOTE 200 hdruser$/m, "basic+header with userAuthIps set accepts the user header from that range");

################################################################################
# basic auth header parsing, the challenge is what makes browsers prompt
################################################################################
($code, $out) = tryConfig("authMode=basic", "basic");
is($code, 0, "basic mode starts");
like($out, qr/^BASIC NONE 401 Basic realm="Moloch" Unauthorized$/m, "no Authorization gets the Basic challenge");
like($out, qr/^BASIC SCHEME 401 Basic realm="Moloch" /m, "another scheme gets the Basic challenge");
like($out, qr/^BASIC NOSPACE 400 - /m, "a scheme without credentials is a bad request");
like($out, qr/^BASIC NOCOLON 400 - /m, "credentials without a colon are a bad request");
like($out, qr/^BASIC NOUSER 401 Basic realm="Moloch" /m, "an empty user gets the challenge");
like($out, qr/^BASIC BADPASS 401 Basic realm="Moloch" /m, "a wrong password gets the challenge");
like($out, qr/^BASIC NOUSERDB 401 Basic realm="Moloch" /m, "an unknown user gets the challenge");
like($out, qr/^BASIC GOOD 200 - admin$/m, "good credentials with a lowercase scheme authenticate");

################################################################################
# MCP header auth on a non header site only identifies the user, other headers aren't claims
################################################################################
my $mappings = "[user-role-mappings]\narkimeUser=true\narkimeAdmin=vals.groups === 'admins'\n[user-auto-create]\nuserId=vals['x-user']\nenabled=true\nheaderAuthEnabled=true";
($code, $out) = tryConfig("authMode=form\nmcpEnabled=true\nuserNameHeader=x-user\n$mappings", "mcphdr");
like($out, qr/^MCP alice OK arkimeUser$/m, "form site: an MCP header can't map the user to more roles");
like($out, qr/^MCP bob OK arkimeAdmin,arkimeUser$/m, "form site: MCP keeps the roles from the last login");
like($out, qr/^MCP carol ERR NONE$/m, "form site: MCP header doesn't auto create users");

($code, $out) = tryConfig("authMode=header\nmcpEnabled=true\nuserNameHeader=x-user\n$mappings", "mcphdr");
like($out, qr/^MCP alice OK arkimeAdmin,arkimeUser$/m, "header site: the proxy's headers still map roles");
like($out, qr/^MCP bob OK arkimeUser$/m, "header site: roles still follow the headers");
like($out, qr/^MCP carol OK arkimeUser$/m, "header site: header auth still auto creates users");

################################################################################
# config save can't inject ini lines, sections or keys
################################################################################
my $parsed;

($code, $out) = tryConfig("", "save", to_json({default => {a => ["x\n[user-auto-create]\ny=1"], n => 5, s => "l1\nl2"}}));
like($out, qr/SAVE OK/, "save with an array holding a newline works");
($parsed) = $out =~ /PARSED (.*)/;
$parsed = from_json($parsed);
is_deeply([sort keys %{$parsed}], ["default"], "no section injected by an array value");
ok(!exists $parsed->{default}->{y}, "no key injected by an array value");
is($parsed->{default}->{a}, "x\\n[user-auto-create]\\ny=1", "newlines in an array value are escaped");
is($parsed->{default}->{n}, "5", "numbers are saved");

($code, $out) = tryConfig("", "save", to_json({"file:a\n[user-auto-create]" => {k => "v"}}));
like($out, qr/SAVE Invalid section name/, "a section name with a newline is refused");

($code, $out) = tryConfig("", "save", to_json({default => {"a=b" => "v"}}));
like($out, qr/SAVE Invalid key name/, "a key with = is refused");

($code, $out) = tryConfig("", "save", to_json({default => {"[x]" => "v"}}));
like($out, qr/SAVE Invalid key name/, "a key starting with [ is refused");

($code, $out) = tryConfig("", "save", to_json({default => {"#x" => "v"}}));
like($out, qr/SAVE Invalid key name/, "a key starting with # is refused");

($code, $out) = tryConfig("", "save", to_json({default => {" ;x" => "v"}}));
like($out, qr/SAVE Invalid key name/, "a key starting with whitespace and ; is refused");

($code, $out) = tryConfig("", "save", to_json({default => {k => "x ; y #z"}}));
($parsed) = $out =~ /PARSED (.*)/;
is(from_json($parsed)->{default}->{k}, "x ; y #z", "; and # inside a value survive a save");

($code, $out) = tryConfig("", "save", to_json({"file:ok]x" => {file => "/tmp/f"}}));
like($out, qr/SAVE OK/, "a ] inside a section name is still allowed");
