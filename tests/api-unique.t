use Test::More tests => 28;
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

my $pwd = getcwd() . "/pcap";
my $filestr = "(file=$pwd/socks-http-example.pcap||file=$pwd/socks-http-pass.pcap||file=$pwd/socks-https-example.pcap||file=$pwd/socks5-http-302.pcap||file=$pwd/socks5-rdp.pcap||file=$pwd/socks5-reverse.pcap||file=$pwd/socks5-smtp-503.pcap)";
my $files = uri_escape($filestr);



#
my $txt = get("");
my $mtxt = get("", 1);
is ($txt, "Missing field or exp parameter\n", "unique.txt no field parameter");
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });


#
$txt = get("date=-1&field=no");
$mtxt = get("date=-1&field=no", 1);
eq_or_diff($txt, "test\n", "Nodes", { context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });

#
$txt = get("date=-1&field=no&autocomplete=1&expression=" . uri_escape("node=te*"));
$mtxt = get("date=-1&field=no&autocomplete=1&expression=" . uri_escape("node=te*"), 1);
eq_or_diff($txt, "[\"test\"]\n", "Autocomplete Nodes", { context => 3 });
eq_or_diff($mtxt, "[]\n", "Multi Autocomplete Nodes", { context => 3 });

#
$txt = get("date=-1&field=no&expression=$files&counts=1");
$mtxt = get("date=-1&field=no&expression=$files&counts=1", 1);
eq_or_diff($txt, "test, 13\n", "Nodes count", { context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });

#
$txt = get("date=-1&field=a1&expression=$files&counts=1");
$mtxt = get("date=-1&field=a1&expression=$files&counts=1", 1);
eq_or_diff($txt, 
"10.0.0.1, 2
10.0.0.2, 1
10.0.0.3, 1
10.180.156.185, 9
", "ip count", { context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });

#
$txt = get("date=-1&field=a1&autocomplete=1&expression=" . uri_escape("$filestr && ip.src=10.180"));
$mtxt = get("date=-1&field=a1&autocomplete=1&expression=" . uri_escape("$filestr && ip.src=10.180"), 1);
eq_or_diff($txt, "[\"10.180.156.185\"]\n", "Autocomplete IPs", { context => 3 });
eq_or_diff($mtxt, "[]\n", "Multi Autocomplete IPs", { context => 3 });

#
$txt = get("date=-1&field=ta&expression=$files&counts=1");
$mtxt = get("date=-1&field=ta&expression=$files&counts=1", 1);
eq_or_diff($txt, 
"byhost2, 7
byip1, 1
domainwise, 7
dstip, 4
hosttaggertest1, 7
hosttaggertest2, 7
iptaggertest1, 1
iptaggertest2, 1
ipwise, 1
ipwisecsv, 4
smtp:authlogin, 1
socks:password, 2
srcip, 4
wisebyhost2, 7
wisebyip1, 1
", "tags count", { context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });

#
$txt = get("date=-1&field=hh1&expression=$files&counts=1");
$mtxt = get("date=-1&field=hh1&expression=$files&counts=1", 1);
eq_or_diff($txt, 
"accept, 6
accept-encoding, 2
accept-language, 1
connection, 1
cookie, 2
host, 6
referer, 1
user-agent, 6
", "http header count", { context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });

#
$txt = get("date=-1&field=hmd5&expression=$files");
$mtxt = get("date=-1&field=hmd5&expression=$files", 1);
eq_or_diff($txt,
"09b9c392dc1f6e914cea287cb6be34b0
2069181ae704855f29caf964ca52ec49
222315d36e1313774cb1c2f0eb06864f
b0cecae354b9eab1f04f70e46a612cb1
", "http md5", { context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });

#
$txt = get("date=-1&field=hmd5&autocomplete=1&expression=" . uri_escape("$filestr && http.md5=2*"));
$mtxt = get("date=-1&field=hmd5&autocomplete=1&expression=" . uri_escape("$filestr && http.md5=2*"), 1);
eq_or_diff($txt, "[\"2069181ae704855f29caf964ca52ec49\",\"222315d36e1313774cb1c2f0eb06864f\",\"b0cecae354b9eab1f04f70e46a612cb1\"]\n", "Autocomplete HTTP md5s", { context => 3 });
eq_or_diff($mtxt, "[]\n", "Multi Autocomplete HTTP md5s", { context => 3 });

#
$txt = get("date=-1&field=hmd5&expression=$files&counts=1");
$mtxt = get("date=-1&field=hmd5&expression=$files&counts=1", 1);
eq_or_diff($txt,
"09b9c392dc1f6e914cea287cb6be34b0, 4
2069181ae704855f29caf964ca52ec49, 1
222315d36e1313774cb1c2f0eb06864f, 1
b0cecae354b9eab1f04f70e46a612cb1, 1
", "http md5 count", { context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });

#
$txt = get("date=-1&field=rawus&expression=$files&counts=0");
$mtxt = get("date=-1&field=rawus&expression=$files&counts=0", 1);
eq_or_diff($txt,
"//www.example.com/
//www.google.com/
//www.google.com/search?client=firefox&rls=en&q=Sheepskin%20Boots&start=0&num=10&hl=en&gl=us&uule=xxxxxxxxxxxxxxxxxxxxxxxxxxxx
//www.google.com/search?client=firefox&rls=en&q=Sheepskin%20Boots&start=10&num=10&hl=en&gl=us&uule=xxxxxxxxxxxxxxxxxxxxxxxxxxxx
", "http uri", { context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });

#
$txt = get("date=-1&field=rawus&expression=$files&counts=1");
$mtxt = get("date=-1&field=rawus&expression=$files&counts=1", 1);
eq_or_diff($txt,
"//www.example.com/, 4
//www.google.com/, 1
//www.google.com/search?client=firefox&rls=en&q=Sheepskin%20Boots&start=0&num=10&hl=en&gl=us&uule=xxxxxxxxxxxxxxxxxxxxxxxxxxxx, 1
//www.google.com/search?client=firefox&rls=en&q=Sheepskin%20Boots&start=10&num=10&hl=en&gl=us&uule=xxxxxxxxxxxxxxxxxxxxxxxxxxxx, 1
", "http uri", { context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });

#
$txt = get("date=-1&field=rawua&expression=$files&counts=0");
$mtxt = get("date=-1&field=rawua&expression=$files&counts=0", 1);
eq_or_diff($txt,
"Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322)
Mozilla/5.0 (Windows NT 5.1; rv:25.0) Gecko/20100101 Firefox/25.0
curl/7.24.0 (x86_64-apple-darwin12.0) libcurl/7.24.0 OpenSSL/0.9.8y zlib/1.2.5
", "http user agent", {context => 3 });
eq_or_diff($mtxt, $txt, "single doesn't match multi", { context => 3 });
