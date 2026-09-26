use Test::More tests => 30;
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

################################################################################
# s2sRegressionTests accepts plaintext s2s tokens, config alone must not turn it on
################################################################################
($code, $out) = tryConfig("s2sRegressionTests=true");
is($code, 1, "s2sRegressionTests from config alone refuses to start");
like($out, qr/s2sRegressionTests requires --regressionTests/, "and says why");

($code, $out) = tryConfig("s2sRegressionTests=true", "", "--regressionTests");
is($code, 0, "s2sRegressionTests with --regressionTests starts");

################################################################################
# credentials in the query string are refused, they end up in access and proxy logs
################################################################################
($code, $out) = tryConfig("authMode=form", "login");
is($code, 0, "form mode starts");
like($out, qr/QUERY 403/, "login with credentials in the query string is refused");
like($out, qr/BODY 302/, "login with credentials in the body works");
like($out, qr/OTHER 403 .*POST body/, "credentials in the query string of any other url are refused");

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
