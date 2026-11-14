use Test::More tests => 117;
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

// oidc mode needs an issuer to discover before Auth.initialize, and never gets past the redirect to it
const idpReady = mode !== 'oidc' ? Promise.resolve() : new Promise(resolve => {
  const issuer = `http://127.0.0.1:${process.argv[5]}`;
  require('http').createServer((req, res) => {
    res.setHeader('content-type', 'application/json');
    res.end(JSON.stringify({ issuer, authorization_endpoint: `${issuer}/authorize`, token_endpoint: `${issuer}/token`, jwks_uri: `${issuer}/jwks`, response_types_supported: ['code'], subject_types_supported: ['public'], id_token_signing_alg_values_supported: ['RS256'] }));
  }).listen(process.argv[5], '127.0.0.1', resolve);
});

idpReady.then(() => ArkimeConfig.initialize({ defaultConfigFile: iniFile, defaultSections: ['default'] })).then(async () => {
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
  console.log('AUTH OK', Auth.mode);

  if (mode === 'login') {
    await User.setUser('admin', { userId: 'admin', userName: 'admin', enabled: true, webEnabled: true, roles: ['arkimeUser'], passStore: Auth.pass2store('admin', 'secret') });
    const express = require(require.resolve('express', { paths: [common] }));
    const app = express();
    app.use(express.json()); // the viewer parses json bodies for every route
    Auth.app(app);
    const srv = app.listen(0, '127.0.0.1', async () => {
      const base = `http://127.0.0.1:${srv.address().port}/api/login`;
      const headers = { 'content-type': 'application/x-www-form-urlencoded' };
      let r = await fetch(`${base}?username=admin&password=secret`, { method: 'POST', headers, body: '', redirect: 'manual' });
      console.log('QUERY', r.status);
      r = await fetch(base, { method: 'POST', headers, body: 'username=admin&password=secret', redirect: 'manual' });
      console.log('BODY', r.status);
      for (const site of ['cross-site', 'same-site', 'same-origin', 'none']) {
        r = await fetch(base, { method: 'POST', headers: { ...headers, 'sec-fetch-site': site }, body: 'username=admin&password=secret', redirect: 'manual' });
        console.log('FETCHSITE', site, r.status, r.status === 302 ? '-' : (await r.text()).replace(/\n/g, ' '));
      }
      // the form strategy runs on every unauthenticated request, not just /api/login
      r = await fetch(`http://127.0.0.1:${srv.address().port}/api/user?username=admin&password=secret`, { redirect: 'manual' });
      console.log('OTHER', r.status, (await r.text()).replace(/\n/g, ' '));
      // Buffer.from walks anything with a length, this used to block for seconds
      const start = Date.now();
      r = await fetch(base, { method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify({ username: 'admin', password: 'secret', ogurl: { length: 2e8 } }), redirect: 'manual' });
      console.log('OGURLOBJ', r.status, r.headers.get('location'), Date.now() - start < 1000 ? 'FAST' : 'SLOW');
      process.exit(0);
    });
    return;
  }

  if (mode === 'mcphdr') {
    // The MCP header verifier, with alice not an admin and bob an admin from his last login
    await User.setUser('alice', { userId: 'alice', userName: 'alice', enabled: true, webEnabled: true, headerAuthEnabled: true, roles: ['arkimeUser'], passStore: Auth.pass2store('alice', 'x') });
    await User.setUser('bob', { userId: 'bob', userName: 'bob', enabled: true, webEnabled: true, headerAuthEnabled: true, roles: ['arkimeUser', 'arkimeAdmin'], passStore: Auth.pass2store('bob', 'x') });
    const roles = (userId) => new Promise(resolve => User.getUser(userId, (err, user) => resolve(user ? [...user.roles].sort().join(',') : 'NONE')));
    for (const [userId, headers] of [['alice', { groups: 'admins' }], ['bob', {}], ['carol', {}], ['..', {}], ['x/y', {}], ['dom\\user', {}]]) {
      try {
        await Auth.headerAuth({ headers: { 'x-user': userId, ...headers } });
        console.log('MCP', userId, 'OK', await roles(userId));
      } catch (e) {
        console.log('MCP', userId, 'ERR', await roles(userId));
      }
    }
    process.exit(0);
  }

  if (mode === 'jwtscope') {
    // A JWKS to verify against, and one token without the authJwtRequiredScopes scope
    const jose = require(require.resolve('jose', { paths: [common] }));
    const { publicKey, privateKey } = await jose.generateKeyPair('RS256');
    const jwk = { ...await jose.exportJWK(publicKey), kid: 'k1', alg: 'RS256', use: 'sig' };
    await new Promise(resolve => require('http').createServer((req, res) => {
      res.setHeader('content-type', 'application/json');
      res.end(JSON.stringify({ keys: [jwk] }));
    }).listen(process.argv[5], '127.0.0.1', resolve));
    await User.setUser('jwtuser', { userId: 'jwtuser', userName: 'jwtuser', enabled: true, webEnabled: true, headerAuthEnabled: true, roles: ['arkimeUser'], passStore: Auth.pass2store('jwtuser', 'x') });
    const sign = (claims) => new jose.SignJWT(claims).setProtectedHeader({ alg: 'RS256', kid: 'k1' }).setIssuer('iss').setAudience('aud').setIssuedAt().setExpirationTime('5m').sign(privateKey);
    const noScope = await sign({ sub: 'jwtuser' });
    try {
      const user = await Auth.headerAuth({ headers: { 'x-jwt': noScope } });
      console.log('JWTSCOPE header', user?.userId ?? 'NONE');
    } catch (e) {
      console.log('JWTSCOPE header ERR', e.message);
    }
    for (const [label, token] of [['bearer', noScope], ['bearerscope', await sign({ sub: 'jwtuser', scope: 'mcp:use' })]]) {
      try {
        await Auth.verifyJwt(token);
        console.log('JWTSCOPE', label, 'OK');
      } catch (e) {
        console.log('JWTSCOPE', label, 'ERR', e.message);
      }
    }
    process.exit(0);
  }

  if (mode === 'mcp') {
    const MCPServer = require(path.join(common, 'mcpServer'));
    await User.setUser('alice', { userId: 'alice', userName: 'alice', enabled: true, webEnabled: true, headerAuthEnabled: true, roles: ['arkimeUser', 'mcpUser'], passStore: Auth.pass2store('alice', 'x') });
    const express = require(require.resolve('express', { paths: [common] }));
    const app = express();
    // Pretend the peer is whatever x-test-peer says
    app.use((req, res, next) => {
      Object.defineProperty(req.socket, 'remoteAddress', { value: req.headers['x-test-peer'] ?? '127.0.0.1', configurable: true });
      next();
    });
    app.use('/mcp', MCPServer.router({ serviceName: 'test', serviceRole: 'arkimeUser', tools: [], enabled: () => true }));
    const srv = app.listen(0, '127.0.0.1', async () => {
      const url = `http://127.0.0.1:${srv.address().port}/mcp`;
      const body = JSON.stringify({ jsonrpc: '2.0', id: 1, method: 'ping' });
      for (const [label, headers] of [['LOCAL', {}], ['REMOTE', { 'x-test-peer': '10.1.2.3' }], ['BEARER', { authorization: 'Bearer not.a.jwt' }]]) {
        const r = await fetch(url, { method: 'POST', headers: { 'content-type': 'application/json', 'x-user': 'alice', ...headers }, body });
        console.log('MCPIP', label, r.status);
      }
      process.exit(0);
    });
    return;
  }

  if (mode === 'oidc') {
    const express = require(require.resolve('express', { paths: [common] }));
    const app = express();
    Auth.app(app);
    const srv = app.listen(0, '127.0.0.1', async () => {
      for (const host of ['b.example', 'evil.example']) {
        const r = await new Promise(resolve => require('http').get({ host: '127.0.0.1', port: srv.address().port, path: '/', headers: { host } }, resolve));
        console.log('OIDC', host, r.statusCode, new URL(r.headers.location).searchParams.get('redirect_uri'));
      }
      process.exit(0);
    });
    return;
  }

  if (mode === 'mac') {
    const crypto = require('crypto');
    const pbkdf2Sync = crypto.pbkdf2Sync;
    let derives = 0;
    crypto.pbkdf2Sync = (...args) => { derives++; return pbkdf2Sync(...args); };
    const tryAuth = (label, token, secret, options) => {
      try {
        console.log('MAC', label, 'OK', JSON.stringify(Auth.auth2obj(token, secret, options)));
      } catch (e) {
        console.log('MAC', label, 'ERR');
      }
    };
    const flip = (hex) => (hex[0] === '0' ? '1' : '0') + hex.slice(1);

    const parsed = JSON.parse(Auth.obj2auth({ n: 1 }));
    const { mac, ...nomac } = parsed;
    console.log('MAC HAS', /^[0-9a-f]{64}$/.test(mac));
    tryAuth('GOOD', JSON.stringify(parsed));
    tryAuth('NOMAC', JSON.stringify(nomac));
    tryAuth('STORED', JSON.stringify(nomac), undefined, { stored: true });
    tryAuth('BADMAC', JSON.stringify({ ...parsed, mac: flip(mac) }));
    tryAuth('TAMPER', JSON.stringify({ ...parsed, data: flip(parsed.data) }));
    tryAuth('OTHER', Auth.obj2auth({ n: 2 }, 'otherSecret'), 'otherSecret');
    tryAuth('WRONGSECRET', Auth.obj2auth({ n: 3 }, 'otherSecret'));
    const big = Auth.obj2auth({ s: 'x'.repeat(5000) });
    tryAuth('BIG', big);
    tryAuth('BIGSTORED', big, undefined, { stored: true });

    // The IV.E.H format obj2auth wrote before 6.4.0
    const iveh = (obj, secret) => {
      const key = crypto.createHash('sha256').update(secret).digest();
      const iv = crypto.randomBytes(16);
      const c = crypto.createCipheriv('aes-256-cbc', key, iv);
      const e = iv.toString('hex') + '.' + c.update(JSON.stringify(obj), 'utf8', 'hex') + c.final('hex');
      return e + '.' + crypto.createHmac('sha256', key).update(e).digest('hex');
    };
    tryAuth('IVEH', iveh({ n: 4 }, 'testsecret'));
    tryAuth('IVEHSTORED', iveh({ n: 5 }, 'otherSecret'), 'otherSecret', { stored: true });

    // Each forged token has a fresh salt, which used to mean a fresh PBKDF2
    const forge = (withMac) => {
      derives = 0;
      for (let i = 0; i < 5; i++) {
        const forged = { ...nomac, salt: crypto.randomBytes(16).toString('hex') };
        if (withMac) { forged.mac = crypto.randomBytes(32).toString('hex'); }
        try { Auth.auth2obj(JSON.stringify(forged)); } catch (e) {}
      }
      return derives;
    };
    console.log('MAC FORGED', forge(true));
    console.log('MAC FORGEDNOMAC', forge(false));
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
like($out, qr/AUTH OK digest/, "and auth initializes in digest mode");

################################################################################
# userNameHeader no longer selects the auth mode, authMode defaults to digest
################################################################################
($code, $out) = tryConfig("userNameHeader=x-user", "ipgate");
is($code, 0, "userNameHeader without authMode starts");
like($out, qr/AUTH OK digest/, "and uses digest, not header");
like($out, qr/userNameHeader alone no longer enables header auth/, "and says why");
like($out, qr/^LOCAL 401 /m, "and doesn't trust the user header from loopback");

($code, $out) = tryConfig("userNameHeader=oidc");
is($code, 0, "an auth mode in userNameHeader starts");
like($out, qr/AUTH OK digest/, "and uses digest, not that mode");
like($out, qr/Ignoring userNameHeader=oidc/, "and says why");

($code, $out) = tryConfig("mcpEnabled=true\nuserNameHeader=digest");
is($code, 1, "an auth mode in userNameHeader isn't a header for the default header mcpAuthMode");
like($out, qr/userNameHeader missing .* mcpAuthMode=header/, "and says why");

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
# authHeaderJwtVerify needs somewhere to get keys from
################################################################################
($code, $out) = tryConfig("authMode=header-jwt\nuserNameHeader=x-jwt\nauthUserIdField=sub\nauthHeaderJwtVerify=true");
is($code, 1, "authHeaderJwtVerify without authJwtJwksUrl refuses to start");
like($out, qr/authJwtJwksUrl missing/, "and says why");

($code, $out) = tryConfig("authMode=header-jwt\nuserNameHeader=x-jwt\nauthUserIdField=sub\nauthHeaderJwtVerify=true\nauthJwtJwksUrl=https://127.0.0.1/jwks\nauthJwtIssuer=iss");
is($code, 1, "authHeaderJwtVerify without authJwtAudience refuses to start");

($code, $out) = tryConfig("authMode=header-jwt\nuserNameHeader=x-jwt\nauthUserIdField=sub\nauthHeaderJwtVerify=true\nauthJwtJwksUrl=https://127.0.0.1/jwks\nauthJwtIssuer=iss\nauthJwtAudience=aud");
is($code, 0, "authHeaderJwtVerify with jwks, issuer and audience starts");

################################################################################
# mcpAllowedIps is who may send the MCP username header, a jwt is checked on its own
################################################################################
my $mcp = "mcpEnabled=true\nuserNameHeader=x-user";
($code, $out) = tryConfig("authMode=form\n$mcp", "mcp");
like($out, qr/^MCPIP LOCAL 200$/m, "mcp header: trusted from loopback by default");
like($out, qr/^MCPIP REMOTE 401$/m, "mcp header: not from anywhere else");
like($out, qr/^MCPIP BEARER 200$/m, "mcp header only: a Bearer header passed through by the proxy doesn't matter");

($code, $out) = tryConfig("authMode=form\n$mcp\nmcpAllowedIps=10.0.0.0/8", "mcp");
like($out, qr/^MCPIP REMOTE 200$/m, "mcp header: trusted from mcpAllowedIps");
like($out, qr/^MCPIP LOCAL 401$/m, "mcp header: and only from there");

($code, $out) = tryConfig("authMode=header\n$mcp\nuserAuthIps=10.0.0.0/8", "mcp");
like($out, qr/^MCPIP REMOTE 200$/m, "mcp header on a header site: defaults to the proxies in userAuthIps");
like($out, qr/^MCPIP LOCAL 401$/m, "mcp header on a header site: and only those");

($code, $out) = tryConfig("authMode=form\n$mcp\nmcpAuthMode=jwt,header\nauthJwtJwksUrl=https://127.0.0.1:1/jwks\nauthJwtIssuer=iss\nauthJwtAudience=aud\nmcpAllowedIps=127.0.0.1/32", "mcp");
like($out, qr/WARNING - mcpAllowedIps only lists the hosts trusted to send the MCP username header/, "mcp jwt with mcpAllowedIps: warns what it now means");
like($out, qr/^MCPIP LOCAL 200$/m, "mcp jwt,header: the header still works without a Bearer");
like($out, qr/^MCPIP BEARER 401$/m, "mcp jwt,header: a Bearer that fails doesn't fall back to the header");

################################################################################
# oidc redirect_uri comes from authRedirectURIs when it is set, never from a forged Host
################################################################################
my $idpPort = 20000 + ($$ % 20000);
my $oidc = "authMode=oidc\nauthCookieSecure=false\nauthDiscoverURL=http://127.0.0.1:$idpPort/.well-known/openid-configuration\nauthClientId=arkime\nauthClientSecret=s\nauthUserIdField=sub";
($code, $out) = tryConfig("$oidc\nauthRedirectURIs=http://a.example/auth/login/callback,http://b.example/auth/login/callback", "oidc", $idpPort);
like($out, qr{^OIDC b.example 302 http://b.example/auth/login/callback$}m, "authRedirectURIs list: the entry for the request's host is used");
like($out, qr{^OIDC evil.example 302 http://a.example/auth/login/callback$}m, "authRedirectURIs list: an unknown Host gets the first entry");

($code, $out) = tryConfig("$oidc\nauthRedirectURIs=http://a.example/auth/login/callback", "oidc", $idpPort);
like($out, qr{^OIDC evil.example 302 http://a.example/auth/login/callback$}m, "one authRedirectURIs: always that one");

($code, $out) = tryConfig($oidc, "oidc", $idpPort);
like($out, qr{^OIDC evil.example 302 http://evil.example/auth/login/callback$}m, "no authRedirectURIs: still built from the Host, like before");

################################################################################
# authJwtRequiredScopes is for Bearer tokens, a proxy's verified header JWT doesn't need them
################################################################################
($code, $out) = tryConfig("authMode=header-jwt\nuserNameHeader=x-jwt\nauthUserIdField=sub\nauthHeaderJwtVerify=true\nauthJwtJwksUrl=http://127.0.0.1:$idpPort/jwks\nauthJwtIssuer=iss\nauthJwtAudience=aud\nauthJwtRequiredScopes=mcp:use", "jwtscope", $idpPort);
like($out, qr/^JWTSCOPE header jwtuser$/m, "header JWT without the required scope is accepted");
like($out, qr/^JWTSCOPE bearer ERR Token missing required scope\(s\) mcp:use$/m, "Bearer JWT without the required scope is refused");
like($out, qr/^JWTSCOPE bearerscope OK$/m, "Bearer JWT with the required scope is accepted");

################################################################################
# s2s tokens carry a mac that is checked before the PBKDF2
################################################################################
($code, $out) = tryConfig("", "mac");
like($out, qr/^MAC HAS true$/m, "new tokens have a mac");
like($out, qr/^MAC GOOD OK \{"n":1\}$/m, "a token with a good mac decrypts");
like($out, qr/^MAC NOMAC OK \{"n":1\}$/m, "a token from an older version without a mac still decrypts");
like($out, qr/^MAC STORED OK \{"n":1\}$/m, "a stored value without a mac decrypts");
like($out, qr/^MAC BADMAC ERR$/m, "a bad mac is refused");
like($out, qr/^MAC TAMPER ERR$/m, "the mac covers the data");
like($out, qr/^MAC OTHER OK \{"n":2\}$/m, "a token for another secret decrypts with it");
like($out, qr/^MAC WRONGSECRET ERR$/m, "but not with ours");
like($out, qr/^MAC BIG ERR$/m, "a token over the network size limit is refused");
like($out, qr/^MAC BIGSTORED OK \{"s":"x{5000}"\}$/m, "but a stored value that size decrypts");
like($out, qr/^MAC IVEH OK \{"n":4\}$/m, "a pre 6.4 IV.E.H token decrypts");
like($out, qr/^MAC IVEHSTORED OK \{"n":5\}$/m, "a pre 6.4 IV.E.H stored value decrypts");
like($out, qr/^MAC FORGED 0$/m, "forged tokens with a mac cost no PBKDF2");
like($out, qr/^MAC FORGEDNOMAC 5$/m, "forged tokens without a mac still do while v6Compatible");

($code, $out) = tryConfig("v6Compatible=false", "mac");
like($out, qr/^MAC GOOD OK \{"n":1\}$/m, "v6Compatible=false: a token with a mac decrypts");
like($out, qr/^MAC NOMAC ERR$/m, "v6Compatible=false: a token without a mac is refused");
like($out, qr/^MAC STORED OK \{"n":1\}$/m, "v6Compatible=false: stored values without a mac still decrypt");
like($out, qr/^MAC IVEH ERR$/m, "v6Compatible=false: a pre 6.4 IV.E.H token is refused");
like($out, qr/^MAC IVEHSTORED OK \{"n":5\}$/m, "v6Compatible=false: a pre 6.4 IV.E.H stored value still decrypts");
like($out, qr/^MAC FORGEDNOMAC 0$/m, "v6Compatible=false: forged tokens without a mac cost no PBKDF2");

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
like($out, qr/^FETCHSITE cross-site 403 .*Login must come from this site/m, "a login posted from another site is refused");
like($out, qr/^FETCHSITE same-site 403 /m, "and from a sibling site");
like($out, qr/^FETCHSITE same-origin 302 /m, "a login from our own page works");
like($out, qr/^FETCHSITE none 302 /m, "a login the user started themselves works");
like($out, qr/OTHER 403 .*POST body/, "credentials in the query string of any other url are refused");
like($out, qr/^OGURLOBJ 302 \/ FAST$/m, "an ogurl that isn't a string is ignored");

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
like($out, qr/^MCP \.\. ERR NONE$/m, "header site: a .. userId isn't auto created");
like($out, qr{^MCP x/y ERR NONE$}m, "header site: a userId with a / isn't auto created");
like($out, qr/^MCP dom\\user OK arkimeUser$/m, "header site: a DOMAIN\\user userId still is");

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
