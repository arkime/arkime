use Test::More tests => 154;
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
    if (exists $json->{error}) {
        is($json->{error}, $expected);
        diag to_json($json) if ($debug);
    } else {
        eq_or_diff($json->{esquery}->{query}->{bool}->{filter}[0], from_json($expected), {context => 3});
        diag to_json($json->{esquery}->{query}->{bool}->{filter}[0]) if ($debug);
    }
}

# Create shortcuts for testing
esPost("/tests_lookups/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }');
my $token = getTokenCookie();
my $ipshortcut1 = viewerPostToken("/api/shortcut", '{"name":"ipshortcut1","type":"ip","value":"10.10.10.10"}', $token)->{shortcut}->{id};
my $ipshortcut2 = viewerPostToken("/api/shortcut", '{"name":"ipshortcut2","type":"ip","value":"10.10.10.10"}', $token)->{shortcut}->{id};
my $ipshort3 = viewerPostToken("/api/shortcut", '{"name":"ipshort3","type":"ip","value":"10.10.10.10"}', $token)->{shortcut}->{id};

#### IP.SRC
doTest('ip.src == 1.2.3.4', '{"term":{"source.ip":"1.2.3.4"}}');
doTest('ip.src != 1.2.3.4', '{"bool":{"must_not":{"term":{"source.ip":"1.2.3.4"}}}}');

doTest('ip.src == [1.2.3.4]', '{"term":{"source.ip":"1.2.3.4"}}');
doTest('ip.src != [1.2.3.4]', '{"bool":{"must_not":{"term":{"source.ip":"1.2.3.4"}}}}');

doTest('ip.src == ]1.2.3.4[', '{"term":{"source.ip":"1.2.3.4"}}');
doTest('ip.src != ]1.2.3.4[', '{"bool":{"must_not":{"term":{"source.ip":"1.2.3.4"}}}}');

doTest('ip.src == [1.2.3.4,2.3.4.5]', '{"bool":{"should":[{"term":{"source.ip":"1.2.3.4"}},{"term":{"source.ip":"2.3.4.5"}}]}}');
doTest('ip.src != [1.2.3.4,2.3.4.5]', '{"bool":{"must_not":[{"term":{"source.ip":"1.2.3.4"}},{"term":{"source.ip":"2.3.4.5"}}]}}');

doTest('ip.src == ]1.2.3.4,2.3.4.5[', '{"bool":{"must":[{"term":{"source.ip":"1.2.3.4"}},{"term":{"source.ip":"2.3.4.5"}}]}}');
doTest('ip.src != ]1.2.3.4,2.3.4.5[', '{"bool":{"must_not":{"bool":{"must":[{"term":{"source.ip":"1.2.3.4"}},{"term":{"source.ip":"2.3.4.5"}}]}}}}');

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

doTest('ip.src == 1.2.3.4 || ip.src == 2.3.4.5:80 || ip.src == :81 || ip.src == 1.2.3:82', '{"bool":{"should":[{"term":{"source.ip":"1.2.3.4"}},{"bool":{"must":[{"term":{"source.ip":"2.3.4.5"}},{"term":{"source.port":"80"}}]}},{"term":{"source.port":"81"}},{"bool":{"must":[{"term":{"source.ip":"1.2.3.0/24"}},{"term":{"source.port":"82"}}]}}]}}');
doTest('ip.src == 1.2.3.4 && ip.src == 2.3.4.5:80 && ip.src == :81 && ip.src == 1.2.3:82', '{"bool":{"must":[{"term":{"source.ip":"1.2.3.4"}},{"term":{"source.ip":"2.3.4.5"}},{"term":{"source.port":"80"}},{"term":{"source.port":"81"}},{"term":{"source.ip":"1.2.3.0/24"}},{"term":{"source.port":"82"}}]}}');
doTest('!(ip.src == 1.2.3.4 || ip.src == 2.3.4.5:80 || ip.src == :81 || ip.src == 1.2.3:82)', '{"bool":{"must_not":[{"term":{"source.ip":"1.2.3.4"}},{"bool":{"must":[{"term":{"source.ip":"2.3.4.5"}},{"term":{"source.port":"80"}}]}},{"term":{"source.port":"81"}},{"bool":{"must":[{"term":{"source.ip":"1.2.3.0/24"}},{"term":{"source.port":"82"}}]}}]}}');
doTest('!(ip.src == 1.2.3.4 && ip.src == 2.3.4.5:80 && ip.src == :81 && ip.src == 1.2.3:82)', '{"bool":{"must_not":{"bool":{"must":[{"term":{"source.ip":"1.2.3.4"}},{"term":{"source.ip":"2.3.4.5"}},{"term":{"source.port":"80"}},{"term":{"source.port":"81"}},{"term":{"source.ip":"1.2.3.0/24"}},{"term":{"source.port":"82"}}]}}}}');

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

doTest('ip.src != [$ipshortcut?]', qq({"bool":{"must_not":[{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}}]}}));
doTest('ip.src == $ip*cut*', qq({"bool":{"should":[{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}}]}}));
doTest('ip.src == $ip*cut?', qq({"bool":{"should":[{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}}]}}));
doTest('ip.src == $*cut*', qq({"bool":{"should":[{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}}]}}));

doTest('ip.src == [1.2.3.4,$ipshortcut1]', qq({"bool":{"should":[{"term":{"source.ip":"1.2.3.4"}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}}]}}));
doTest('ip.src == [$ipshortcut1,1.2.3.4]', qq({"bool":{"should":[{"term":{"source.ip":"1.2.3.4"}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}}]}}));
doTest('ip.src == [1.2.3.4,$ipshortcut1,2.3.4.5:80]', qq({"bool":{"should":[{"term":{"source.ip":"1.2.3.4"}},{"bool":{"must":[{"term":{"source.ip":"2.3.4.5"}},{"term":{"source.port":"80"}}]}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}}]}}));
doTest('ip.src == [$ipshortcut1,1.2.3.4,$ipshortcut2]', qq({"bool":{"should":[{"term":{"source.ip":"1.2.3.4"}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}}]}}));

doTest('ip.src != [1.2.3.4,$ipshortcut1]', qq({"bool":{"must_not":[{"term":{"source.ip":"1.2.3.4"}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}}]}}));
doTest('ip.src != [$ipshortcut1,1.2.3.4]', qq({"bool":{"must_not":[{"term":{"source.ip":"1.2.3.4"}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}}]}}));
doTest('ip.src != [1.2.3.4,$ipshortcut1,2.3.4.5:80]', qq({"bool":{"must_not":[{"term":{"source.ip":"1.2.3.4"}},{"bool":{"must":[{"term":{"source.ip":"2.3.4.5"}},{"term":{"source.port":"80"}}]}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}}]}}));
doTest('ip.src != [$ipshortcut1,1.2.3.4,$ipshortcut2]', qq({"bool":{"must_not":[{"term":{"source.ip":"1.2.3.4"}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut1"}}},{"terms":{"source.ip":{"index":"tests_lookups","path":"ip","id":"$ipshortcut2"}}}]}}));

doTest('ip.src == ]$ipshortcut1[', q(]$ipshortcut1[ - AND array not supported with shortcuts));

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

#### host.http

doTest('host.http == fred', '{"term":{"http.host":"fred"}}');
doTest('host.http != fred', '{"bool":{"must_not":{"term":{"http.host":"fred"}}}}');
doTest('host.http == fred*', '{"wildcard":{"http.host":"fred*"}}');
doTest('host.http != fred*', '{"bool":{"must_not":{"wildcard":{"http.host":"fred*"}}}}');
doTest('host.http == /fred/', '{"regexp":{"http.host":"fred"}}');
doTest('host.http != /fred/', '{"bool":{"must_not":{"regexp":{"http.host":"fred"}}}}');

doTest('host.http == [/barney/,fred,fred*]', '{"bool":{"should":[{"regexp":{"http.host":"barney"}},{"terms":{"http.host":["fred"]}},{"wildcard":{"http.host":"fred*"}}]}}');
doTest('host.http == [/barney/,http://fred,fred*]', '{"bool":{"should":[{"regexp":{"http.host":"barney"}},{"terms":{"http.host":["fred"]}},{"wildcard":{"http.host":"fred*"}}]}}');
doTest('host.http == [/barney/,fred/foobar,fred*]', '{"bool":{"should":[{"regexp":{"http.host":"barney"}},{"terms":{"http.host":["fred"]}},{"wildcard":{"http.host":"fred*"}}]}}');

doTest('host.http != [/barney/,fred,fred*]', '{"bool":{"must_not":[{"regexp":{"http.host":"barney"}},{"terms":{"http.host":["fred"]}},{"wildcard":{"http.host":"fred*"}}]}}');
doTest('host.http != [/barney/,http://fred,fred*]', '{"bool":{"must_not":[{"regexp":{"http.host":"barney"}},{"terms":{"http.host":["fred"]}},{"wildcard":{"http.host":"fred*"}}]}}');
doTest('host.http != [/barney/,fred/foobar,fred*]', '{"bool":{"must_not":[{"regexp":{"http.host":"barney"}},{"terms":{"http.host":["fred"]}},{"wildcard":{"http.host":"fred*"}}]}}');

doTest('host.http == ]/barney/,fred,fred*[', '{"bool":{"must":[{"regexp":{"http.host":"barney"}},{"term":{"http.host":"fred"}},{"wildcard":{"http.host":"fred*"}}]}}');
doTest('host.http == ]/barney/,http://fred,fred*[', '{"bool":{"must":[{"regexp":{"http.host":"barney"}},{"term":{"http.host":"fred"}},{"wildcard":{"http.host":"fred*"}}]}}');
doTest('host.http == ]/barney/,fred/foobar,fred*[', '{"bool":{"must":[{"regexp":{"http.host":"barney"}},{"term":{"http.host":"fred"}},{"wildcard":{"http.host":"fred*"}}]}}');

doTest('host.http != ]/barney/,fred,fred*[', '{"bool":{"must_not":{"bool":{"must":[{"regexp":{"http.host":"barney"}},{"term":{"http.host":"fred"}},{"wildcard":{"http.host":"fred*"}}]}}}}');
doTest('host.http != ]/barney/,http://fred,fred*[', '{"bool":{"must_not":{"bool":{"must":[{"regexp":{"http.host":"barney"}},{"term":{"http.host":"fred"}},{"wildcard":{"http.host":"fred*"}}]}}}}');
doTest('host.http != ]/barney/,fred/foobar,fred*[', '{"bool":{"must_not":{"bool":{"must":[{"regexp":{"http.host":"barney"}},{"term":{"http.host":"fred"}},{"wildcard":{"http.host":"fred*"}}]}}}}');

#### host.http.cnt
doTest('host.http.cnt == 1', '{"term":{"http.hostCnt":1}}');
doTest('host.http.cnt != 1', '{"bool":{"must_not":{"term":{"http.hostCnt":1}}}}');
doTest('host.http.cnt == [1]', '{"terms":{"http.hostCnt":[1]}}');
doTest('host.http.cnt != [1]', '{"bool":{"must_not":{"terms":{"http.hostCnt":[1]}}}}');
doTest('host.http.cnt == [1,2,3]', '{"terms":{"http.hostCnt":[1,2,3]}}');
doTest('host.http.cnt != [1,2,3]', '{"bool":{"must_not":{"terms":{"http.hostCnt":[1,2,3]}}}}');
doTest('host.http.cnt == ]1,2,3[', '{"bool":{"must":[{"term":{"http.hostCnt":1}},{"term":{"http.hostCnt":2}},{"term":{"http.hostCnt":3}}]}}');
doTest('host.http.cnt != ]1,2,3[', '{"bool":{"must_not":{"bool":{"must":[{"term":{"http.hostCnt":1}},{"term":{"http.hostCnt":2}},{"term":{"http.hostCnt":3}}]}}}}');
doTest('host.http.cnt == 1-5', '{"range":{"http.hostCnt":{"gte":1,"lte":5}}}');
doTest('host.http.cnt != -1-5', '{"bool":{"must_not":{"range":{"http.hostCnt":{"gte":-1,"lte":5}}}}}');
doTest('host.http.cnt != -10--5', '{"bool":{"must_not":{"range":{"http.hostCnt":{"gte":-10,"lte":-5}}}}}');

doTest('host.http.cnt > 1', '{"range":{"http.hostCnt":{"gt":1}}}');
doTest('host.http.cnt >= 1', '{"range":{"http.hostCnt":{"gte":1}}}');
doTest('host.http.cnt <= 1', '{"range":{"http.hostCnt":{"lte":1}}}');
doTest('host.http.cnt < 1', '{"range":{"http.hostCnt":{"lt":1}}}');
doTest('host.http.cnt > -1', '{"range":{"http.hostCnt":{"gt":-1}}}');
doTest('host.http.cnt >= -1', '{"range":{"http.hostCnt":{"gte":-1}}}');
doTest('host.http.cnt <= -1', '{"range":{"http.hostCnt":{"lte":-1}}}');
doTest('host.http.cnt < -1', '{"range":{"http.hostCnt":{"lt":-1}}}');

#### wise.float
doTest('wise.float == 1', '{"term":{"wise.float":1}}');
doTest('wise.float == 0.1', '{"term":{"wise.float":0.1}}');
doTest('wise.float == 10.1', '{"term":{"wise.float":10.1}}');
doTest('wise.float == 10.1234', '{"term":{"wise.float":10.1234}}');

doTest('wise.float == -1', '{"term":{"wise.float":-1}}');
doTest('wise.float == -0.1', '{"term":{"wise.float":-0.1}}');
doTest('wise.float == -10.1', '{"term":{"wise.float":-10.1}}');
doTest('wise.float == -10.1234', '{"term":{"wise.float":-10.1234}}');

doTest('wise.float == 1.2', '{"term":{"wise.float":1.2}}');
doTest('wise.float != 1.2', '{"bool":{"must_not":{"term":{"wise.float":1.2}}}}');
doTest('wise.float == [1.2]', '{"terms":{"wise.float":[1.2]}}');
doTest('wise.float != [1.2]', '{"bool":{"must_not":{"terms":{"wise.float":[1.2]}}}}');
doTest('wise.float == [-1,-0.2,3.2]', '{"terms":{"wise.float":[-1,-0.2,3.2]}}');
doTest('wise.float != [-1,-0.2,3.2]', '{"bool":{"must_not":{"terms":{"wise.float":[-1,-0.2,3.2]}}}}');
doTest('wise.float == ]-1,-0.2,3.2[', '{"bool":{"must":[{"term":{"wise.float":-1}},{"term":{"wise.float":-0.2}},{"term":{"wise.float":3.2}}]}}');
doTest('wise.float != ]-1,-0.2,3.2[', '{"bool":{"must_not":{"bool":{"must":[{"term":{"wise.float":-1}},{"term":{"wise.float":-0.2}},{"term":{"wise.float":3.2}}]}}}}');

doTest('wise.float == 1.2-5.2', '{"range":{"wise.float":{"gte":1.2,"lte":5.2}}}');
doTest('wise.float != -1-5.2', '{"bool":{"must_not":{"range":{"wise.float":{"gte":-1,"lte":5.2}}}}}');
doTest('wise.float != -1-5', '{"bool":{"must_not":{"range":{"wise.float":{"gte":-1,"lte":5}}}}}');
doTest('wise.float != -1.2-5', '{"bool":{"must_not":{"range":{"wise.float":{"gte":-1.2,"lte":5}}}}}');
doTest('wise.float != -1.2-5.2', '{"bool":{"must_not":{"range":{"wise.float":{"gte":-1.2,"lte":5.2}}}}}');
doTest('wise.float != -10.2--5.2', '{"bool":{"must_not":{"range":{"wise.float":{"gte":-10.2,"lte":-5.2}}}}}');

doTest('wise.float > 1', '{"range":{"wise.float":{"gt":1}}}');
doTest('wise.float >= 1', '{"range":{"wise.float":{"gte":1}}}');
doTest('wise.float <= 1', '{"range":{"wise.float":{"lte":1}}}');
doTest('wise.float < 1', '{"range":{"wise.float":{"lt":1}}}');
doTest('wise.float > -1', '{"range":{"wise.float":{"gt":-1}}}');
doTest('wise.float >= -1', '{"range":{"wise.float":{"gte":-1}}}');
doTest('wise.float <= -1', '{"range":{"wise.float":{"lte":-1}}}');
doTest('wise.float < -1', '{"range":{"wise.float":{"lt":-1}}}');

doTest('wise.float > 1.2', '{"range":{"wise.float":{"gt":1.2}}}');
doTest('wise.float >= 1.2', '{"range":{"wise.float":{"gte":1.2}}}');
doTest('wise.float <= 1.2', '{"range":{"wise.float":{"lte":1.2}}}');
doTest('wise.float < 1.2', '{"range":{"wise.float":{"lt":1.2}}}');
doTest('wise.float > -1.2', '{"range":{"wise.float":{"gt":-1.2}}}');
doTest('wise.float >= -1.2', '{"range":{"wise.float":{"gte":-1.2}}}');
doTest('wise.float <= -1.2', '{"range":{"wise.float":{"lte":-1.2}}}');
doTest('wise.float < -1.2', '{"range":{"wise.float":{"lt":-1.2}}}');

### stoptime
doTest('stoptime=="2014/02/26 10:27:57"', '{"range":{"lastPacket":{"lte":"2014-02-26T10:27:57-05:00","gte":"2014-02-26T10:27:57-05:00"}}}');
doTest('stoptime!="2014/02/26 10:27:57"', '{"bool":{"must_not":{"range":{"lastPacket":{"lte":"2014-02-26T10:27:57-05:00","gte":"2014-02-26T10:27:57-05:00"}}}}}');

doTest('stoptime>"2014/02/26 10:27:57"', '{"range":{"lastPacket":{"gt":"2014-02-26T10:27:57-05:00"}}}');
doTest('stoptime<"2014/02/26 10:27:57"', '{"range":{"lastPacket":{"lt":"2014-02-26T10:27:57-05:00"}}}');

doTest('stoptime==["2014/02/26 10:27:57", "2014-06-10T10:10:10-05:00"]', '{"bool":{"should":[{"range":{"lastPacket":{"lte":"2014-02-26T10:27:57-05:00","gte":"2014-02-26T10:27:57-05:00"}}},{"range":{"lastPacket":{"gte":"2014-06-10T11:10:10-04:00","lte":"2014-06-10T11:10:10-04:00"}}}]}}');
doTest('stoptime!=["2014/02/26 10:27:57", "2014-06-10T10:10:10-05:00"]', '{"bool":{"must_not":[{"range":{"lastPacket":{"lte":"2014-02-26T10:27:57-05:00","gte":"2014-02-26T10:27:57-05:00"}}},{"range":{"lastPacket":{"gte":"2014-06-10T11:10:10-04:00","lte":"2014-06-10T11:10:10-04:00"}}}]}}');

doTest('stoptime==]"2014/02/26 10:27:57", "2014-06-10T10:10:10-05:00"[', '{"bool":{"must":[{"range":{"lastPacket":{"lte":"2014-02-26T10:27:57-05:00","gte":"2014-02-26T10:27:57-05:00"}}},{"range":{"lastPacket":{"gte":"2014-06-10T11:10:10-04:00","lte":"2014-06-10T11:10:10-04:00"}}}]}}');
doTest('stoptime!=]"2014/02/26 10:27:57", "2014-06-10T10:10:10-05:00"[', '{"bool":{"must_not":{"bool":{"must":[{"range":{"lastPacket":{"lte":"2014-02-26T10:27:57-05:00","gte":"2014-02-26T10:27:57-05:00"}}},{"range":{"lastPacket":{"gte":"2014-06-10T11:10:10-04:00","lte":"2014-06-10T11:10:10-04:00"}}}]}}}}');
