use Test::More tests => 30;
use Cwd;
use URI::Escape;
use MolochTest;
use Test::Differences;
use strict;


sub get {
my ($param, $multi) = @_;

    my $txt;
    if ($multi) {
        $txt = $MolochTest::userAgent->get("http://$MolochTest::host:8125/unique.txt?$param")->content;
    } else {
        $txt = $MolochTest::userAgent->get("http://$MolochTest::host:8123/unique.txt?$param")->content;
    }
    my @lines = split(/\n/, $txt);

    # Sort since the server returns any order with the same counts
    @lines = sort @lines;
    return join("\n", @lines) . "\n";
}

my $pwd = "*/pcap";
my $filestr = "(file=$pwd/socks-http-example.pcap||file=$pwd/socks-http-pass.pcap||file=$pwd/socks-https-example.pcap||file=$pwd/socks5-http-302.pcap||file=$pwd/socks5-rdp.pcap||file=$pwd/socks5-reverse.pcap||file=$pwd/socks5-smtp-503.pcap||file=$pwd/v6-http.pcap)";
my $files = uri_escape($filestr);



#
my $txt = get("");
my $mtxt = get("", 1);
is ($txt, "Missing field or exp parameter\n", "unique.txt node field parameter");
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });


#
$txt = get("date=-1&field=node");
$mtxt = get("date=-1&field=node", 1);
eq_or_diff($txt, "test\n", "Nodes", { context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });

#
$txt = get("date=-1&field=node&autocomplete=1&expression=" . uri_escape("node=te*"));
$mtxt = get("date=-1&field=node&autocomplete=1&expression=" . uri_escape("node=te*"), 1);
eq_or_diff($txt, "[\"test\"]\n", "Autocomplete Nodes", { context => 3 });
eq_or_diff($mtxt, "[]\n", "Multi Autocomplete Nodes", { context => 3 });

#
$txt = get("date=-1&field=node&expression=$files&counts=1");
$mtxt = get("date=-1&field=node&expression=$files&counts=1", 1);
eq_or_diff($txt, "test, 19\n", "Nodes count", { context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });

#
$txt = get("date=-1&field=srcIp&expression=$files&counts=1");
$mtxt = get("date=-1&field=srcIp&expression=$files&counts=1", 1);
eq_or_diff($txt,
"10.0.0.1, 2
10.0.0.2, 1
10.0.0.3, 1
10.180.156.185, 9
2001:6f8:102d:0:1033:c4c:7e57:b19e, 1
2001:6f8:102d:0:2d0:9ff:fee3:e8de, 1
::, 1
fe80::211:25ff:fe82:95b5, 2
fe80::2d0:9ff:fee3:e8de, 1
", "ip count", { context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });

#
$txt = get("date=-1&field=srcIp&autocomplete=1&expression=" . uri_escape("$filestr && ip.src=10.180"));
$mtxt = get("date=-1&field=srcIp&autocomplete=1&expression=" . uri_escape("$filestr && ip.src=10.180"), 1);
eq_or_diff($txt, "[\"10.180.156.185\"]\n", "Autocomplete IPs", { context => 3 });
eq_or_diff($mtxt, "[]\n", "Multi Autocomplete IPs", { context => 3 });

#
$txt = get("date=-1&field=tags&expression=$files&counts=1");
$mtxt = get("date=-1&field=tags&expression=$files&counts=1", 1);
eq_or_diff($txt,
"byhost2, 7
byip1, 1
byip2, 1
domainwise, 7
dstip, 4
hosttaggertest1, 7
hosttaggertest2, 7
iptaggertest1, 2
iptaggertest2, 2
ipwise, 3
ipwisecsv, 4
smtp:authlogin, 1
socks:password, 2
srcip, 4
wisebyhost2, 7
wisebyip1, 1
wisebyip3, 2
", "tags count", { context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });

#
$txt = get("date=-1&field=http.requestHeader&expression=$files&counts=1");
$mtxt = get("date=-1&field=http.requestHeader&expression=$files&counts=1", 1);

SKIP: {
    skip "Upgrade test", 1 if ($ENV{MOLOCH_REINDEX_TEST}); # reindex doesn't have http.has-header
eq_or_diff($txt,
"accept, 7
accept-encoding, 3
accept-language, 2
connection, 1
cookie, 2
host, 7
referer, 1
user-agent, 7
", "http header count", { context => 3 });
}
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });

#
$txt = get("date=-1&field=http.md5&expression=$files");
$mtxt = get("date=-1&field=http.md5&expression=$files", 1);
eq_or_diff($txt,
"09b9c392dc1f6e914cea287cb6be34b0
2069181ae704855f29caf964ca52ec49
222315d36e1313774cb1c2f0eb06864f
27cb95a0c4fff954073bc23328021b96
b0cecae354b9eab1f04f70e46a612cb1
", "http md5", { context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });

#
$txt = get("date=-1&field=http.md5&autocomplete=1&expression=" . uri_escape("$filestr && http.md5=2*"));
$mtxt = get("date=-1&field=http.md5&autocomplete=1&expression=" . uri_escape("$filestr && http.md5=2*"), 1);
eq_or_diff($txt, "[\"2069181ae704855f29caf964ca52ec49\",\"222315d36e1313774cb1c2f0eb06864f\",\"27cb95a0c4fff954073bc23328021b96\",\"b0cecae354b9eab1f04f70e46a612cb1\"]\n", "Autocomplete HTTP md5s", { context => 3 });
eq_or_diff($mtxt, "[]\n", "Multi Autocomplete HTTP md5s", { context => 3 });

#
$txt = get("date=-1&field=http.md5&expression=$files&counts=1");
$mtxt = get("date=-1&field=http.md5&expression=$files&counts=1", 1);
eq_or_diff($txt,
"09b9c392dc1f6e914cea287cb6be34b0, 4
2069181ae704855f29caf964ca52ec49, 1
222315d36e1313774cb1c2f0eb06864f, 1
27cb95a0c4fff954073bc23328021b96, 1
b0cecae354b9eab1f04f70e46a612cb1, 1
", "http md5 count", { context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });

#
$txt = get("date=-1&field=http.uri&expression=$files&counts=0");
$mtxt = get("date=-1&field=http.uri&expression=$files&counts=0", 1);
eq_or_diff($txt,
"cl-1985.ham-01.de.sixxs.net/
www.example.com/
www.google.com/
www.google.com/search?client=firefox&rls=en&q=Sheepskin%20Boots&start=0&num=10&hl=en&gl=us&uule=xxxxxxxxxxxxxxxxxxxxxxxxxxxx
www.google.com/search?client=firefox&rls=en&q=Sheepskin%20Boots&start=10&num=10&hl=en&gl=us&uule=xxxxxxxxxxxxxxxxxxxxxxxxxxxx
", "http uri", { context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });

#
$txt = get("date=-1&field=http.uri&expression=$files&counts=1");
$mtxt = get("date=-1&field=http.uri&expression=$files&counts=1", 1);
eq_or_diff($txt,
"cl-1985.ham-01.de.sixxs.net/, 1
www.example.com/, 4
www.google.com/, 1
www.google.com/search?client=firefox&rls=en&q=Sheepskin%20Boots&start=0&num=10&hl=en&gl=us&uule=xxxxxxxxxxxxxxxxxxxxxxxxxxxx, 1
www.google.com/search?client=firefox&rls=en&q=Sheepskin%20Boots&start=10&num=10&hl=en&gl=us&uule=xxxxxxxxxxxxxxxxxxxxxxxxxxxx, 1
", "http uri", { context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });

#
$txt = get("date=-1&field=http.useragent&expression=$files&counts=0");
$mtxt = get("date=-1&field=http.useragent&expression=$files&counts=0", 1);
eq_or_diff($txt,
"Lynx/2.8.6rel.2 libwww-FM/2.14 SSL-MM/1.4.1 OpenSSL/0.9.8b
Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322)
Mozilla/5.0 (Windows NT 5.1; rv:25.0) Gecko/20100101 Firefox/25.0
curl/7.24.0 (x86_64-apple-darwin12.0) libcurl/7.24.0 OpenSSL/0.9.8y zlib/1.2.5
", "http user agent", {context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });

#
$txt = get("date=-1&field=ip.src:srcPort&expression=$files&counts=1");
$mtxt = get("date=-1&field=ip.src:srcPort&expression=$files&counts=1", 1);
eq_or_diff($txt,
"10.0.0.1:1637, 1
10.0.0.1:54263, 1
10.0.0.2:53709, 1
10.0.0.3:2276, 1
10.180.156.185:53533, 1
10.180.156.185:53534, 1
10.180.156.185:53535, 1
10.180.156.185:53554, 1
10.180.156.185:53555, 1
10.180.156.185:53556, 1
10.180.156.185:54068, 1
10.180.156.185:54069, 1
10.180.156.185:54072, 1
2001:6f8:102d:0:1033:c4c:7e57:b19e.5353, 1
2001:6f8:102d:0:2d0:9ff:fee3:e8de.59201, 1
::.0, 1
fe80::211:25ff:fe82:95b5.0, 2
fe80::2d0:9ff:fee3:e8de.0, 1
", "ip count", { context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });
