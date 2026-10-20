# Test rules v2 expressions, runs capture in --tests mode so no Elasticsearch is
# needed. Parse tests read the parsed form capture logs with debug=2, match tests
# check which rules tagged a known session.
use Test::More tests => 167;
use JSON;
use strict;

my $file = "/tmp/arkime-rulesv2-$$.yaml";
END { unlink $file if $file && !$ENV{KEEP}; }

sub captureRule {
    my ($when, $expression) = @_;
    $expression =~ s/'/''/g;
    open(my $fh, '>', $file) or die "can't write $file";
    print $fh "version: 1\nrules:\n  - name: \"t\"\n    when: \"$when\"\n    expression: '$expression'\n    ops:\n      tags: \"x\"\n";
    close($fh);
    return `../capture/capture --tests -c config.test.ini -n test -o rulesFiles=$file -o debug=2 -r pcap/empty.pcap 2>&1`;
}

sub fieldsRule {
    my ($when, $fields, $pcap, $log) = @_;
    open(my $fh, '>', $file) or die "can't write $file";
    print $fh "version: 1\nrules:\n  - name: \"t\"\n    when: \"$when\"\n" . ($log ? "    log: true\n" : "") . "    fields:\n$fields    ops:\n      tags: \"x\"\n";
    close($fh);
    return `../capture/capture --tests -c config.test.ini -n test -o rulesFiles=$file -r pcap/@{[$pcap // "empty.pcap"]} 2>&1`;
}

sub fieldsError {
    my ($when, $fields, $expected) = @_;
    like(fieldsRule($when, $fields), qr/\Q$expected\E/, "fields error: $expected");
}

sub runRule {
    my ($when, $expression, $want) = @_;
    my $out = captureRule($when, $expression);
    return $1 if ($want && $out =~ /^$want: (.*)/m);
    return $1 if (!$want && $out =~ /rule 't' expression (\(.*)/);
    return "ERROR: $1\n$2" if ($out =~ /expression error: (.*)\n(.*\n.*)/);
    return "ERROR: $1" if ($out =~ /FATAL CONFIG ERROR - \S+: rule 't' (.*)/);
    return "UNKNOWN: $out";
}

sub parsed {
    my ($expression, $expected, $when) = @_;
    is(runRule($when // "afterClassify", $expression), $expected, $expression);
}

sub optimized {
    my ($expression, $expected) = @_;
    is(runRule("afterClassify", $expression, "optimized"), $expected, "optimized: $expression");
}

sub triggers {
    my ($expression, $expected) = @_;
    is(runRule("fieldSet", $expression, "triggers"), $expected, "triggers: $expression");
}

sub error {
    my ($expression, $expected, $when) = @_;
    my $out = runRule($when // "afterClassify", $expression);
    like($out, qr/^ERROR: \Q$expected\E/, "error: $expression") or diag($out);
}

# Precedence and grouping
parsed('host.http == a || host.http == b && port.dst == 80', '(or (== host.http "a") (and (== host.http "b") (== port.dst 80)))');
parsed('(host.http == a || host.http == b) && port.dst == 80', '(and (or (== host.http "a") (== host.http "b")) (== port.dst 80))');
parsed('!host.http == a && port.dst == 1', '(and (not (== host.http "a")) (== port.dst 1))');
parsed('host.http == a & host.http == b | host.http = c', '(or (and (== host.http "a") (== host.http "b")) (== host.http "c"))');

# Value forms
parsed('host.http == [foo*, *bar, *baz*, a*b?c, "x,y"]', '(== host.http head:"foo" tail:"bar" contains:"baz" glob:"a*b?c" "x,y")');
parsed('http.uri == /\/admin\/.*\.php/', '(== http.uri regex:/\/admin\/.*\.php/)');
parsed('http.method != ]GET,HEAD[', '(!=all http.method "GET" "HEAD")');
parsed('port.dst == [80, 1000-2000, -5]', '(== port.dst 80 range:1000-2000 -5)');
parsed('ip.dst == [10.0.0.0/8, 1.2.3.4, ipv4, ipv6, 2001:db8::/32]', '(== ip.dst 10.0.0.0/8 1.2.3.4/32 ipv4 ipv6 2001:db8::/32)');
parsed('host.http == "quoted \"x\""', '(== host.http "quoted \"x\"")');
parsed('host.http == EXISTS! && http.uri != EXISTS!', '(and (exists host.http) (!exists http.uri))');

# Math, comparisons and functions
parsed('databytes.src > 10 * databytes.dst + 1', '(> databytes.src (+ (* 10 databytes.dst) 1))');
parsed('-databytes.src < -5', '(< (neg databytes.src) -5)');
parsed('port.src == port.dst', '(== port.src port.dst)');
parsed('count(host.http) > 3 && has(http.uri)', '(and (> (count host.http) 3) (has http.uri))');
parsed('all(http.method, _ == [GET, HEAD]) && any(host.http, endsWith(_, .ru))', '(and (all http.method (== _ "GET" "HEAD")) (any host.http (endsWith _ ".ru")))');
parsed('lower(host.http) == *.foo.com', '(== (lower host.http) tail:".foo.com")');
parsed('bpf("tcp port 80") && isPrivate(ip.src)', '(and (bpf "tcp port 80") (isPrivate ip.src))', 'sessionSetup');

# Errors
error('host.htp == foo', "unknown field 'host.htp' at line 1 col 1");
error('host.http == foo &&', "unexpected end of expression");
error('port.dst == abc', "'abc' is not an integer or integer range at line 1 col 13");
error('port.dst == 10-5', "range '10-5' has min > max");
error('ip.dst == 10.0.0.0/33', "'10.0.0.0/33' is not an ip or cidr");
error('ip.dst == /10/', "regex can only be used with string values");
error('host.http', "'host.http' is a field, not a condition");
error('host.http == /a[/', "bad regex");
error('port.dst < foo.com', "unknown field 'foo.com'");
error('port.dst == 1 == 2', "comparisons can't be chained");
error('(host.http == foo', "unexpected end of expression, expected ')'");
error('host.http == *', "use 'EXISTS!' instead of '*'");
error('bpf("tcp")', "bpf() can only be used in sessionSetup rules");
error('bpf("tcpx port")', "bad bpf", 'sessionSetup');
error('nope(host.http)', "unknown function 'nope'");
error('count(5) > 1', "count() argument 1 must be a field");
error('_ == 5', "'_' can only be used inside any() or all()");
error('host.http + 1 > 2', "'+' needs numbers, found a string");
error('host.http == $shortcut', "shortcuts like '\$shortcut' aren't supported");
error('port.dst == [80,,90]', "empty list item");
error('port.dst == 80 extra', "expected an operator");
error(("(" x 300) . "port.dst == 80" . (")" x 300), "expression is nested more than 256 deep");
error(("!" x 300) . "(port.dst == 80)", "expression is nested more than 256 deep");
parsed(("(" x 200) . "port.dst == 80" . (")" x 200), '(== port.dst 80)');
error('host.http == a < 3', "comparisons can't be chained");
error('!(host.http == foo)', "fieldSet expression has nothing to trigger on", 'fieldSet');
error('packets.src > 1', "fieldSet expression has nothing to trigger on", 'fieldSet');

# Optimizer, folding, merging same field matches, cheapest first
optimized('port.dst == 80 || host.http == a || port.dst == 443', '(or (== port.dst 80 443) (== host.http "a"))');
optimized('!(host.http == a) && !(host.http == b*)', '(not (== host.http "a" head:"b"))');
optimized('any(host.http, _ == a) || any(host.http, _ == b)', '(any host.http (== _ "a" "b"))');
optimized('http.uri == /x/ && port.dst == 80', '(and (== port.dst 80) (== http.uri regex:/x/))');
optimized('databytes.src > 2 * 512 + 1', '(> databytes.src 1025)');
optimized('1 + 1 == 2 && host.http == a', '(== host.http "a")');
optimized('1 == 2 || host.http == a', '(== host.http "a")');
optimized('startsWith("abc", a) && max(1, 2.5) > 2 && port.dst == 1', '(== port.dst 1)');
optimized('host.http == a && host.http == a', '(== host.http "a")');
optimized('!(!(host.http == a))', '(== host.http "a")');
optimized('lower("ABC") == abc', 'true');
optimized('"abc" == *b* && -3 < abs(-2)', 'true');
optimized('host.http == a && 1 > 2', 'false');
optimized('port.dst / 0 == 1', '(== (/ port.dst 0) 1)');

# Large lists load and index quickly, fieldSet trigger dedupe used to be quadratic
{
    my $list = join(", ", map { "h$_.example.net" } (1 .. 20000));
    my $start = time();
    my $out = runRule("fieldSet", "host.http == [$list, h5.example.net]", "optimized");
    like($out, qr/^\(== host\.http "h1\.example\.net"/, "large list: compiles");
    cmp_ok(time() - $start, '<', 10, "large list: loads quickly");
}

# fields: rules run on the same engine, their load errors are unchanged
fieldsError("fieldSet", "      \"!port.dst\": 80\n", "NOT rules are not supported for fieldSet rules");
fieldsError("beforeFinalSave", "      port.dst: 80\n      \"!port.dst\": 81\n", "Can't have both ! and normal fields");
fieldsError("beforeFinalSave", "      \"!port.dst\": 81\n      port.dst: 80\n", "Can't have both ! and normal fields");
fieldsError("beforeFinalSave", "      \"port.dst,endsWith\": 80\n", "doesn't support modifier endsWith");
fieldsError("beforeFinalSave", "      port.dst: 90-80\n", "Min 90 > Max 80 not allowed");
fieldsError("beforeFinalSave", "      \"!ip.src\": 1.2.3.4\n", "Currently only string and integer fields support NOT rules");
like(fieldsRule("beforeFinalSave", "      host.http: xxxxxxxxxxxxx.xxx.com\n      port.dst: [80, 443]\n      \"!http.method\": POST\n", "http-content-zip.pcap", 1),
     qr/arkime_rules_match\(\): t - \S+ => \S+ - port\.dst \(80\), host\.http \(xxxxxxxxxxxxx\.xxx\.com\)\n/, "fields log: field (value) for each matched field");

# Registered functions, tests/plugins/test.c registers the test* ones
optimized('testDouble(21) == 42 && port.dst == 1', '(== port.dst 1)');
optimized('testRev("abc") == cba && testHalf(3) == 1.5 && testHasDot("a.b")', 'true');
optimized('testPortSum() > 1', '(> (testPortSum) 1)');
optimized('testNothing(1) == 1', '(== (testNothing 1) 1)');
error('testDouble("x") == 1', "testDouble() argument 1 must be an integer, found a string");
error('testDouble(1, 2) == 1', "testDouble() takes 1 argument, found 2");
error('testHasDot(port.dst)', "testHasDot() argument 1 must be a string, found a integer");
error('testSameIp(ip.src, port.dst)', "testSameIp() argument 2 must be an ip, found a integer");
error('testDouble(port.dst)', "expected a condition but found a integer value");
like(captureRule("afterClassify", 'testSameIp(ip.src, ip.dst)'), qr/CALL_B +testSameIp\(r\d+, r\d+\)/, "call: disassembly");

# Common subexpressions are computed once per condition
my $cse = captureRule("afterClassify", 'databytes.src * databytes.src > databytes.src + 1');
is(scalar(() = $cse =~ /LOAD_INT +r\d+ = databytes\.src/g), 1, "cse: databytes.src loaded once");

# fieldSet triggers, only fields used without ! and that capture sets
triggers('host.http == [a, *.b, c*, *d*] && port.dst == [80, 1000-2000] && ip.dst == [10.0.0.0/8, ipv6]',
         'port.dst:int:80 port.dst:range:1000-2000 ip.dst:cidr:10.0.0.0/8 ip.dst:cidr:::/0 host.http:str:a host.http:tail:.b host.http:head:c host.http:contains:d');
triggers('host.http == [a, /b/]', 'host.http:any');
triggers('host.http == a || host.http == EXISTS!', 'host.http:any');
triggers('port.src == 1-3', 'port.src:int:1 port.src:int:2 port.src:int:3');
triggers('!(host.http == a) && http.method == GET', 'http.method:str:GET');
triggers('!(http.method != GET)', 'http.method:str:GET');
triggers('count(host.http) > 1 || !(count(http.uri) > 2)', 'host.http:any http.uri:any');
triggers('packets.src > 1 && host.http == a', 'host.http:str:a');
triggers('lower(host.http) == a && any(http.method, _ == GET)', 'http.method:any host.http:any');

# Match tests, http-content-zip.pcap is one session 10.0.0.1:52925 -> 10.0.0.2:80
# GET xxxxxxxxxxxxx.xxx.com/a.zip, 7 src and 5 dst packets
my @matches = (
    ['host.http == *.xxx.com', 1],
    ['host.http == *.yyy.com', 0],
    ['host.http != *.yyy.com', 1],
    ['host.http != EXISTS!', 0],
    ['host.http == xxxxxxxxxxxxx.xxx.com && http.statuscode != 404', 1],
    ['host.http == x*.x?x.com', 1],
    ['len(host.http) == 21', 1],
    ['http.method == [POST, GET] && port.dst == 80', 1],
    ['http.method == ]GET,POST[', 0],
    ['http.hasheader.src == ]accept,host[', 1],
    ['port.dst == 80 && ip.src == 10.0.0.0/24 && !(ip.dst == 10.0.0.1)', 1],
    ['port.dst == 1000-2000 || port.src == 52000-53000', 1],
    ['port.src == port.dst', 0],
    ['port.src != port.dst', 1],
    ['packets.src > packets.dst', 1],
    ['packets.src + packets.dst == 12', 1],
    ['packets.src * 2 < packets.dst', 0],
    ['abs(packets.dst - packets.src) == 2', 1],
    ['max(packets.src, packets.dst) == 7.0', 1],
    ['packets.src / 0 == 1', 0],
    ['!(packets.src / 0 == 1)', 1],
    ['http.statuscode >= 200 && http.statuscode < 300', 1],
    ['http.user == EXISTS! || http.statuscode == 404', 0],
    ['http.md5 == EXISTS! && has(http.sha256)', 1],
    ['all(http.method, _ == GET)', 1],
    ['all(http.hasheader.src, _ == accept)', 0],
    ['all(http.user, _ == bob)', 0],
    ['any(http.hasheader.src, startsWith(_, user))', 1],
    ['any(http.hasheader.src, _ == host && all(http.method, _ == GET))', 1],
    ['any(http.hasheader.src, _ == host && lower(http.user-agent) == *safari*)', 1],
    ['count(http.hasheader.src) == 6', 1],
    ['lower(http.user-agent) == *chrome*', 1],
    ['icontains(http.user-agent, CHROME) && !contains(http.user-agent, CHROME)', 1],
    ['http.uri == /.*\.zip/', 1],
    ['http.uri == /\.zip/', 0],
    ['isPrivate(ip.src) && !isMulticast(ip.dst) && isV4(ip.dst) && !isV6(ip.src)', 1],
    ['ip.dst:port == "10.0.0.2:80"', 1],
    ['communityId == "XNKhxjIW0JZ2H0bhi58j9olVOcI="', 1],
    ['port.dst == 80 && bpf("tcp")', 1, 'sessionSetup'],
    ['bpf("udp")', 0, 'sessionSetup'],
    # fieldSet, rules are checked as each field gets values, so order shouldn't matter
    ['host.http == *.xxx.com', 1, 'fieldSet'],
    ['host.http == *.yyy.com', 0, 'fieldSet'],
    ['http.method == GET && host.http == xxxxxxxxxxxxx.xxx.com', 1, 'fieldSet'],
    ['host.http == xxxxxxxxxxxxx.xxx.com && http.method == GET', 1, 'fieldSet'],
    ['host.http == xxxxxxxxxxxxx.xxx.com && http.statuscode == 200', 1, 'fieldSet'],
    ['http.statuscode == 200-299 && http.method == GET', 1, 'fieldSet'],
    ['ip.src == 10.0.0.0/24 && port.dst == 80', 1, 'fieldSet'],
    ['ip.dst == ipv6', 0, 'fieldSet'],
    ['count(http.hasheader.src) >= 6', 1, 'fieldSet'],
    ['count(http.hasheader.src) > 6', 0, 'fieldSet'],
    ['http.uri == /.*\.zip/', 1, 'fieldSet'],
    ['host.http == *.xxx.com && !(http.method == POST)', 1, 'fieldSet'],
    ['host.http == *.xxx.com && http.method == POST', 0, 'fieldSet'],
    ['lower(http.user-agent) == *chrome*', 1, 'fieldSet'],
    ['http.hasheader.src == ]accept,host[', 1, 'fieldSet'],
    ['all(http.method, _ == GET) && port.dst == 80', 1, 'fieldSet'],
    # Optimizer must not change results
    ['port.dst == 81 || port.dst == 80', 1],
    ['!(host.http == a) && !(host.http == *.xxx.com)', 0],
    ['!(host.http == a) && !(host.http == *.yyy.com)', 1],
    ['any(http.method, _ == POST) || any(http.method, _ == GET)', 1],
    ['2 > 1 && host.http == *.xxx.com', 1],
    ['1 > 2 || host.http == *.yyy.com', 0],
    ['packets.src * packets.src == 49', 1],
    # Registered functions
    ['testDouble(port.dst) == 160', 1],
    ['testHalf(packets.src) == 3.5', 1],
    ['testRev(host.http) == moc.xxx.xxxxxxxxxxxxx', 1],
    ['testPortSum() == 53005', 1],
    ['testHasDot(host.http)', 1],
    ['testHasDot(http.method)', 0],
    ['testNothing(port.dst) == 1', 0],
    ['testNothing(port.dst) != 1', 1],
    ['testSameIp(ip.src, ip.src) && !testSameIp(ip.src, ip.dst)', 1],
    ['testHasDot(host.http)', 1, 'fieldSet'],
);

open(my $fh, '>', $file) or die "can't write $file";
print $fh "version: 1\nrules:\n";
for (my $i = 0; $i <= $#matches; $i++) {
    my ($expression, $expected, $when) = @{$matches[$i]};
    $expression =~ s/'/''/g;
    print $fh "  - name: \"m$i\"\n    when: \"" . ($when // "beforeFinalSave") . "\"\n    expression: '$expression'\n    ops:\n      tags: \"rv2-m$i\"\n";
}
# v1 fieldSet rule indexed alongside the expression rules
print $fh "  - name: \"v1\"\n    when: \"fieldSet\"\n    fields:\n      host.http:\n        - xxxxxxxxxxxxx.xxx.com\n    ops:\n      tags: \"rv2-v1\"\n";
close($fh);

my $out = `../capture/capture --tests -c config.test.ini -n test -o rulesFiles=$file -r pcap/http-content-zip.pcap 2>&1`;
my %tags = map { $_ => 1 } ($out =~ /"tags":\[([^\]]*)\]/ ? @{from_json("[$1]")} : ());
for (my $i = 0; $i <= $#matches; $i++) {
    my ($expression, $expected) = @{$matches[$i]};
    is($tags{"rv2-m$i"} ? 1 : 0, $expected, ($expected ? "match: " : "no match: ") . $expression);
}
ok($tags{"rv2-v1"}, "v1 fieldSet rule still matches");

# Explain, log: true logs each condition that matched and the value
my @explains = (
    ['host.http == *.xxx.com && !(http.method == POST) && packets.src > packets.dst',
     'packets.src > packets.dst (7, 5), host.http == *.xxx.com (xxxxxxxxxxxxx.xxx.com), !(http.method == POST)'],
    ['lower(http.user-agent) == *chrome* && ip.src == 10.0.0.0/24',
     'ip.src == 10.0.0.0/24 (10.0.0.1), lower(http.user-agent) == *chrome* (Mozilla/5.0 (Macintosh; Intel Mac OS X 10_8_5) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/31.0.16...)'],
    ['any(http.hasheader.src, _ == host) && port.dst == [80, 443]', 'port.dst == [80, 443] (80), any(http.hasheader.src, _ == host) (host)'],
    ['testHasDot(host.http) && testDouble(port.dst) > 100', 'testDouble(port.dst) > 100 (160, 100), testHasDot(host.http) (xxxxxxxxxxxxx.xxx.com)'],
    ['port.dst != [443, 8443] && http.method != POST && !(host.http == a) && !(http.statuscode != 200)',
     'port.dst != [443, 8443], !(http.statuscode != 200) (200), http.method != POST, !(host.http == a)'],
);
open($fh, '>', $file) or die "can't write $file";
print $fh "version: 1\nrules:\n";
for (my $i = 0; $i <= $#explains; $i++) {
    my $expression = $explains[$i]->[0];
    $expression =~ s/'/''/g;
    print $fh "  - name: \"e$i\"\n    when: \"beforeFinalSave\"\n    log: true\n    expression: '$expression'\n    ops:\n      tags: \"rv2-e$i\"\n";
}
close($fh);
$out = `../capture/capture --tests -c config.test.ini -n test -o rulesFiles=$file -r pcap/http-content-zip.pcap 2>&1`;
for (my $i = 0; $i <= $#explains; $i++) {
    my ($line) = $out =~ /e$i - [^\n]* - (.*)/;
    is($line, $explains[$i]->[1], "explain: $explains[$i]->[0]");
}
