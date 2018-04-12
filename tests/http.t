use Test::More tests => 288;
use Cwd;
use URI::Escape;
use MolochTest;
use strict;

my $pwd = "*/pcap";
# host tests
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&host==http://www.example.com"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&host==http://www.example.com/foo"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&host==www.example.com/foo"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&host==www.example.com"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&host==*.example.com"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&host==http://*.example.com"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&host==/.*xxx.com/"));
# http.host tests
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==http://www.example.com"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==http://www.example.com/foo"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==www.example.com/foo"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==www.example.com"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==*.example.com"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==http://*.example.com"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==/.*xxx.com/"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host!=www.example.com"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host!=[www.example.com,foo.com]"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==[www.example.com,foo.com]"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==www.EXample.com"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==*.EXample.com"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==/.*XXx.com/"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host!=www.EXample.com"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host!=[www.EXample.com,foo.com]"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==[www.EXample.com,foo.com]"));
# http.method tests
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.method==GET"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.method!=GET"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.method==get"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.method==[GET,HEAD]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.method==[\"GET\",\"HEAD\"]"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.method!=[GET,HEAD]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.method==/.*E.*/"));
# http.uri tests
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==http://samples.example.com/UpdataConfig.dat"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==samples.example.com/UpdataConfig.dat"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==samples.example.com*"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==UpdataConfig.dat"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==*Config.dat"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==Config.dat"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==*config.dat"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==config.dat"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==a.zip"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==/.*a.zip/"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==/.*a.zip/"));
# http.uri.path slash tests
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri==/.*\\/js\\/xxxxxx.js/"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri==*/js/xxxxxx.j*"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri==[/.*js\\/xxxxxx.js/]"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri==[\"/.*js\\/xxxxxx.js/\"]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri==[*/js/xxxxxx.j*]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri.path==/js/xxxxxx.js"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri.path==//js/xxxxxx.js"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri.path==js/xxxxxx"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri.path==/\\/js\\/xxxxxx.js/"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri.path==/js/xxxxxx.j*"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri.path==//js/xxxxxx.j*"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri.path==[/js/xxxxxx.js]"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri.path==[//js/xxxxxx.js]"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri.path==[js/xxxxxx]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri.path==[/\\/js\\/xxxxxx.js/]"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri.path==[\"/\\/js\\/xxxxxx.js/\"]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri.path==[/\\/js\\/.*.js/]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri.path==[/js/xxxxxx.js*]"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri.path==[//js/xxxxxx.js*]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap||file=$pwd/http-content-zip.pcap)&&http.uri.path==[/js/xxxxxx.js,a.zip]"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap||file=$pwd/http-content-zip.pcap)&&http.uri.path==[//js/xxxxxx.js,a.zip]"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap||file=$pwd/http-content-zip.pcap)&&http.uri.path==[js/xxxxxx,a.zip]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap||file=$pwd/http-content-zip.pcap)&&http.uri.path==[/\\/js\\/xxxxxx.js/,a.zip]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap||file=$pwd/http-content-zip.pcap)&&http.uri.path==[/\\/js\\/.*.js/,a.zip]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap||file=$pwd/http-content-zip.pcap)&&http.uri.path==[/js/xxxxxx.js*,a.zip]"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap||file=$pwd/http-content-zip.pcap)&&http.uri.path==[//js/xxxxxx.js*,a.zip]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap||file=$pwd/http-content-zip.pcap)&&http.uri.path==[/js/xxxxxx.js,*a.zip*]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap||file=$pwd/http-content-zip.pcap)&&http.uri.path==[//js/xxxxxx.js,*a.zip*]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap||file=$pwd/http-content-zip.pcap)&&http.uri.path==[js/xxxxxx,*a.zip*]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap||file=$pwd/http-content-zip.pcap)&&http.uri.path==[/\\/js\\/xxxxxx.js/,*a.zip*]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap||file=$pwd/http-content-zip.pcap)&&http.uri.path==[\"/\\/js\\/xxxxxx.js/\",*a.zip*]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap||file=$pwd/http-content-zip.pcap)&&http.uri.path==[/\\/js\\/.*.js/,*a.zip*]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap||file=$pwd/http-content-zip.pcap)&&http.uri.path==[/js/xxxxxx.js*,*a.zip*]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap||file=$pwd/http-content-zip.pcap)&&http.uri.path==[//js/xxxxxx.js*,*a.zip*]"));

SKIP: {
    skip "Upgrade test", 42 if ($ENV{MOLOCH_REINDEX_TEST}); # reindex doesn't have http.has-header
# http.hasheader, http.hasheader.src, http.hasheader.dst tests
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader==server"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.dst==server"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.src==cookie"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader==ser*"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.dst==ser*"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.src==cook*"));
    #TODO countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader==/ser.*/"));
    #TODO countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.dst==/ser.*/"));
    #TODO tTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.src==/ser.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader==[server]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.dst==[server]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.src==[cookie]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader==SeRver"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.dst==SeRver"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.src==CookIe"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader==*VeR"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.dst==*VeR"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.src==*kIe"));
    #TODO countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader==/.*VeR/"));
    #TODO countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.dst==/.*VeR/"));
    #TODO countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.src==/.*kIe/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader==[SeRver]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.dst==[SeRver]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.src==[CooKie]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader==[content-length]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.dst==[content-length]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.src==[accept-encoding]"));
}

# http.version tests
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version==1.1"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.src==1.1"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.dst==1.1"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version==not"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.src==fudge"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.dst==paste"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version==[1.1]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.src==[1.1]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.dst==[1.1]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version==1.*"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.src==1.*"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.dst==1.*"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version==/1.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.src==/1.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.dst==/1.*/"));
# http.user-agent tests
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent==\"Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322)\""));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent!=\"*Mozilla/4.0*\""));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent==\"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_8_5) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/31.0.1650.63 Safari/537.36\""));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent!=\"*Mozilla/5.0*\""));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent==*Mozilla*"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent==*mozilla*"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent==/.*Mozilla.*/"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent==/.*mozilla.*/"));
# http.md5 tests
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5=40be8f5100e9beabab293c9d7bacaff0"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5=40Be8f5100e9beabab293c9d7bacaff0"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5=40be8f5100e9beabab293c9d7*"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5=40Be8f5100e9beabab293c9d7*"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5=/40be8f5100e9beabab293c9d7.*/"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5=/40Be8f5100e9beabab293c9d7.*/"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5!=40be8f5100e9beabab293c9d7bacaff0"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5!=40Be8f5100e9beabab293c9d7bacaff0"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5=[40be8f5100e9beabab293c9d7bacaff0,b0cecae354b9eab1f04f70e46a612cb1]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5=[40Be8f5100e9beabab293c9d7bacaff0,B0cecae354b9eab1f04f70e46a612cb1]"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5!=[40be8f5100e9beabab293c9d7bacaff0,b0cecae354b9eab1f04f70e46a612cb1]"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5!=[40Be8f5100e9beabab293c9d7bacaff0,B0cecae354b9eab1f04f70e46a612cb1]"));

# http.user tests
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-basicauth.pcap)&&http.user==userrrrr"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-basicauth.pcap)&&http.user==Userrrrr"));

# http.cookie.key
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-gzip.pcap||file=$pwd/socks5-reverse.pcap)&&http.cookie.key==NID"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-gzip.pcap||file=$pwd/socks5-reverse.pcap)&&http.cookie.key==[NID,xxxxxxxxxx]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-gzip.pcap||file=$pwd/socks5-reverse.pcap)&&http.cookie.key!=FRED"));

# http.cookie.value
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-gzip.pcap||file=$pwd/socks5-reverse.pcap)&&http.cookie.value==xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-gzip.pcap||file=$pwd/socks5-reverse.pcap)&&http.cookie.value==[xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx,xxx]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-gzip.pcap||file=$pwd/socks5-reverse.pcap)&&http.cookie.value!=FRED"));

# http.referer
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-gzip.pcap||file=$pwd/socks5-reverse.pcap)&&http.referer==EXISTS!"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-gzip.pcap||file=$pwd/socks5-reverse.pcap)&&http.referer==*search*"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-gzip.pcap||file=$pwd/socks5-reverse.pcap)&&http.referer==notfound"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-gzip.pcap||file=$pwd/socks5-reverse.pcap)&&http.referer!=notfound"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-gzip.pcap||file=$pwd/socks5-reverse.pcap)&&http.referer==/.*id=xxx.*/"));
