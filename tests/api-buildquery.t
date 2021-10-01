use Test::More tests => 56;
use Cwd;
use URI::Escape;
use MolochTest;
use Data::Dumper;
use JSON;
use Test::Differences;
use strict;

my $json;

sub doTest {
my ($expression, $expected, $debug) = @_;
    local $Test::Builder::Level = $Test::Builder::Level + 1;
    $json = viewerGet('/api/buildquery?date=-1&expression=' . uri_escape($expression));
    eq_or_diff($json->{esquery}->{query}->{bool}->{filter}[0], from_json($expected), {context => 3});
    diag to_json($json->{esquery}->{query}->{bool}->{filter}[0]) if ($debug);
}

# Create shortcuts for testing
esPost("/tests_lookups/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }');
my $token = getTokenCookie();
my $ipshortcut1 = viewerPostToken("/api/shortcut", '{"name":"ipshortcut1","type":"ip","value":"10.10.10.10"}', $token)->{shortcut}->{id};
my $ipshortcut2 = viewerPostToken("/api/shortcut", '{"name":"ipshortcut2","type":"ip","value":"10.10.10.10"}', $token)->{shortcut}->{id};


#### IP.SRC
doTest('ip.src == 1.2.3.4', '{"term":{"source.ip":"1.2.3.4"}}');
doTest('ip.src != 1.2.3.4', '{"bool":{"must_not":{"term":{"source.ip":"1.2.3.4"}}}}');

doTest('ip.src == [1.2.3.4]', '{"term":{"source.ip":"1.2.3.4"}}');
doTest('ip.src != [1.2.3.4]', '{"bool":{"must_not":{"term":{"source.ip":"1.2.3.4"}}}}');

doTest('ip.src == 1.2.3.4/31', '{"term":{"source.ip":"1.2.3.4/31"}}');
doTest('ip.src != 1.2.3.4/31', '{"bool":{"must_not":{"term":{"source.ip":"1.2.3.4/31"}}}}');

doTest('ip.src == 1.2.3', '{"term":{"source.ip":"1.2.3.0/24"}}');
doTest('ip.src != 1.2.3', '{"bool":{"must_not":{"term":{"source.ip":"1.2.3.0/24"}}}}');

doTest('ip.src == 1.2.3/25', '{"term":{"source.ip":"1.2.3.0/25"}}');
doTest('ip.src != 1.2.3/25', '{"bool":{"must_not":{"term":{"source.ip":"1.2.3.0/25"}}}}');

doTest('ip.src == 1.2.3.', '{"term":{"source.ip":"1.2.3.0/24"}}');
doTest('ip.src != 1.2.3.', '{"bool":{"must_not":{"term":{"source.ip":"1.2.3.0/24"}}}}');

doTest('ip.src == 1.2.3./23', '{"term":{"source.ip":"1.2.3.0/23"}}');
doTest('ip.src != 1.2.3./23', '{"bool":{"must_not":{"term":{"source.ip":"1.2.3.0/23"}}}}');

doTest('ip.src == 1./23', '{"term":{"source.ip":"1.0.0.0/23"}}');
doTest('ip.src != 1./23', '{"bool":{"must_not":{"term":{"source.ip":"1.0.0.0/23"}}}}');

doTest('ip.src == 1./23:80', '{"bool":{"must":[{"term":{"source.ip":"1.0.0.0/23"}},{"term":{"source.port":"80"}}]}}');
doTest('ip.src != 1./23:80', '{"bool":{"must_not":{"bool":{"must":[{"term":{"source.ip":"1.0.0.0/23"}},{"term":{"source.port":"80"}}]}}}}');

doTest('ip.src == 1.2.3.4:80', '{"bool":{"must":[{"term":{"source.ip":"1.2.3.4"}},{"term":{"source.port":"80"}}]}}');
doTest('ip.src != 1.2.3.4:80', '{"bool":{"must_not":{"bool":{"must":[{"term":{"source.ip":"1.2.3.4"}},{"term":{"source.port":"80"}}]}}}}');

doTest('ip.src == :80', '{"term":{"source.port":"80"}}');
doTest('ip.src != :80', '{"bool":{"must_not":{"term":{"source.port":"80"}}}}');

doTest('ip.src == [1.2.3.4,2.3.4.5:80,:81,1.2.3:82]', '{"bool":{"should":[{"term":{"source.ip":"1.2.3.4"}},{"bool":{"must":[{"term":{"source.ip":"2.3.4.5"}},{"term":{"source.port":"80"}}]}},{"term":{"source.port":"81"}},{"bool":{"must":[{"term":{"source.ip":"1.2.3.0/24"}},{"term":{"source.port":"82"}}]}}]}}');
doTest('ip.src != [1.2.3.4,2.3.4.5:80,:81,1.2.3:82]', '{"bool":{"must_not":[{"term":{"source.ip":"1.2.3.4"}},{"bool":{"must":[{"term":{"source.ip":"2.3.4.5"}},{"term":{"source.port":"80"}}]}},{"term":{"source.port":"81"}},{"bool":{"must":[{"term":{"source.ip":"1.2.3.0/24"}},{"term":{"source.port":"82"}}]}}]}}');

doTest('ip.src == $ipshortcut1', qq({"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}}));
doTest('ip.src != $ipshortcut1', qq({"bool":{"must_not":[{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}}]}}));

doTest('ip.src == [$ipshortcut1]', qq({"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}}));
doTest('ip.src != [$ipshortcut1]', qq({"bool":{"must_not":[{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}}]}}));

doTest('ip.src == [$ipshortcut1, $ipshortcut2]', qq({"bool":{"should":[{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}}]}}));
doTest('ip.src != [$ipshortcut1, $ipshortcut2]', qq({"bool":{"must_not":[{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}}]}}));

doTest('ip.src == $ipshortcut*', qq({"bool":{"should":[{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}}]}}));
doTest('ip.src != $ipshortcut*', qq({"bool":{"must_not":[{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}}]}}));

doTest('ip.src == [$ipshortcut*]', qq({"bool":{"should":[{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}}]}}));
doTest('ip.src != [$ipshortcut*]', qq({"bool":{"must_not":[{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}}]}}));

doTest('ip.src == [1.2.3.4,$ipshortcut1]', qq({"bool":{"should":[{"term":{"source.ip":"1.2.3.4"}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}}]}}));
doTest('ip.src == [$ipshortcut1,1.2.3.4]', qq({"bool":{"should":[{"term":{"source.ip":"1.2.3.4"}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}}]}}));
doTest('ip.src == [1.2.3.4,$ipshortcut1,2.3.4.5:80]', qq({"bool":{"should":[{"term":{"source.ip":"1.2.3.4"}},{"bool":{"must":[{"term":{"source.ip":"2.3.4.5"}},{"term":{"source.port":"80"}}]}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}}]}}));
doTest('ip.src == [$ipshortcut1,1.2.3.4,$ipshortcut2]', qq({"bool":{"should":[{"term":{"source.ip":"1.2.3.4"}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}}]}}));

doTest('ip.src != [1.2.3.4,$ipshortcut1]', qq({"bool":{"must_not":[{"term":{"source.ip":"1.2.3.4"}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}}]}}));
doTest('ip.src != [$ipshortcut1,1.2.3.4]', qq({"bool":{"must_not":[{"term":{"source.ip":"1.2.3.4"}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}}]}}));
doTest('ip.src != [1.2.3.4,$ipshortcut1,2.3.4.5:80]', qq({"bool":{"must_not":[{"term":{"source.ip":"1.2.3.4"}},{"bool":{"must":[{"term":{"source.ip":"2.3.4.5"}},{"term":{"source.port":"80"}}]}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}}]}}));
doTest('ip.src != [$ipshortcut1,1.2.3.4,$ipshortcut2]', qq({"bool":{"must_not":[{"term":{"source.ip":"1.2.3.4"}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}}]}}));

#### IP

doTest('ip == 1.2.3.4', '{"bool":{"should":[{"term":{"greIp":"1.2.3.4"}},{"term":{"dns.ip":"1.2.3.4"}},{"term":{"dnsipall":"1.2.3.4"}},{"term":{"dns.mailserverIp":"1.2.3.4"}},{"term":{"dns.nameserverIp":"1.2.3.4"}},{"term":{"destination.ip":"1.2.3.4"}},{"term":{"email.ip":"1.2.3.4"}},{"term":{"socks.ip":"1.2.3.4"}},{"term":{"source.ip":"1.2.3.4"}},{"term":{"http.xffIp":"1.2.3.4"}},{"term":{"radius.endpointIp":"1.2.3.4"}},{"term":{"radius.framedIp":"1.2.3.4"}},{"term":{"test.ip":"1.2.3.4"}}]}}');
doTest('ip != 1.2.3.4', '{"bool":{"must_not":[{"term":{"greIp":"1.2.3.4"}},{"term":{"dns.ip":"1.2.3.4"}},{"term":{"dnsipall":"1.2.3.4"}},{"term":{"dns.mailserverIp":"1.2.3.4"}},{"term":{"dns.nameserverIp":"1.2.3.4"}},{"term":{"destination.ip":"1.2.3.4"}},{"term":{"email.ip":"1.2.3.4"}},{"term":{"socks.ip":"1.2.3.4"}},{"term":{"source.ip":"1.2.3.4"}},{"term":{"http.xffIp":"1.2.3.4"}},{"term":{"radius.endpointIp":"1.2.3.4"}},{"term":{"radius.framedIp":"1.2.3.4"}},{"term":{"test.ip":"1.2.3.4"}}]}}');

doTest('ip == 1.2.3.4:80', '{"bool":{"should":[{"bool":{"must":[{"term":{"destination.ip":"1.2.3.4"}},{"term":{"destination.port":"80"}}]}},{"bool":{"must":[{"term":{"socks.ip":"1.2.3.4"}},{"term":{"socks.port":"80"}}]}},{"bool":{"must":[{"term":{"source.ip":"1.2.3.4"}},{"term":{"source.port":"80"}}]}}]}}');
doTest('ip != 1.2.3.4:80', '{"bool":{"must_not":[{"bool":{"must":[{"term":{"destination.ip":"1.2.3.4"}},{"term":{"destination.port":"80"}}]}},{"bool":{"must":[{"term":{"socks.ip":"1.2.3.4"}},{"term":{"socks.port":"80"}}]}},{"bool":{"must":[{"term":{"source.ip":"1.2.3.4"}},{"term":{"source.port":"80"}}]}}]}}');

doTest('ip == [1.2.3.4]', '{"bool":{"should":[{"term":{"greIp":"1.2.3.4"}},{"term":{"dns.ip":"1.2.3.4"}},{"term":{"dnsipall":"1.2.3.4"}},{"term":{"dns.mailserverIp":"1.2.3.4"}},{"term":{"dns.nameserverIp":"1.2.3.4"}},{"term":{"destination.ip":"1.2.3.4"}},{"term":{"email.ip":"1.2.3.4"}},{"term":{"socks.ip":"1.2.3.4"}},{"term":{"source.ip":"1.2.3.4"}},{"term":{"http.xffIp":"1.2.3.4"}},{"term":{"radius.endpointIp":"1.2.3.4"}},{"term":{"radius.framedIp":"1.2.3.4"}},{"term":{"test.ip":"1.2.3.4"}}]}}');
doTest('ip != [1.2.3.4]', '{"bool":{"must_not":[{"term":{"greIp":"1.2.3.4"}},{"term":{"dns.ip":"1.2.3.4"}},{"term":{"dnsipall":"1.2.3.4"}},{"term":{"dns.mailserverIp":"1.2.3.4"}},{"term":{"dns.nameserverIp":"1.2.3.4"}},{"term":{"destination.ip":"1.2.3.4"}},{"term":{"email.ip":"1.2.3.4"}},{"term":{"socks.ip":"1.2.3.4"}},{"term":{"source.ip":"1.2.3.4"}},{"term":{"http.xffIp":"1.2.3.4"}},{"term":{"radius.endpointIp":"1.2.3.4"}},{"term":{"radius.framedIp":"1.2.3.4"}},{"term":{"test.ip":"1.2.3.4"}}]}}');

doTest('ip == [1.2.3.4,2.3.4.5:80,:81,1.2.3:82]', '{"bool":{"should":[{"term":{"greIp":"1.2.3.4"}},{"term":{"dns.ip":"1.2.3.4"}},{"term":{"dnsipall":"1.2.3.4"}},{"term":{"dns.mailserverIp":"1.2.3.4"}},{"term":{"dns.nameserverIp":"1.2.3.4"}},{"term":{"destination.ip":"1.2.3.4"}},{"term":{"email.ip":"1.2.3.4"}},{"term":{"socks.ip":"1.2.3.4"}},{"term":{"source.ip":"1.2.3.4"}},{"term":{"http.xffIp":"1.2.3.4"}},{"term":{"radius.endpointIp":"1.2.3.4"}},{"term":{"radius.framedIp":"1.2.3.4"}},{"term":{"test.ip":"1.2.3.4"}},{"bool":{"must":[{"term":{"destination.ip":"2.3.4.5"}},{"term":{"destination.port":"80"}}]}},{"bool":{"must":[{"term":{"socks.ip":"2.3.4.5"}},{"term":{"socks.port":"80"}}]}},{"bool":{"must":[{"term":{"source.ip":"2.3.4.5"}},{"term":{"source.port":"80"}}]}},{"term":{"destination.port":"81"}},{"term":{"socks.port":"81"}},{"term":{"source.port":"81"}},{"bool":{"must":[{"term":{"destination.ip":"1.2.3.0/24"}},{"term":{"destination.port":"82"}}]}},{"bool":{"must":[{"term":{"socks.ip":"1.2.3.0/24"}},{"term":{"socks.port":"82"}}]}},{"bool":{"must":[{"term":{"source.ip":"1.2.3.0/24"}},{"term":{"source.port":"82"}}]}}]}}');
doTest('ip != [1.2.3.4,2.3.4.5:80,:81,1.2.3:82]', '{"bool":{"must_not":[{"term":{"greIp":"1.2.3.4"}},{"term":{"dns.ip":"1.2.3.4"}},{"term":{"dnsipall":"1.2.3.4"}},{"term":{"dns.mailserverIp":"1.2.3.4"}},{"term":{"dns.nameserverIp":"1.2.3.4"}},{"term":{"destination.ip":"1.2.3.4"}},{"term":{"email.ip":"1.2.3.4"}},{"term":{"socks.ip":"1.2.3.4"}},{"term":{"source.ip":"1.2.3.4"}},{"term":{"http.xffIp":"1.2.3.4"}},{"term":{"radius.endpointIp":"1.2.3.4"}},{"term":{"radius.framedIp":"1.2.3.4"}},{"term":{"test.ip":"1.2.3.4"}},{"bool":{"must":[{"term":{"destination.ip":"2.3.4.5"}},{"term":{"destination.port":"80"}}]}},{"bool":{"must":[{"term":{"socks.ip":"2.3.4.5"}},{"term":{"socks.port":"80"}}]}},{"bool":{"must":[{"term":{"source.ip":"2.3.4.5"}},{"term":{"source.port":"80"}}]}},{"term":{"destination.port":"81"}},{"term":{"socks.port":"81"}},{"term":{"source.port":"81"}},{"bool":{"must":[{"term":{"destination.ip":"1.2.3.0/24"}},{"term":{"destination.port":"82"}}]}},{"bool":{"must":[{"term":{"socks.ip":"1.2.3.0/24"}},{"term":{"socks.port":"82"}}]}},{"bool":{"must":[{"term":{"source.ip":"1.2.3.0/24"}},{"term":{"source.port":"82"}}]}}]}}');

doTest('ip == $ipshortcut1', qq({"bool":{"should":[{"terms":{"greIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"dns.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"dnsipall":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"dns.mailserverIp":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"dns.nameserverIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"destination.ip":{"path":"ip","id":"$ipshortcut1","index":"tests_lookups"}}},{"terms":{"email.ip":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"socks.ip":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"source.ip":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"http.xffIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"radius.endpointIp":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"radius.framedIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"test.ip":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}}]}}));
doTest('ip != $ipshortcut1', qq({"bool":{"must_not":[{"terms":{"greIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"dns.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"dnsipall":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"dns.mailserverIp":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"dns.nameserverIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"destination.ip":{"path":"ip","id":"$ipshortcut1","index":"tests_lookups"}}},{"terms":{"email.ip":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"socks.ip":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"source.ip":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"http.xffIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"radius.endpointIp":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"radius.framedIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"test.ip":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}}]}}));

doTest('ip == [$ipshortcut1]', qq({"bool":{"should":[{"terms":{"greIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"dns.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"dnsipall":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"dns.mailserverIp":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"dns.nameserverIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"destination.ip":{"path":"ip","id":"$ipshortcut1","index":"tests_lookups"}}},{"terms":{"email.ip":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"socks.ip":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"source.ip":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"http.xffIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"radius.endpointIp":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"radius.framedIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"test.ip":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}}]}}));
doTest('ip != [$ipshortcut1]', qq({"bool":{"must_not":[{"terms":{"greIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"dns.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"dnsipall":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"dns.mailserverIp":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"dns.nameserverIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"destination.ip":{"path":"ip","id":"$ipshortcut1","index":"tests_lookups"}}},{"terms":{"email.ip":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"socks.ip":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"source.ip":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"http.xffIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"radius.endpointIp":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"radius.framedIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"test.ip":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}}]}}));

doTest('ip == [$ipshortcut1,$ipshortcut2]', qq({"bool":{"should":[{"terms":{"greIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"dns.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"dnsipall":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"dns.mailserverIp":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"dns.nameserverIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"destination.ip":{"path":"ip","id":"$ipshortcut1","index":"tests_lookups"}}},{"terms":{"email.ip":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"socks.ip":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"source.ip":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"http.xffIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"radius.endpointIp":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"radius.framedIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"test.ip":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"greIp":{"id":"$ipshortcut2","index":"tests_lookups","path":"ip"}}},{"terms":{"dns.ip":{"id":"$ipshortcut2","index":"tests_lookups","path":"ip"}}},{"terms":{"dnsipall":{"id":"$ipshortcut2","index":"tests_lookups","path":"ip"}}},{"terms":{"dns.mailserverIp":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}},{"terms":{"dns.nameserverIp":{"id":"$ipshortcut2","index":"tests_lookups","path":"ip"}}},{"terms":{"destination.ip":{"id":"$ipshortcut2","path":"ip","index":"tests_lookups"}}},{"terms":{"email.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}},{"terms":{"socks.ip":{"path":"ip","index":"tests_lookups","id":"$ipshortcut2"}}},{"terms":{"source.ip":{"id":"$ipshortcut2","index":"tests_lookups","path":"ip"}}},{"terms":{"http.xffIp":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}},{"terms":{"radius.endpointIp":{"id":"$ipshortcut2","path":"ip","index":"tests_lookups"}}},{"terms":{"radius.framedIp":{"id":"$ipshortcut2","path":"ip","index":"tests_lookups"}}},{"terms":{"test.ip":{"id":"$ipshortcut2","path":"ip","index":"tests_lookups"}}}]}}));
doTest('ip != [$ipshortcut1,$ipshortcut2]', qq({"bool":{"must_not":[{"terms":{"greIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"dns.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"dnsipall":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"dns.mailserverIp":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"dns.nameserverIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"destination.ip":{"path":"ip","id":"$ipshortcut1","index":"tests_lookups"}}},{"terms":{"email.ip":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"socks.ip":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"source.ip":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"http.xffIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"radius.endpointIp":{"id":"$ipshortcut1","path":"ip","index":"tests_lookups"}}},{"terms":{"radius.framedIp":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"test.ip":{"index":"tests_lookups","id":"$ipshortcut1","path":"ip"}}},{"terms":{"greIp":{"id":"$ipshortcut2","index":"tests_lookups","path":"ip"}}},{"terms":{"dns.ip":{"id":"$ipshortcut2","index":"tests_lookups","path":"ip"}}},{"terms":{"dnsipall":{"id":"$ipshortcut2","index":"tests_lookups","path":"ip"}}},{"terms":{"dns.mailserverIp":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}},{"terms":{"dns.nameserverIp":{"id":"$ipshortcut2","index":"tests_lookups","path":"ip"}}},{"terms":{"destination.ip":{"id":"$ipshortcut2","path":"ip","index":"tests_lookups"}}},{"terms":{"email.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}},{"terms":{"socks.ip":{"path":"ip","index":"tests_lookups","id":"$ipshortcut2"}}},{"terms":{"source.ip":{"id":"$ipshortcut2","index":"tests_lookups","path":"ip"}}},{"terms":{"http.xffIp":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}},{"terms":{"radius.endpointIp":{"id":"$ipshortcut2","path":"ip","index":"tests_lookups"}}},{"terms":{"radius.framedIp":{"id":"$ipshortcut2","path":"ip","index":"tests_lookups"}}},{"terms":{"test.ip":{"id":"$ipshortcut2","path":"ip","index":"tests_lookups"}}}]}}));

# Delete shortcuts
esPost("/tests_lookups/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }');
