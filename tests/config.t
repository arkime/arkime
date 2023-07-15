# Test config
use lib ".";
use MolochTest;
use Test::More tests => 34;
use Test::Differences;
use Data::Dumper;
use JSON;
use strict;

open(FH, '>', "../viewer/public/testconfig.ini") or die $!;
print FH <<EOF;
[default]
var=1
[node]
var=2
EOF
close(FH);

open(FH, '>', "../viewer/public/testconfig.json") or die $!;
print FH <<EOF;
{
    "default": { "var":"1" },
    "node": { "var":"2" }
}
EOF
close(FH);

system("curl -s -k ${MolochTest::elasticsearch}/testconfig/_doc/testconfig -d @../viewer/public/testconfig.json -H 'Content-Type: application/json' > /dev/null");
esGet("/_refresh");

my ($out, $es, $url);

#### FILE INI

$out = `node ../viewer/viewer.js -c ../viewer/public/testconfig.ini -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(2) { \'default.foo\' => \'bar\', \'test.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../cont3xt/cont3xt.js -c ../viewer/public/testconfig.ini -o cont3xt.foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(1) { \'cont3xt.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../wiseService/wiseService.js -c ../viewer/public/testconfig.ini -o wiseService.foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
$out =~ s/^\[.*\] //mg;
eq_or_diff($out, "OVERRIDE Map(1) { \'wiseService.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `../capture/capture -c ../viewer/public/testconfig.ini -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE:
foo=bar
CONFIG:
[default]
var=1

[node]
var=2
");

#### NOTFOUND FILE

$out = `node ../viewer/viewer.js -c ../viewer/public/notfound.ini -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(2) { \'default.foo\' => \'bar\', \'test.foo\' => \'bar\' }
CONFIG {}
");

$out = `node ../cont3xt/cont3xt.js -c ../viewer/public/notfound.ini -o cont3xt.foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(1) { \'cont3xt.foo\' => \'bar\' }
CONFIG {}
");

$out = `node ../wiseService/wiseService.js -c ../viewer/public/notfound.ini -o wiseService.foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
$out =~ s/^\[.*\] //mg;
eq_or_diff($out, "OVERRIDE Map(1) { \'wiseService.foo\' => \'bar\' }
CONFIG {}
");

$out = `../capture/capture -c ../viewer/public/notfound.ini -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "");

#### FILE JSON

$out = `node ../viewer/viewer.js -c ../viewer/public/testconfig.json -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(2) { \'default.foo\' => \'bar\', \'test.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../cont3xt/cont3xt.js -c ../viewer/public/testconfig.json -o cont3xt.foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(1) { \'cont3xt.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../wiseService/wiseService.js -c ../viewer/public/testconfig.json -o wiseService.foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
$out =~ s/^\[.*\] //mg;
eq_or_diff($out, "OVERRIDE Map(1) { \'wiseService.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `../capture/capture -c ../viewer/public/testconfig.json -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE:
foo=bar
CONFIG:
[default]
var=1

[node]
var=2
");

#### ELASTICSEARCH JSON

$es = "${MolochTest::elasticsearch}/testconfig/_doc/testconfig";
$es =~ s/^http/elasticsearch/;

$out = `node ../viewer/viewer.js -c $es -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(2) { \'default.foo\' => \'bar\', \'test.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../cont3xt/cont3xt.js -c $es -o cont3xt.foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(1) { \'cont3xt.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../wiseService/wiseService.js -c $es -o wiseService.foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
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

$es = "${MolochTest::elasticsearch}/testconfig/_doc/notfound";
$es =~ s/^http/elasticsearch/;

$out = `node ../viewer/viewer.js -c $es -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(2) { \'default.foo\' => \'bar\', \'test.foo\' => \'bar\' }
CONFIG {}
");

$out = `node ../cont3xt/cont3xt.js -c $es -o cont3xt.foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(1) { \'cont3xt.foo\' => \'bar\' }
CONFIG {}
");

$out = `node ../wiseService/wiseService.js -c $es -o wiseService.foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
$out =~ s/^\[.*\] //mg;
eq_or_diff($out, "OVERRIDE Map(1) { \'wiseService.foo\' => \'bar\' }
CONFIG {}
");

$out = `../capture/capture -c $es -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "");

#### HTTP JSON

$url = "${MolochTest::elasticsearch}/testconfig/_doc/testconfig/_source";

$out = `node ../viewer/viewer.js -c $url -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(2) { \'default.foo\' => \'bar\', \'test.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../cont3xt/cont3xt.js -c $url -o cont3xt.foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(1) { \'cont3xt.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../wiseService/wiseService.js -c $url -o wiseService.foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
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

$url = "${MolochTest::elasticsearch}/testconfig/_doc/notfound/_source";

$out = `node ../viewer/viewer.js -c $url -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(2) { \'default.foo\' => \'bar\', \'test.foo\' => \'bar\' }
CONFIG {}
");

$out = `node ../cont3xt/cont3xt.js -c $url -o cont3xt.foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(1) { \'cont3xt.foo\' => \'bar\' }
CONFIG {}
");

$out = `node ../wiseService/wiseService.js -c $url -o wiseService.foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
$out =~ s/^\[.*\] //mg;
eq_or_diff($out, "OVERRIDE Map(1) { \'wiseService.foo\' => \'bar\' }
CONFIG {}
");

$out = `../capture/capture -c $url -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "");

SKIP: {
#### REDIS JSON
$out = `redis-cli -x set testconfig < ../viewer/public/testconfig.json`;
skip "Redis down", 6 if ($out ne "OK\n");

$url = "redis://127.0.0.1/0/testconfig";

$out = `node ../viewer/viewer.js -c $url -o foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(2) { \'default.foo\' => \'bar\', \'test.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../cont3xt/cont3xt.js -c $url -o cont3xt.foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(1) { \'cont3xt.foo\' => \'bar\' }
CONFIG { default: { var: \'1\' }, node: { var: \'2\' } }
");

$out = `node ../wiseService/wiseService.js -c $url -o wiseService.foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
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

$out = `node ../cont3xt/cont3xt.js -c $url -o cont3xt.foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
eq_or_diff($out, "OVERRIDE Map(1) { \'cont3xt.foo\' => \'bar\' }
CONFIG {}
");

$out = `node ../wiseService/wiseService.js -c $url -o wiseService.foo=bar -n test --regressionTests --dumpConfig 2>&1 1>/dev/null`;
$out =~ s/^\[.*\] //mg;
eq_or_diff($out, "OVERRIDE Map(1) { \'wiseService.foo\' => \'bar\' }
CONFIG {}
");
}

#### Clean up
unlink("../viewer/public/testconfig.ini");
unlink("../viewer/public/testconfig.json");
