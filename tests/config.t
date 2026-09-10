# Test config
use lib ".";
use ArkimeTest;
use Test::More tests => 100;
use Test::Differences;
use Data::Dumper;
use JSON;
use strict;

# This test curates its own ARKIME_ environment variables; scrub any inherited
# ones (e.g. from tests_ch.sh) so they don't leak into the dumped config
delete $ENV{$_} for grep { /^ARKIME_/ } keys %ENV;

open(FH, '>', "testconfig.ini") or die $!;
print FH <<EOF;
[default]
var=1
[node]
var=2
EOF
close(FH);

open(FH, '>', "testconfig.json") or die $!;
print FH <<EOF;
{
    "default": { "var":"1" },
    "node": { "var":"2" }
}
EOF
close(FH);

system("curl -s -k ${ArkimeTest::elasticsearch}/testconfig/_doc/testconfig -d '\@testconfig.json' -H 'Content-Type: application/json' > /dev/null");
esGet("/_refresh");

my ($out, $es, $url);

#### ENV
my $testenv='ARKIME_ignore=ignore ARKIME__foo1=foo1 ARKIME_default__foo2=foo2 ARKIME_foo_fooDOTDASHCOLON__foo3=foo3 ARKIME_node__fooDASH4=4 ARKIME_overrideDASHips__10DOT1DOT0DOT0SLASH16="tag:ny-office;country:USA;asn:AS0000 This is neat"';
$out = `cd ../viewer && $testenv node viewer.js -c ../tests/testconfig.ini -o foo=bar -o default.bar=foo -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff(from_json($out), from_json('{
   "OVERRIDE": {
     "default.bar": "foo",
     "default.foo": "bar",
     "test.foo": "bar"
   },
   "CONFIG": {
     "default": {
       "var": "1",
       "foo1": "foo1",
       "foo2": "foo2"
     },
     "node": {
       "var": "2",
       "foo-4": "4"
     },
     "foo_foo.-:": {
       "foo3": "foo3"
     },
     "override-ips": {
       "10.1.0.0/16": "tag:ny-office;country:USA;asn:AS0000 This is neat"
     }
   }
 }'));

$out = `cd ../cont3xt && $testenv node cont3xt.js -c ../tests/testconfig.ini -o cont3xt.foo=bar -o bar=foo --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff(from_json($out), from_json('{
   "OVERRIDE": {
     "cont3xt.bar": "foo",
     "cont3xt.foo": "bar"
   },
   "CONFIG": {
     "cont3xt": {
       "foo1": "foo1"
     },
     "default": {
       "var": "1",
       "foo2": "foo2"
     },
     "node": {
       "var": "2",
       "foo-4": "4"
     },
     "foo_foo.-:": {
       "foo3": "foo3"
     },
     "override-ips": {
       "10.1.0.0/16": "tag:ny-office;country:USA;asn:AS0000 This is neat"
     }
   }
 }'));

$out = `cd ../wiseService && $testenv node wiseService.js -c ../tests/testconfig.ini -o wiseService.foo=bar -o bar=foo --regressionTests --dumpConfig 2>&1 1>/dev/null`;
$out =~ s/^\[.*\] //mg;
eq_or_diff(from_json($out), from_json('{
   "OVERRIDE": {
     "wiseService.bar": "foo",
     "wiseService.foo": "bar"
   },
   "CONFIG": {
     "default": {
       "var": "1",
       "foo2": "foo2"
     },
     "node": {
       "var": "2",
       "foo-4": "4"
     },
     "foo_foo.-:": {
       "foo3": "foo3"
     },
     "override-ips": {
       "10.1.0.0/16": "tag:ny-office;country:USA;asn:AS0000 This is neat"
     },
     "wiseService": {
       "foo1": "foo1"
     }
   }
 }'));

$out = `$testenv ../capture/capture -c testconfig.ini -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE:
foo=bar
CONFIG:
[default]
foo1=foo1
foo2=foo2
var=1

[foo_foo.-:]
foo3=foo3

[node]
foo-4=4
var=2

[override-ips]
10.1.0.0/16=tag:ny-office;country:USA;asn:AS0000 This is neat
");

#### No config, don't set anything in default
$testenv='ARKIME_foo__bar=foobar';

SKIP: {
skip "Running on system with arkime installed", 2 if (-f "/opt/arkime/etc/config.ini");
$out = `cd ../viewer && $testenv node viewer.js -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff(from_json($out), from_json('{
   "OVERRIDE": {
   },
   "CONFIG": {
     "foo": {
       "bar": "foobar"
     }
   }
 }'));

$out = `$testenv ../capture/capture -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "CONFIG:
[default]

[foo]
bar=foobar
");
}

SKIP: {
skip "Running on system with arkime installed", 1 if (-f "/opt/arkime/etc/cont3xt.ini");
$out = `cd ../cont3xt && $testenv node cont3xt.js --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff(from_json($out), from_json('{
   "OVERRIDE": {
   },
   "CONFIG": {
     "foo": {
       "bar": "foobar"
     }
   }
 }'));
}

SKIP: {
skip "Running on system with arkime installed", 1 if (-f "/opt/arkime/etc/wiseService.ini");
$out = `cd ../wiseService && $testenv node wiseService.js --regressionTests --dumpConfig 2>&1 1>/dev/null`;
print Dumper($out);
$out =~ s/^\[.*\] //mg;
eq_or_diff(from_json($out), from_json('{
   "OVERRIDE": {
   },
   "CONFIG": {
     "foo": {
       "bar": "foobar"
     }
   }
 }'));
}

#### standard tests
sub doGoodTest {
    my ($config, $skipcapture) = @_;

    $out = `cd ../viewer && node viewer.js -c $config -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
    eq_or_diff(from_json($out), from_json('{
       "OVERRIDE": {
         "default.foo": "bar",
         "test.foo": "bar"
       },
       "CONFIG": {
         "default": {
           "var": "1"
         },
         "node": {
           "var": "2"
         }
       }
     }'));

    $out = `cd ../cont3xt && node cont3xt.js -c $config -o cont3xt.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
    eq_or_diff(from_json($out), from_json('{
       "OVERRIDE": {
         "cont3xt.foo": "bar"
       },
       "CONFIG": {
         "default": {
           "var": "1"
         },
         "node": {
           "var": "2"
         }
       }
     }'));

    $out = `cd ../wiseService && node wiseService.js -c $config -o wiseService.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
    $out =~ s/^\[.*\] //mg;
    eq_or_diff(from_json($out), from_json('{
       "OVERRIDE": {
         "wiseService.foo": "bar"
       },
       "CONFIG": {
         "default": {
           "var": "1"
         },
         "node": {
           "var": "2"
         }
       }
     }'));

    return if ($skipcapture);

    $out = `../capture/capture -c $config -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
    eq_or_diff($out, "OVERRIDE:
foo=bar
CONFIG:
[default]
var=1

[node]
var=2
");
}


sub doNotFoundTest {
    my ($config, $skipcapture) = @_;

    $out = `cd ../viewer && node viewer.js -c $config -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
    eq_or_diff(from_json($out), from_json('{
       "OVERRIDE": {
         "default.foo": "bar",
         "test.foo": "bar"
       },
       "CONFIG": {
       }
     }'));

    $out = `cd ../cont3xt && node cont3xt.js -c $config -o cont3xt.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
    eq_or_diff(from_json($out), from_json('{
       "OVERRIDE": {
         "cont3xt.foo": "bar"
       },
       "CONFIG": {
       }
     }'));

    $out = `cd ../wiseService && node wiseService.js -c $config -o wiseService.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
    $out =~ s/^\[.*\] //mg;
    eq_or_diff(from_json($out), from_json('{
       "OVERRIDE": {
         "wiseService.foo": "bar"
       },
       "CONFIG": {
       }
     }'));

    return if ($skipcapture);

    $out = `../capture/capture -c $config -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
    eq_or_diff($out, "");
}

#### FILE INI


doGoodTest("../tests/testconfig.ini");

#### NOTFOUND FILE

doNotFoundTest("notfound.ini");

#### FILE JSON

doGoodTest("../tests/testconfig.json");

#### NOTFOUND FILE JSON

doNotFoundTest("notfound.json");

#### ELASTICSEARCH JSON

$es = "${ArkimeTest::elasticsearch}/testconfig/_doc/testconfig";
$es =~ s/^http/elasticsearch/;

doGoodTest($es);

#### NOTFOUND ELASTICSEARCH JSON

$es = "${ArkimeTest::elasticsearch}/testconfig/_doc/notfound";
$es =~ s/^http/elasticsearch/;

doNotFoundTest($es);

#### HTTP JSON

$url = "${ArkimeTest::elasticsearch}/testconfig/_source/testconfig";

doGoodTest($url);

#### NOTFOUND HTTP JSON

$url = "${ArkimeTest::elasticsearch}/testconfig/_source/notfound";

doNotFoundTest($url);

#### REDIS JSON
use IO::Socket::INET;
my $json = do { local $/; open my $fh, '<', 'testconfig.json' or die $!; <$fh> };
my $rs = IO::Socket::INET->new(PeerAddr => "127.0.0.1", PeerPort => 7379, Proto => "tcp");
if ($rs) { my $vlen = length($json); print $rs "*3\r\n\$3\r\nSET\r\n\$10\r\ntestconfig\r\n\$$vlen\r\n$json\r\n"; $rs->close(); }

$url = "redis://127.0.0.1:7379/0/testconfig";

doGoodTest($url);

#### NOTFOUND REDIS JSON
$url = "redis://127.0.0.1:7379/0/notfound";

doNotFoundTest($url);

#### Clean up
unlink("testconfig.ini");
unlink("testconfig.json");

#### View Config API - what the running viewer will show an admin

my $vcToken = getTokenCookie();
viewerPostToken("/api/user", '{"userId": "viewconfig-notadmin", "userName": "notadmin", "enabled":true, "password":"password", "roles":["arkimeUser"]}', $vcToken);
addUser("-n testuser viewconfig-admin viewconfig-admin viewconfig-admin --roles arkimeAdmin");
addUser("-n testuser viewconfig-super viewconfig-super viewconfig-super --roles superAdmin");

# node test runs the default mode, superAdmin
my $vc = viewerGet("/api/viewconfig?arkimeRegressionUser=viewconfig-notadmin");
is($vc->{text}, "You do not have permission to access this resource", "view config refuses a non admin");

$vc = viewerGet("/api/viewconfig?arkimeRegressionUser=viewconfig-admin");
is($vc->{text}, "You do not have permission to access this resource", "view config refuses an app admin by default");

$vc = viewerGet("/api/viewconfig?arkimeRegressionUser=viewconfig-super");
is($vc->{success}, 1, "view config success");
is($vc->{sections}->{default}->{passwordSecret}, "[redacted]", "secret key redacted");
is($vc->{sections}->{test2}->{packetPortalConnect}, "http://test2:[redacted]\@127.0.0.1:8123", "url password redacted");
is($vc->{sections}->{test}->{passwordSecret}, "", "empty secret not redacted");
ok((grep { $_ eq "default.passwordSecret" } @{$vc->{redacted}}), "redacted list has the secret key");
ok(exists $vc->{overrides}->{"default.elasticsearch"}, "command line override for default shown");
ok(exists $vc->{overrides}->{"test.elasticsearch"}, "command line override for the node shown");
ok((grep { $_ eq "test" } @{$vc->{defaultSections}}), "default sections include the node");
like($vc->{configFile}, qr/config.test.ini/, "config file shown");
is($vc->{sections}->{'moloch-clusters'}->{test2}, "url:http://localhost:8124;passwordSecret:[redacted];name:Test2", "secret packed inside a value redacted");
is($vc->{sections}->{'esproxy-sensors'}->{test2}, "pass:[redacted];ip:1.2.3.4,127.0.0.1", "pass packed inside a value redacted");
is($vc->{sections}->{keks}->{test}, "[redacted]", "every value in the keks section redacted");
is($vc->{sections}->{'viewconfig-test'}->{key}, "[redacted]", "a bare key setting redacted");
is($vc->{sections}->{'viewconfig-test'}->{keyColumn}, "Prefix", "a key named setting that isn't a credential is left alone");
is($vc->{sections}->{'viewconfig-test'}->{esClientKeyPass}, "[redacted]", "a name ending in Pass redacted");
is($vc->{sections}->{'viewconfig-test'}->{bypassCount}, "5", "pass inside a longer word is left alone");
is($vc->{sections}->{testuser}->{disableUserPasswordUI}, "false", "a switch that reads like a credential is left alone");
ok((grep { $_ eq "viewconfig-test.esClientKeyPass" } @{$vc->{redacted}}), "redacted list has the key passphrase");
is($vc->{sections}->{'viewconfig-test'}->{comboUrl}, "http://user:[redacted]\@example.com/x", "url password with a comma in it redacted");
is($vc->{sections}->{'viewconfig-test'}->{queryUrl}, "http://example.com?to=a\@b.com", "an \@ in a query string is not treated as a url password");
unlike(to_json($vc), qr/test2:test2\@/, "no url password anywhere in the response");
unlike(to_json($vc->{sections}->{default}), qr/"passwordSecret":"password"/, "no secret value anywhere in the response");

#### View Config API - viewConfigMode

# node test2 is appAdmin, so this app's own admin role is enough there
$vc = viewerGet2("/api/viewconfig?arkimeRegressionUser=viewconfig-admin");
is($vc->{success}, 1, "view config allows an app admin when viewConfigMode is appAdmin");

$vc = viewerGet2("/api/viewconfig?arkimeRegressionUser=viewconfig-notadmin");
is($vc->{text}, "You do not have permission to access this resource", "viewConfigMode appAdmin still refuses a non admin");

# node all is off, so there is nothing there at all, even for a superAdmin
my $offRes = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8125/api/viewconfig?arkimeRegressionUser=viewconfig-super");
is($offRes->code, 404, "viewConfigMode off answers 404");
is(from_json($offRes->content)->{success}, 0, "viewConfigMode off says no");

$offRes = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8125/api/viewconfig/totp?arkimeRegressionUser=viewconfig-super", 'Content-Type' => 'application/json', Content => '{"code": "123456"}');
is($offRes->code, 404, "viewConfigMode off hides the totp api too");

#### View Config API - another node over s2s

$vc = viewerGet("/api/viewconfig/node/test2?arkimeRegressionUser=viewconfig-super");
is($vc->{success}, 1, "remote node view config success");
is($vc->{node}, "test2", "remote node view config is for the node asked for");
is($vc->{sections}->{default}->{passwordSecret}, "[redacted]", "remote node redacts too");

$vc = viewerGet("/api/viewconfig/node/nosuchnode?arkimeRegressionUser=viewconfig-super");
is($vc->{success}, 0, "remote node view config fails for an unknown node");

#### View Config API - a user with totp must prove it first

addUser("-n testuser viewconfig-totp viewconfig-totp viewconfig-totp --roles superAdmin");
my $totpToken = getTokenCookie('viewconfig-totp');

$vc = viewerGetToken("/api/viewconfig?arkimeRegressionUser=viewconfig-totp", $totpToken);
is($vc->{success}, 1, "view config without totp set needs no code");

my $setup = viewerPostToken("/api/user/totp/setup?arkimeRegressionUser=viewconfig-totp", '{}', $totpToken);
viewerPostToken("/api/user/totp/confirm?arkimeRegressionUser=viewconfig-totp", '{"code": "' . generate_totp($setup->{secret}) . '"}', $totpToken);

$vc = viewerGetToken("/api/viewconfig?arkimeRegressionUser=viewconfig-totp", $totpToken);
is($vc->{success}, 0, "view config with totp set is refused without a code");
is($vc->{needTotp}, 1, "view config asks for a totp code");

$vc = viewerPostToken("/api/viewconfig/totp?arkimeRegressionUser=viewconfig-totp", '{"code": "000000"}', $totpToken);
is($vc->{success}, 0, "view config totp fails with a bad code");

$vc = viewerPostToken("/api/viewconfig/totp?arkimeRegressionUser=viewconfig-totp", '{"code": "' . generate_totp($setup->{secret}) . '"}', $totpToken);
is($vc->{success}, 1, "view config totp succeeds with a good code");
my $grant = $vc->{grant};
ok(defined $grant && length($grant) == 64, "view config totp hands back a grant for this browser");

# the grant, not the user, is what unlocks it, so another browser stays locked
my $grantUrl = "http://$ArkimeTest::host:8123/api/viewconfig?arkimeRegressionUser=viewconfig-totp";

$vc = viewerGetToken("/api/viewconfig?arkimeRegressionUser=viewconfig-totp", $totpToken);
is($vc->{success}, 0, "view config stays locked for a browser without the grant");

$vc = from_json($ArkimeTest::userAgent->get($grantUrl, "x-arkime-cookie" => $totpToken, "x-arkime-viewconfig" => "0" x 64)->content);
is($vc->{success}, 0, "view config refuses a made up grant");

$vc = from_json($ArkimeTest::userAgent->get($grantUrl, "x-arkime-cookie" => $totpToken, "x-arkime-viewconfig" => $grant)->content);
is($vc->{success}, 1, "view config is unlocked with the grant");

# the grant is bound to the user it was issued to
$vc = from_json($ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/viewconfig?arkimeRegressionUser=viewconfig-super", "x-arkime-viewconfig" => $grant)->content);
is($vc->{success}, 1, "a user with no totp is unaffected by someone else's grant");

#### View Config API - the other apps serve their own config

addUser("-n testuser viewconfig-cont3xt viewconfig-cont3xt viewconfig-cont3xt --roles superAdmin");
addUser("-n testuser viewconfig-parliament viewconfig-parliament viewconfig-parliament --roles superAdmin");
addUser("-n testuser viewconfig-wise viewconfig-wise viewconfig-wise --roles superAdmin");

$vc = cont3xtGet("/api/viewconfig?arkimeRegressionUser=viewconfig-cont3xt");
is($vc->{success}, 1, "cont3xt view config success");
ok((grep { $_ eq "cont3xt" } @{$vc->{defaultSections}}), "cont3xt reads its own section");

$vc = parliamentGet("/parliament/api/viewconfig?arkimeRegressionUser=viewconfig-parliament");
is($vc->{success}, 1, "parliament view config success");
ok((grep { $_ eq "parliament" } @{$vc->{defaultSections}}), "parliament reads its own section");

$ArkimeTest::userAgent->credentials("$ArkimeTest::host:8081", 'Moloch', 'viewconfig-wise', 'viewconfig-wise');
$vc = from_json($ArkimeTest::userAgent->get("http://$ArkimeTest::host:8081/api/viewconfig")->content);
$ArkimeTest::userAgent->credentials("$ArkimeTest::host:8081", 'Moloch', '', '');
is($vc->{success}, 1, "wise view config success");
ok((grep { $_ eq "wiseService" } @{$vc->{defaultSections}}), "wise reads its own section");
is($vc->{sections}->{cache}->{redisURL}, "[redacted]", "a wise field marked password is hidden whatever it is named");
is($vc->{sections}->{'splunk:test'}->{password}, "[redacted]", "a wise source password is hidden");
is($vc->{sections}->{'databricks:test'}->{token}, "[redacted]", "a wise source token is hidden");
