# Test config
use lib ".";
use ArkimeTest;
use Test::More tests => 46;
use Test::Differences;
use Data::Dumper;
use JSON;
use strict;

system("perl mini-redis.pl 7379 &");

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

doGoodTest($url, 1);

#### NOTFOUND REDIS JSON
$url = "redis://127.0.0.1:7379/0/notfound";

doNotFoundTest($url, 1);

#### Clean up
unlink("testconfig.ini");
unlink("testconfig.json");
if (my $rs2 = IO::Socket::INET->new(PeerAddr => "127.0.0.1", PeerPort => 7379, Proto => "tcp")) { print $rs2 "*1\r\n\$8\r\nSHUTDOWN\r\n"; $rs2->close(); }
