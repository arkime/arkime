# Test config
use lib ".";
use ArkimeTest;
use Test::More tests => 46;
use Test::Differences;
use Data::Dumper;
use JSON;
use strict;

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
my $testenv='ARKIME__foo1=foo1 ARKIME_default__foo2=foo2 ARKIME_foo_fooDOTDASHCOLON__foo3=foo3 ARKIME_node__fooDASH4=4 ARKIME_overrideDASHips__10DOT1DOT0DOT0SLASH16="tag:ny-office;country:USA;asn:AS0000 This is neat"';

$out = `$testenv node ../viewer/viewer.js -c testconfig.ini -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff(from_json($out), from_json('{
   "OVERRIDE": {
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

$out = `$testenv node ../cont3xt/cont3xt.js -c testconfig.ini -o cont3xt.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff(from_json($out), from_json('{
   "OVERRIDE": {
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

$out = `$testenv node ../wiseService/wiseService.js -c testconfig.ini -o wiseService.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
$out =~ s/^\[.*\] //mg;
eq_or_diff(from_json($out), from_json('{
   "OVERRIDE": {
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

$out = `$testenv node ../viewer/viewer.js -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff(from_json($out), from_json('{
   "OVERRIDE": {
   },
   "CONFIG": {
     "foo": {
       "bar": "foobar"
     }
   }
 }'));

$out = `$testenv node ../cont3xt/cont3xt.js --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff(from_json($out), from_json('{
   "OVERRIDE": {
   },
   "CONFIG": {
     "foo": {
       "bar": "foobar"
     }
   }
 }'));

$out = `$testenv node ../wiseService/wiseService.js --regressionTests --dumpConfig 2>&1 1>/dev/null`;
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

$out = `$testenv ../capture/capture -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "CONFIG:
[default]

[foo]
bar=foobar
");


#### standard tests
sub doGoodTest {
    my ($config, $skipcapture) = @_;

    $out = `node ../viewer/viewer.js -c $config -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
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

    $out = `node ../cont3xt/cont3xt.js -c $config -o cont3xt.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
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

    $out = `node ../wiseService/wiseService.js -c $config -o wiseService.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
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

    $out = `node ../viewer/viewer.js -c $config -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
    eq_or_diff(from_json($out), from_json('{
       "OVERRIDE": {
         "default.foo": "bar",
         "test.foo": "bar"
       },
       "CONFIG": {
       }
     }'));

    $out = `node ../cont3xt/cont3xt.js -c $config -o cont3xt.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
    eq_or_diff(from_json($out), from_json('{
       "OVERRIDE": {
         "cont3xt.foo": "bar"
       },
       "CONFIG": {
       }
     }'));

    $out = `node ../wiseService/wiseService.js -c $config -o wiseService.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
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


doGoodTest("testconfig.ini");

#### NOTFOUND FILE

doNotFoundTest("notfound.ini");

#### FILE JSON

doGoodTest("testconfig.json");

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

SKIP: {
#### REDIS JSON
$out = `redis-cli -x set testconfig < testconfig.json`;
skip "Redis down", 6 if ($out ne "OK\n");

$url = "redis://127.0.0.1/0/testconfig";

doGoodTest($url, 1);

#### NOTFOUND REDIS JSON
$url = "redis://127.0.0.1/0/notfound";

doNotFoundTest($url, 1);

}

#### Clean up
unlink("testconfig.ini");
unlink("testconfig.json");
