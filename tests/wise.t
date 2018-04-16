# WISE tests
use Test::More tests => 44;
use MolochTest;
use Cwd;
use URI::Escape;
use Data::Dumper;
use Test::Differences;
use JSON;
use strict;


my $wise;


# IP Query
$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/ip/10.0.0.3")->content;
eq_or_diff($wise, '[{field: "tags", len: 10, value: "wisebyip1"},
{field: "irc.channel", len: 17, value: "wisebyip1channel"},
{field: "email.x-priority", len: 4, value: "999"},
{field: "tags", len: 7, value: "ipwise"},
{field: "tags", len: 10, value: "ipwisecsv"}]
', "All 10.0.0.3");

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/file:ip/ip/10.0.0.3")->content;
eq_or_diff($wise, '[{field: "tags", len: 10, value: "wisebyip1"},
{field: "irc.channel", len: 17, value: "wisebyip1channel"},
{field: "email.x-priority", len: 4, value: "999"},
{field: "tags", len: 7, value: "ipwise"}]
', "file:ip 10.0.0.3");

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/file:ipblah/ip/10.0.0.3")->content;
eq_or_diff($wise, 'Unknown source file:ipblah', "file:ipblah 10.0.0.3");


$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/ip/10.0.0.2")->content;
eq_or_diff($wise, '[{field: "tags", len: 10, value: "ipwisecsv"}]
', "All 10.0.0.2");

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/ip/10.0.0.1")->content;
eq_or_diff($wise, '[]
', "All 10.0.0.1");

# IP File Dump

$wise = "[" . $MolochTest::userAgent->get("http://$MolochTest::host:8081/dump/file:ip")->content . "]";
my @wise = sort { $a->{key} cmp $b->{key}} @{from_json($wise, {relaxed=>1, allow_barekey=>1})};
eq_or_diff(\@wise, 
from_json('[
{key: "10.0.0.3", ops:
[{field: "tags", len: 7, value: "ipwise"},
{field: "tags", len: 10, value: "wisebyip1"},
{field: "irc.channel", len: 17, value: "wisebyip1channel"},
{field: "email.x-priority", len: 4, value: "999"}]
},
{key: "128.128.128.0/24", ops:
[{field: "tags", len: 7, value: "ipwise"},
{field: "tags", len: 10, value: "wisebyip2"},
{field: "mysql.ver", len: 22, value: "wisebyip2mysqlversion"},
{field: "test.ip", len: 12, value: "21.21.21.21"}]
},
{key: "192.168.177.160", ops:
[{field: "tags", len: 7, value: "ipwise"},
{field: "tags", len: 10, value: "wisebyip2"},
{field: "mysql.ver", len: 22, value: "wisebyip2mysqlversion"},
{field: "test.ip", len: 12, value: "21.21.21.21"}]
},
{key: "fe80::211:25ff:fe82:95b5", ops:
[{field: "tags", len: 7, value: "ipwise"},
{field: "tags", len: 10, value: "wisebyip3"},
{field: "mysql.ver", len: 22, value: "wisebyip3mysqlversion"},
{field: "test.ip", len: 12, value: "22.22.22.22"}]
}
]', {relaxed=>1, allow_barekey=>1}), "file:ip dump");

$wise = "[" . $MolochTest::userAgent->get("http://$MolochTest::host:8081/dump/file:ipcsv")->content . "]";
@wise = sort { $a->{key} cmp $b->{key}} @{from_json($wise, {relaxed=>1, allow_barekey=>1})};
eq_or_diff(\@wise, 
from_json('[
{key: "10.0.0.2", ops:
[{field: "tags", len: 10, value: "ipwisecsv"}]
},
{key: "10.0.0.3", ops:
[{field: "tags", len: 10, value: "ipwisecsv"}]
}
]', {relaxed=>1, allow_barekey=>1}), "file:ipcsv dump");

# Email Query
$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/email/fudge\@aol.com")->content;
eq_or_diff($wise, '[]
', "ALL fudge\@aol.com");

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/email/fudge\@fudge.com")->content;
eq_or_diff($wise, '[{field: "tags", len: 10, value: "emailwise"}]
', "ALL fudge\@fudge.com");

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/email/12345678\@aol.com")->content;
eq_or_diff($wise, '[{field: "email.dst", len: 11, value: "wiseadded1"},
{field: "tags", len: 13, value: "wisesrcmatch"},
{field: "wise.str", len: 6, value: "house"},
{field: "wise.str", len: 5, value: "boat"},
{field: "tags", len: 10, value: "emailwise"}]
', "ALL 12345678\@aol.com");

if (0) {
# Zeus Query
$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/dump/url:zeus.domain")->content;
$wise =~ /key: "(.*)"/;
my $domain = $1;
$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/url:zeus.domain/domain/$domain")->content;
eq_or_diff($wise, '[{field: "tags", len: 12, value: "zeustracker"},
{field: "tags", len: 7, value: "botnet"}]
', "Zeus $domain");

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/url:zeus.domain/domain/aol.com")->content;
eq_or_diff($wise, 'Not found', "Zeus aol.com");
}


my $pwd = "*/pcap";

# wise tests 2

    
    #UDP Issues
    if (0) {
    countTest(4, "date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=domainwise"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&host=cluster5.us.messagelabs.com"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=wisebyhost1&&irc.channel=wisebyhost1channel&&email.x-priority=777"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&host=www.example.com"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=wisebyhost2&&mysql.ver=wisebyhost2mysqlversion&&test.ip=101.101.101.101"));

    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/bt-udp.pcap||file=$pwd/bigendian.pcap)&&tags=ipwise"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/bt-udp.pcap||file=$pwd/bigendian.pcap)&&ip=10.0.0.3"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/bt-udp.pcap||file=$pwd/bigendian.pcap)&&tags=wisebyip1&&irc.channel=wisebyip1channel&&email.x-priority=999"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/bt-udp.pcap||file=$pwd/bigendian.pcap)&&ip=192.168.177.160"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/bt-udp.pcap||file=$pwd/bigendian.pcap)&&tags=wisebyip2&&mysql.ver=wisebyip2mysqlversion&&test.ip=21.21.21.21"));
    }

    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/http-content-gzip.pcap)&&tags=md5wise"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/http-content-gzip.pcap)&&tags=wisebymd51&&mysql.ver=wisebymd51mysqlversion&&test.ip=144.144.144.144"));

    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/https-generalizedtime.pcap||file=$pwd/http-content-gzip.pcap)&&tags=ja3wise"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/https-generalizedtime.pcap||file=$pwd/http-content-gzip.pcap)&&tags=wisebyja31&&mysql.ver=wisebyja31mysqlversion&&test.ip=155.155.155.155"));

    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/smtp-zip.pcap)&&tags=sha256wise"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/smtp-zip.pcap)&&tags=wisebysha2561&&mysql.ver=wisebysha2561mysqlversion&&test.ip=1::2"));

    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&tags=emailwise"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&tags=wisesrcmatch"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&tags=wisedstmatch"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&email.dst=wiseadded1"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&email.src=wiseadded2"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&wise.str=house"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&wise.str=boat"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&wise.int=3"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&wise.int=1"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-500-head.pcap||file=$pwd/http-wrapped-header.pcap)&&http.referer=added1wise&&tags=firstmatchwise"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-500-head.pcap||file=$pwd/http-wrapped-header.pcap)&&http.user-agent=added2wise&&tags=secondmatchwise"));
