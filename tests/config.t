# Test config
use lib ".";
use ArkimeTest;
use Test::More tests => 38;
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
$out = `ARKIME__foo1=foo1 ARKIME_default__foo2=foo2 ARKIME_foo_fooPERIODDASHCOLON__foo3=foo3 ARKIME_node__fooDASH4=4 node ../viewer/viewer.js -c testconfig.ini -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(2) { \'default.foo\' => \'bar\', \'test.foo\' => \'bar\' }
CONFIG {
  default: { var: \'1\', foo1: \'foo1\', foo2: \'foo2\' },
  node: { var: \'2\', \'foo-4\': \'4\' },
  \'foo_foo.-:\': { foo3: \'foo3\' }
}
");

$out = `ARKIME__foo1=foo1 ARKIME_default__foo2=foo2 ARKIME_foo_fooPERIODDASHCOLON__foo3=foo3 ARKIME_node__fooDASH4=4 node ../cont3xt/cont3xt.js -c testconfig.ini -o cont3xt.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(1) { \'cont3xt.foo\' => \'bar\' }
CONFIG {
  default: { var: \'1\', foo1: \'foo1\', foo2: \'foo2\' },
  node: { var: \'2\', \'foo-4\': \'4\' },
  \'foo_foo.-:\': { foo3: \'foo3\' }
}
");

$out = `ARKIME__foo1=foo1 ARKIME_default__foo2=foo2 ARKIME_foo_fooPERIODDASHCOLON__foo3=foo3 ARKIME_node__fooDASH4=4 node ../wiseService/wiseService.js -c testconfig.ini -o wiseService.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
$out =~ s/^\[.*\] //mg;
eq_or_diff($out, "OVERRIDE Map(1) { \'wiseService.foo\' => \'bar\' }
CONFIG {
  default: { var: \'1\', foo1: \'foo1\', foo2: \'foo2\' },
  node: { var: \'2\', \'foo-4\': \'4\' },
  \'foo_foo.-:\': { foo3: \'foo3\' }
}
");

$out = `ARKIME__foo1=foo1 ARKIME_default__foo2=foo2 ARKIME_foo_fooPERIODDASHCOLON__foo3=foo3 ARKIME_node__fooDASH4=4 ../capture/capture -c testconfig.ini -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE:
foo=bar
CONFIG:
[default]
var=1
foo1=foo1
foo2=foo2

[node]
var=2
foo-4=4

[foo_foo.-:]
foo3=foo3
");

#### FILE INI

$out = `node ../viewer/viewer.js -c testconfig.ini -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(2) { \'default.foo\' => \'bar\', \'test.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../cont3xt/cont3xt.js -c testconfig.ini -o cont3xt.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(1) { \'cont3xt.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../wiseService/wiseService.js -c testconfig.ini -o wiseService.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
$out =~ s/^\[.*\] //mg;
eq_or_diff($out, "OVERRIDE Map(1) { \'wiseService.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `../capture/capture -c testconfig.ini -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE:
foo=bar
CONFIG:
[default]
var=1

[node]
var=2
");

#### NOTFOUND FILE

$out = `node ../viewer/viewer.js -c notfound.ini -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(2) { \'default.foo\' => \'bar\', \'test.foo\' => \'bar\' }
CONFIG {}
");

$out = `node ../cont3xt/cont3xt.js -c notfound.ini -o cont3xt.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(1) { \'cont3xt.foo\' => \'bar\' }
CONFIG {}
");

$out = `node ../wiseService/wiseService.js -c notfound.ini -o wiseService.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
$out =~ s/^\[.*\] //mg;
eq_or_diff($out, "OVERRIDE Map(1) { \'wiseService.foo\' => \'bar\' }
CONFIG {}
");

$out = `../capture/capture -c notfound.ini -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "");

#### FILE JSON

$out = `node ../viewer/viewer.js -c testconfig.json -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(2) { \'default.foo\' => \'bar\', \'test.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../cont3xt/cont3xt.js -c testconfig.json -o cont3xt.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(1) { \'cont3xt.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../wiseService/wiseService.js -c testconfig.json -o wiseService.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
$out =~ s/^\[.*\] //mg;
eq_or_diff($out, "OVERRIDE Map(1) { \'wiseService.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `../capture/capture -c testconfig.json -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE:
foo=bar
CONFIG:
[default]
var=1

[node]
var=2
");

#### ELASTICSEARCH JSON

$es = "${ArkimeTest::elasticsearch}/testconfig/_doc/testconfig";
$es =~ s/^http/elasticsearch/;

$out = `node ../viewer/viewer.js -c $es -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(2) { \'default.foo\' => \'bar\', \'test.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../cont3xt/cont3xt.js -c $es -o cont3xt.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(1) { \'cont3xt.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../wiseService/wiseService.js -c $es -o wiseService.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
$out =~ s/^\[.*\] //mg;
eq_or_diff($out, "OVERRIDE Map(1) { \'wiseService.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `../capture/capture -c $es -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE:
foo=bar
CONFIG:
[default]
var=1

[node]
var=2
");

#### NOTFOUND ELASTICSEARCH JSON

$es = "${ArkimeTest::elasticsearch}/testconfig/_doc/notfound";
$es =~ s/^http/elasticsearch/;

$out = `node ../viewer/viewer.js -c $es -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(2) { \'default.foo\' => \'bar\', \'test.foo\' => \'bar\' }
CONFIG {}
");

$out = `node ../cont3xt/cont3xt.js -c $es -o cont3xt.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(1) { \'cont3xt.foo\' => \'bar\' }
CONFIG {}
");

$out = `node ../wiseService/wiseService.js -c $es -o wiseService.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
$out =~ s/^\[.*\] //mg;
eq_or_diff($out, "OVERRIDE Map(1) { \'wiseService.foo\' => \'bar\' }
CONFIG {}
");

$out = `../capture/capture -c $es -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "");

#### HTTP JSON

$url = "${ArkimeTest::elasticsearch}/testconfig/_source/testconfig";

$out = `node ../viewer/viewer.js -c $url -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(2) { \'default.foo\' => \'bar\', \'test.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../cont3xt/cont3xt.js -c $url -o cont3xt.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(1) { \'cont3xt.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../wiseService/wiseService.js -c $url -o wiseService.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
$out =~ s/^\[.*\] //mg;
eq_or_diff($out, "OVERRIDE Map(1) { \'wiseService.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `../capture/capture -c $url -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE:
foo=bar
CONFIG:
[default]
var=1

[node]
var=2
");

#### NOTFOUND HTTP JSON

$url = "${ArkimeTest::elasticsearch}/testconfig/_source/notfound";

$out = `node ../viewer/viewer.js -c $url -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(2) { \'default.foo\' => \'bar\', \'test.foo\' => \'bar\' }
CONFIG {}
");

$out = `node ../cont3xt/cont3xt.js -c $url -o cont3xt.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(1) { \'cont3xt.foo\' => \'bar\' }
CONFIG {}
");

$out = `node ../wiseService/wiseService.js -c $url -o wiseService.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
$out =~ s/^\[.*\] //mg;
eq_or_diff($out, "OVERRIDE Map(1) { \'wiseService.foo\' => \'bar\' }
CONFIG {}
");

$out = `../capture/capture -c $url -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "");

SKIP: {
#### REDIS JSON
$out = `redis-cli -x set testconfig < testconfig.json`;
skip "Redis down", 6 if ($out ne "OK\n");

$url = "redis://127.0.0.1/0/testconfig";

$out = `node ../viewer/viewer.js -c $url -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(2) { \'default.foo\' => \'bar\', \'test.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../cont3xt/cont3xt.js -c $url -o cont3xt.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(1) { \'cont3xt.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../wiseService/wiseService.js -c $url -o wiseService.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
$out =~ s/^\[.*\] //mg;
eq_or_diff($out, "OVERRIDE Map(1) { \'wiseService.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

#### NOTFOUND REDIS JSON
$url = "redis://127.0.0.1/0/notfound";

$out = `node ../viewer/viewer.js -c $url -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(2) { \'default.foo\' => \'bar\', \'test.foo\' => \'bar\' }
CONFIG {}
");

$out = `node ../cont3xt/cont3xt.js -c $url -o cont3xt.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(1) { \'cont3xt.foo\' => \'bar\' }
CONFIG {}
");

$out = `node ../wiseService/wiseService.js -c $url -o wiseService.foo=bar --regressionTests --dumpConfig 2>&1 1>/dev/null`;
$out =~ s/^\[.*\] //mg;
eq_or_diff($out, "OVERRIDE Map(1) { \'wiseService.foo\' => \'bar\' }
CONFIG {}
");
}

#### Clean up
unlink("testconfig.ini");
unlink("testconfig.json");
