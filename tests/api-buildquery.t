use Test::More tests => 32;
use Cwd;
use URI::Escape;
use MolochTest;
use Data::Dumper;
use JSON;
use Test::Differences;
use strict;

my $json;

sub doTest {
my ($expression, $expected) = @_;
    local $Test::Builder::Level = $Test::Builder::Level + 1;
    $json = viewerGet('/api/buildquery?date=-1&expression=' . uri_escape($expression));
    eq_or_diff($json->{esquery}->{query}->{bool}->{filter}[0], from_json($expected), {context => 3});
    #    diag to_json($json->{esquery}->{query}->{bool}->{filter}[0]);
}

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


#### IP

doTest('ip == 1.2.3.4', '{"bool":{"should":[{"term":{"dnsipall":"1.2.3.4"}},{"term":{"greIp":"1.2.3.4"}},{"term":{"dns.ip":"1.2.3.4"}},{"term":{"http.xffIp":"1.2.3.4"}},{"term":{"radius.framedIp":"1.2.3.4"}},{"term":{"email.ip":"1.2.3.4"}},{"term":{"dns.mailserverIp":"1.2.3.4"}},{"term":{"destination.ip":"1.2.3.4"}},{"term":{"source.ip":"1.2.3.4"}},{"term":{"dns.nameserverIp":"1.2.3.4"}},{"term":{"test.ip":"1.2.3.4"}},{"term":{"socks.ip":"1.2.3.4"}},{"term":{"radius.endpointIp":"1.2.3.4"}}]}}');
doTest('ip != 1.2.3.4', '{"bool":{"must_not":[{"term":{"dnsipall":"1.2.3.4"}},{"term":{"greIp":"1.2.3.4"}},{"term":{"dns.ip":"1.2.3.4"}},{"term":{"http.xffIp":"1.2.3.4"}},{"term":{"radius.framedIp":"1.2.3.4"}},{"term":{"email.ip":"1.2.3.4"}},{"term":{"dns.mailserverIp":"1.2.3.4"}},{"term":{"destination.ip":"1.2.3.4"}},{"term":{"source.ip":"1.2.3.4"}},{"term":{"dns.nameserverIp":"1.2.3.4"}},{"term":{"test.ip":"1.2.3.4"}},{"term":{"socks.ip":"1.2.3.4"}},{"term":{"radius.endpointIp":"1.2.3.4"}}]}}');

doTest('ip == 1.2.3.4:80', '{"bool":{"should":[{"bool":{"must":[{"term":{"destination.ip":"1.2.3.4"}},{"term":{"destination.port":"80"}}]}},{"bool":{"must":[{"term":{"source.ip":"1.2.3.4"}},{"term":{"source.port":"80"}}]}},{"bool":{"must":[{"term":{"socks.ip":"1.2.3.4"}},{"term":{"socks.port":"80"}}]}}]}}');
doTest('ip != 1.2.3.4:80', '{"bool":{"must_not":[{"bool":{"must":[{"term":{"destination.ip":"1.2.3.4"}},{"term":{"destination.port":"80"}}]}},{"bool":{"must":[{"term":{"source.ip":"1.2.3.4"}},{"term":{"source.port":"80"}}]}},{"bool":{"must":[{"term":{"socks.ip":"1.2.3.4"}},{"term":{"socks.port":"80"}}]}}]}}');

doTest('ip == [1.2.3.4]', '{"bool":{"should":[{"term":{"dnsipall":"1.2.3.4"}},{"term":{"greIp":"1.2.3.4"}},{"term":{"dns.ip":"1.2.3.4"}},{"term":{"http.xffIp":"1.2.3.4"}},{"term":{"radius.framedIp":"1.2.3.4"}},{"term":{"email.ip":"1.2.3.4"}},{"term":{"dns.mailserverIp":"1.2.3.4"}},{"term":{"destination.ip":"1.2.3.4"}},{"term":{"source.ip":"1.2.3.4"}},{"term":{"dns.nameserverIp":"1.2.3.4"}},{"term":{"test.ip":"1.2.3.4"}},{"term":{"socks.ip":"1.2.3.4"}},{"term":{"radius.endpointIp":"1.2.3.4"}}]}}');
doTest('ip != [1.2.3.4]', '{"bool":{"must_not":[{"term":{"dnsipall":"1.2.3.4"}},{"term":{"greIp":"1.2.3.4"}},{"term":{"dns.ip":"1.2.3.4"}},{"term":{"http.xffIp":"1.2.3.4"}},{"term":{"radius.framedIp":"1.2.3.4"}},{"term":{"email.ip":"1.2.3.4"}},{"term":{"dns.mailserverIp":"1.2.3.4"}},{"term":{"destination.ip":"1.2.3.4"}},{"term":{"source.ip":"1.2.3.4"}},{"term":{"dns.nameserverIp":"1.2.3.4"}},{"term":{"test.ip":"1.2.3.4"}},{"term":{"socks.ip":"1.2.3.4"}},{"term":{"radius.endpointIp":"1.2.3.4"}}]}}');

doTest('ip == [1.2.3.4,2.3.4.5:80,:81,1.2.3:82]', '{"bool":{"should":[{"term":{"dnsipall":"1.2.3.4"}},{"term":{"greIp":"1.2.3.4"}},{"term":{"dns.ip":"1.2.3.4"}},{"term":{"http.xffIp":"1.2.3.4"}},{"term":{"radius.framedIp":"1.2.3.4"}},{"term":{"email.ip":"1.2.3.4"}},{"term":{"dns.mailserverIp":"1.2.3.4"}},{"term":{"destination.ip":"1.2.3.4"}},{"term":{"source.ip":"1.2.3.4"}},{"term":{"dns.nameserverIp":"1.2.3.4"}},{"term":{"test.ip":"1.2.3.4"}},{"term":{"socks.ip":"1.2.3.4"}},{"term":{"radius.endpointIp":"1.2.3.4"}},{"bool":{"must":[{"term":{"destination.ip":"2.3.4.5"}},{"term":{"destination.port":"80"}}]}},{"bool":{"must":[{"term":{"source.ip":"2.3.4.5"}},{"term":{"source.port":"80"}}]}},{"bool":{"must":[{"term":{"socks.ip":"2.3.4.5"}},{"term":{"socks.port":"80"}}]}},{"term":{"destination.port":"81"}},{"term":{"source.port":"81"}},{"term":{"socks.port":"81"}},{"bool":{"must":[{"term":{"destination.ip":"1.2.3.0/24"}},{"term":{"destination.port":"82"}}]}},{"bool":{"must":[{"term":{"source.ip":"1.2.3.0/24"}},{"term":{"source.port":"82"}}]}},{"bool":{"must":[{"term":{"socks.ip":"1.2.3.0/24"}},{"term":{"socks.port":"82"}}]}}]}}');
doTest('ip != [1.2.3.4,2.3.4.5:80,:81,1.2.3:82]', '{"bool":{"must_not":[{"term":{"dnsipall":"1.2.3.4"}},{"term":{"greIp":"1.2.3.4"}},{"term":{"dns.ip":"1.2.3.4"}},{"term":{"http.xffIp":"1.2.3.4"}},{"term":{"radius.framedIp":"1.2.3.4"}},{"term":{"email.ip":"1.2.3.4"}},{"term":{"dns.mailserverIp":"1.2.3.4"}},{"term":{"destination.ip":"1.2.3.4"}},{"term":{"source.ip":"1.2.3.4"}},{"term":{"dns.nameserverIp":"1.2.3.4"}},{"term":{"test.ip":"1.2.3.4"}},{"term":{"socks.ip":"1.2.3.4"}},{"term":{"radius.endpointIp":"1.2.3.4"}},{"bool":{"must":[{"term":{"destination.ip":"2.3.4.5"}},{"term":{"destination.port":"80"}}]}},{"bool":{"must":[{"term":{"source.ip":"2.3.4.5"}},{"term":{"source.port":"80"}}]}},{"bool":{"must":[{"term":{"socks.ip":"2.3.4.5"}},{"term":{"socks.port":"80"}}]}},{"term":{"destination.port":"81"}},{"term":{"source.port":"81"}},{"term":{"socks.port":"81"}},{"bool":{"must":[{"term":{"destination.ip":"1.2.3.0/24"}},{"term":{"destination.port":"82"}}]}},{"bool":{"must":[{"term":{"source.ip":"1.2.3.0/24"}},{"term":{"source.port":"82"}}]}},{"bool":{"must":[{"term":{"socks.ip":"1.2.3.0/24"}},{"term":{"socks.port":"82"}}]}}]}}');
