use Test::More tests => 21;
use ArkimeTest;
use strict;

# ArkimeConfig refuses to start when a setting that restricts access or bounds a
# resource is broken. Driven at the config layer rather than by launching a
# viewer, so this needs no cluster and no generated common/version.js.

my $runner = "/tmp/arkime-config-validate-$$.js";
open(my $rf, '>', $runner) or die "can't write $runner";
print $rf "const path = require('path');\n";
print $rf "require(path.resolve(process.argv[2]))\n";
print $rf "  .initialize({ defaultConfigFile: process.argv[3], defaultSections: 'default' })\n";
print $rf "  .then(() => console.log('CONFIG OK'));\n";
close($rf);

# Returns (exitCode, output) for a [default] section body
sub tryConfig {
    my ($body) = @_;
    my $ini = "/tmp/arkime-config-validate-$$.ini";
    open(my $fh, '>', $ini) or die "can't write $ini";
    print $fh "[default]\n$body\n";
    close($fh);
    my $out = `node $runner ../common/arkimeConfig $ini 2>&1`;
    my $code = $? >> 8;
    unlink $ini;
    return ($code, $out);
}

my ($code, $out);

################################################################################
# a good config starts
################################################################################
($code, $out) = tryConfig("mcpAllowedIps=127.0.0.0/8;10.0.0.0/8\nuserAuthIps=::1\nuploadFileSizeLimit=2147483648\nmaxSessionsQueried=2000000\nmcpMaxQueryDays=0.5");
is($code, 0, "a valid config starts");
like($out, qr/CONFIG OK/, "and reports it loaded");

# no settings at all is fine, they are all optional
($code, $out) = tryConfig("");
is($code, 0, "an empty config starts");

# -1 is the documented no limit sentinel, it must survive the minimum check
($code, $out) = tryConfig("mcpMaxQueryDays=-1");
is($code, 0, "mcpMaxQueryDays=-1 is allowed");

################################################################################
# a broken allow list is a hard failure, never a shrug. Each of these used to
# be accepted and then silently match everything, or abort the process.
################################################################################
foreach my $bad ('10.0.0.0/abc', '10.0.0.0/', '10.0.0.0/33', '10/8', 'false') {
    ($code, $out) = tryConfig("mcpAllowedIps=$bad");
    is($code, 1, "mcpAllowedIps=$bad refuses to start");
}

($code, $out) = tryConfig("mcpAllowedIps=10.0.0.0/abc");
like($out, qr/refusing to start/, "the failure says it is refusing to start");
like($out, qr/mcpAllowedIps/, "and names the setting");
like($out, qr/10\.0\.0\.0\/abc/, "and quotes the bad entry");

# userAuthIps is the same control for the rest of the site
($code, $out) = tryConfig("userAuthIps=10.0.0.0/");
is($code, 1, "userAuthIps=10.0.0.0/ refuses to start");
like($out, qr/userAuthIps/, "and names that setting");

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

($code, $out) = tryConfig("uploadFileSizeLimit=0");
is($code, 1, "a value below the minimum refuses to start");

################################################################################
# every bad setting is reported in one pass, so the operator fixes them together
################################################################################
($code, $out) = tryConfig("mcpAllowedIps=10/8\nuploadFileSizeLimit=2g\nmaxSessionsQueried=abc\nmcpMaxQueryDays=sevem");
is($code, 1, "several bad settings refuse to start");
like($out, qr/4 bad settings/, "and all four are reported at once");

unlink $runner;
