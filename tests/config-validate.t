use Test::More tests => 56;
use ArkimeTest;
use strict;

# ArkimeConfig validates settings that restrict access or bound a resource when
# the config loads, and refuses to start when one is bad. Driven at the config
# layer, so this needs no cluster and no generated common/version.js.

my $runner = "/tmp/arkime-config-validate-$$.js";
my $ini = "/tmp/arkime-config-validate-$$.ini";
my $yaml = "/tmp/arkime-config-validate-$$.yaml";

# Clean up even if a test dies part way through
END { unlink $runner if $runner; unlink $ini if $ini; unlink $yaml if $yaml; }

open(my $rf, '>', $runner) or die "can't write $runner";
print $rf <<'JS';
const path = require('path');
const common = path.resolve(process.argv[2]);
const ArkimeConfig = require(path.join(common, 'arkimeConfig'));
const ArkimeUtil = require(path.join(common, 'arkimeUtil'));

// The owners register their own settings at require time, exactly as the real
// apps do: viewer.js/cont3xt.js require mcpServer, and auth registers its own.
require(path.join(common, 'mcpServer'));
require(path.join(common, 'auth'));
// viewer/config.js registers these before calling initialize
ArkimeConfig.registerValidated({
  uploadFileSizeLimit: { type: 'int', min: 0 },
  maxSessionsQueried: { type: 'int', min: 0 },
  elasticsearch: { type: 'urls' },
  usersElasticsearch: { type: 'urls' }
});

const mode = process.argv[4];
const sections = ['testnode', 'default'];

if (mode === 'nodeclass') {
  // viewer/config.js splices the [nodeClass] section in from a loaded callback,
  // which runs after the config is parsed
  ArkimeConfig.loaded(() => {
    const nodeClass = ArkimeConfig.getFull(['testnode'], 'nodeClass');
    if (nodeClass && ArkimeConfig.getSection(nodeClass) !== undefined) { sections.splice(1, 0, nodeClass); }
  }, true);
}

ArkimeConfig.initialize({ defaultConfigFile: process.argv[3], defaultSections: sections }).then(() => {
  console.log('CONFIG OK');
  if (mode === 'trie') {
    // a valid allow list has to still match the peers it is supposed to
    const { trie } = ArkimeUtil.buildIpTrie(ArkimeConfig.getArray('mcpAllowedIps', ''));
    for (const ip of ['::ffff:127.0.0.1', '::ffff:10.1.2.3', '::ffff:8.8.8.8', '::1', '2001:db8::5']) {
      console.log(`MATCH ${ip} ${trie.find(ip) ? 'yes' : 'no'}`);
    }
  }
});
JS
close($rf);

# Returns (exitCode, output) for a [default] section body
sub tryConfig {
    my ($body, $mode) = @_;
    open(my $fh, '>', $ini) or die "can't write $ini";
    print $fh "[default]\n$body\n";
    close($fh);
    # Strip ARKIME_* so an exported override in the dev/CI environment cannot
    # leak into these cases - reload() merges every one of them into the config
    my %clean = map { ($_ => $ENV{$_}) } grep { !/^ARKIME/ } keys %ENV;
    my $out = do {
        local %ENV = %clean;
        `node $runner ../common $ini ${\ ($mode // '')} 2>&1`;
    };
    return ($? >> 8, $out);
}

# Same for a yaml file, $body is the default: mapping
sub tryYaml {
    my ($body) = @_;
    open(my $fh, '>', $yaml) or die "can't write $yaml";
    print $fh "default:\n$body\n";
    close($fh);
    my %clean = map { ($_ => $ENV{$_}) } grep { !/^ARKIME/ } keys %ENV;
    my $out = do {
        local %ENV = %clean;
        `node $runner ../common $yaml 2>&1`;
    };
    return ($? >> 8, $out);
}

my ($code, $out);

################################################################################
# a good config starts, and the accessors coerce
################################################################################
($code, $out) = tryConfig("mcpAllowedIps=127.0.0.0/8;10.0.0.0/8\nuserAuthIps=::1\nuploadFileSizeLimit=2147483648\nmaxSessionsQueried=2000000\nmcpMaxQueryDays=0.5\nauthJwtClockSkew=60");
is($code, 0, "a valid config starts");
like($out, qr/CONFIG OK/, "and reports it loaded");

($code, $out) = tryConfig("");
is($code, 0, "an empty config starts");

($code, $out) = tryConfig("mcpMaxQueryDays=-1");
is($code, 0, "mcpMaxQueryDays=-1, the no limit sentinel, is allowed");

# 0 is a legitimate way to disable uploads, it must not be a boot failure
($code, $out) = tryConfig("uploadFileSizeLimit=0");
is($code, 0, "uploadFileSizeLimit=0 starts");

# an empty value means unset, which is a common container idiom
($code, $out) = tryConfig("uploadFileSizeLimit=");
is($code, 0, "an empty numeric value is treated as unset");

################################################################################
# a valid allow list must still match the right peers. Both allow lists had
# their trie construction rewritten, so pin the behaviour and not just the
# exit codes.
################################################################################
($code, $out) = tryConfig("mcpAllowedIps=127.0.0.0/8;10.0.0.0/8;2001:db8::/32", 'trie');
is($code, 0, "the allow list config starts");
like($out, qr/MATCH ::ffff:127\.0\.0\.1 yes/, "127.0.0.1 matches 127.0.0.0/8");
like($out, qr/MATCH ::ffff:10\.1\.2\.3 yes/, "10.1.2.3 matches 10.0.0.0/8");
like($out, qr/MATCH ::ffff:8\.8\.8\.8 no/, "8.8.8.8 matches nothing");
like($out, qr/MATCH ::1 no/, "::1 is not matched by the v4 entries");
like($out, qr/MATCH 2001:db8::5 yes/, "2001:db8::5 matches 2001:db8::/32");

# An IPv4 mapped literal is folded to the v4 it names, with either prefix
# style, instead of becoming a v6 /8 that matches every peer
($code, $out) = tryConfig("mcpAllowedIps=::ffff:10.0.0.0/8;::ffff:127.0.0.0/104", 'trie');
is($code, 0, "mapped v4 literals start");
like($out, qr/MATCH ::ffff:10\.1\.2\.3 yes/, "::ffff:10.0.0.0/8 matches 10.1.2.3");
like($out, qr/MATCH ::ffff:127\.0\.0\.1 yes/, "::ffff:127.0.0.0/104 matches 127.0.0.1");
like($out, qr/MATCH ::ffff:8\.8\.8\.8 no/, "and 8.8.8.8 is still not matched");
like($out, qr/MATCH ::1 no/, "and neither is ::1");

################################################################################
# a broken allow list is a hard failure. Each of these used to be accepted and
# then match everything, or abort the process.
################################################################################
foreach my $bad ('10.0.0.0/abc', '10.0.0.0/', '10.0.0.0/33', '10/8', 'false', '999.1.1.1/8', '::ffff:10.0.0.0/40', '2001:db8::/129', '2001:zz::1', '1.2.3.4.5/32') {
    ($code, $out) = tryConfig("mcpAllowedIps=$bad");
    is($code, 1, "mcpAllowedIps=$bad refuses to start");
}

($code, $out) = tryConfig("mcpAllowedIps=10.0.0.0/abc");
like($out, qr/refusing to start/, "the failure says it is refusing to start");
like($out, qr/mcpAllowedIps/, "and names the setting");
like($out, qr/10\.0\.0\.0\/abc/, "and quotes what the operator wrote");

($code, $out) = tryConfig("userAuthIps=10.0.0.0/");
is($code, 1, "a bad userAuthIps refuses to start");
like($out, qr/userAuthIps/, "and names that setting");

# getArray used to drop any entry with a space in it, which made these look
# unset or, worse, like a shorter good list
# a yaml key with no value is null, which is unset, like ini key=
($code, $out) = tryYaml("  uploadFileSizeLimit:\n  userAuthIps:\n  mcpMaxQueryDays: 0.5");
is($code, 0, "yaml null values are unset and the config starts");
unlike($out, qr/null/, "and nothing complains about 'null'");

# a yaml list item with no value is null, the ini split would have made it ''
($code, $out) = tryYaml("  mcpAllowedIps:\n    - 127.0.0.0/8\n    -\n");
is($code, 0, "a yaml list with a blank item starts");
unlike($out, qr/unusable/, "and the blank item is not called unusable");

# .inf is a real number in yaml, and would pass a >= 0 bound and remove the cap
($code, $out) = tryYaml("  mcpMaxQueryDays: .inf");
is($code, 1, "an infinite mcpMaxQueryDays refuses to start");
like($out, qr/mcpMaxQueryDays/, "and names the setting");

# a url list entry that no client could connect to is named at load, rather
# than throwing from inside the es client with no mention of the setting
($code, $out) = tryConfig("elasticsearch=http://es1:9200;es2:9200");
is($code, 0, "a url list with and without a scheme starts");
($code, $out) = tryConfig("elasticsearch=http://es1:9200;http://es2 :9200");
is($code, 1, "a malformed elasticsearch url refuses to start");
like($out, qr/elasticsearch/, "and names the setting");
like($out, qr/'http:\/\/es2 :9200'/, "and quotes the bad entry");

($code, $out) = tryConfig("userAuthIps=10.0.0.0/8 10.0.0.1/32");
is($code, 1, "a space separated userAuthIps refuses to start");
like($out, qr/'10\.0\.0\.0\/8 10\.0\.0\.1\/32'/, "and quotes the whole thing as one entry");
($code, $out) = tryConfig("mcpAllowedIps=10.0.0.0/8;10.0.0.1 /32");
is($code, 1, "one entry with a space in a good list refuses to start");
like($out, qr/'10\.0\.0\.1 \/32'/, "and quotes that entry");

# a good entry does not excuse a bad one, a partial allow list is worse than none
($code, $out) = tryConfig("mcpAllowedIps=127.0.0.0/8;10.0.0.0/abc");
is($code, 1, "one bad entry fails the whole list");

################################################################################
# numeric bounds. parseInt stops at the first non digit, so these used to mean
# 2 bytes and NaN rather than what the operator wrote.
################################################################################
($code, $out) = tryConfig("uploadFileSizeLimit=2g");
is($code, 1, "uploadFileSizeLimit=2g refuses to start");
like($out, qr/uploadFileSizeLimit/, "and names the setting");

($code, $out) = tryConfig("maxSessionsQueried=2m");
is($code, 1, "maxSessionsQueried=2m refuses to start");

# a non numeric clock skew made jose compare exp against NaN, which is always
# false, so an expired token verified forever
($code, $out) = tryConfig("authJwtClockSkew=60s");
is($code, 1, "authJwtClockSkew=60s refuses to start");
like($out, qr/authJwtClockSkew/, "and names the setting");

################################################################################
# a [nodeClass] section is spliced into the section list from a loaded callback,
# so validation has to run after those or it reads a shorter list than every
# runtime consumer does
################################################################################
($code, $out) = tryConfig("nodeClass=myclass\n[myclass]\nmcpAllowedIps=10.0.0.0/33\n[testnode]\nnodeClass=myclass", 'nodeclass');
is($code, 1, "a bad setting in a [nodeClass] section refuses to start");
like($out, qr/mcpAllowedIps/, "and names it");

################################################################################
# every bad setting is reported in one pass, so the operator fixes them together
################################################################################
($code, $out) = tryConfig("mcpAllowedIps=10/8\nuploadFileSizeLimit=2g\nmaxSessionsQueried=abc\nmcpMaxQueryDays=sevem");
is($code, 1, "several bad settings refuse to start");
like($out, qr/4 bad settings/, "and all four are reported at once");
