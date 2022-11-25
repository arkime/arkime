# WISE tests
use Test::More tests => 113;
use MolochTest;
use Cwd;
use URI::Escape;
use Data::Dumper;
use Test::Differences;
use JSON -support_by_pp;
use strict;

my $wise;
my @wise;

# IP Query
$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/ip/10.0.0.3")->content;
@wise = sort { $a->{value} cmp $b->{value}} @{from_json($wise)};

eq_or_diff(\@wise, from_json('[
{"field":"email.x-priority","len":3,"value":"999"},
{"field":"tags","len":6,"value":"ipwise"},
{"field":"tags","len":7,"value":"ipwise2"},
{"field":"tags","len":9,"value":"ipwisecsv"},
{"field":"tags","len":9,"value":"wisebyip1"},
{"field":"irc.channel","len":16,"value":"wisebyip1channel"}
]'),"All 10.0.0.3");

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/file:ip/ip/10.0.0.3")->content;
@wise = sort { $a->{value} cmp $b->{value}} @{from_json($wise)};

eq_or_diff(\@wise, from_json('[
{"field":"email.x-priority","len":3,"value":"999"},
{"field":"tags","len":6,"value":"ipwise"},
{"field":"tags","len":7,"value":"ipwise2"},
{"field":"tags","len":9,"value":"wisebyip1"},
{"field":"irc.channel","len":16,"value":"wisebyip1channel"}
]'),"file:ip 10.0.0.3");

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/file:ipblah/ip/10.0.0.3")->content;
eq_or_diff($wise, 'Unknown source file:ipblah',"file:ipblah 10.0.0.3");

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/ip/10.0.0.2")->content;
eq_or_diff($wise, '[{"field":"tags","len":9,"value":"ipwisecsv"}]', "All 10.0.0.2");

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/ip/10.0.0.1")->content;
eq_or_diff($wise, '[]',"All 10.0.0.1");

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/ip/2001:16d8:ffce:0010:aca8:353c:291d:a9b3")->content;
eq_or_diff($wise, '[{"field":"tags","len":12,"value":"ipwise-array"},
{"field":"tags","len":10,"value":"ipwisejson"}]');

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/ip/2001:16d8:ffce:0010:aca8:353c:291d:0001")->content;
eq_or_diff($wise, '[{"field":"tags","len":13,"value":"ipwise-normal"},
{"field":"tags","len":10,"value":"ipwisejson"}]');

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/ip/2001:16d8:ffce:0010:aca8:353c:291d:0002")->content;
eq_or_diff($wise, '[{"field":"tags","len":12,"value":"ipwise-comma"},
{"field":"tags","len":10,"value":"ipwisejson"}]');

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/ip/10.20.30.50")->content;
eq_or_diff($wise, '[{"field":"tags","len":12,"value":"ipwise-array"},
{"field":"tags","len":10,"value":"ipwisejson"}]');

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/ip/10.20.30.51")->content;
eq_or_diff($wise, '[{"field":"tags","len":12,"value":"ipwise-comma"},
{"field":"tags","len":10,"value":"ipwisejson"}]');

# IP File Dump
$wise = "[" . $MolochTest::userAgent->get("http://$MolochTest::host:8081/dump/file:ip")->content . "]";
@wise = sort { $a->{key} cmp $b->{key}} @{from_json($wise, {relaxed=>1})};
eq_or_diff(\@wise,
from_json('[
{"key":"10.0.0.3","ops":
[{"field":"tags","len":6,"value":"ipwise"},
{"field":"tags","len":7,"value":"ipwise2"},
{"field":"tags","len":9,"value":"wisebyip1"},
{"field":"irc.channel","len":16,"value":"wisebyip1channel"},
{"field":"email.x-priority","len":3,"value":"999"}]
},
{"key":"128.128.128.0/24","ops":
[{"field":"tags","len":6,"value":"ipwise"},
{"field":"tags","len":7,"value":"ipwise2"},
{"field":"tags","len":9,"value":"wisebyip2"},
{"field":"mysql.ver","len":21,"value":"wisebyip2mysqlversion"},
{"field":"test.ip","len":11,"value":"21.21.21.21"}]
},
{"key":"192.168.177.160","ops":
[{"field":"tags","len":6,"value":"ipwise"},
{"field":"tags","len":7,"value":"ipwise2"},
{"field":"tags","len":9,"value":"wisebyip2"},
{"field":"mysql.ver","len":21,"value":"wisebyip2mysqlversion"},
{"field":"test.ip","len":11,"value":"21.21.21.21"}]
},
{"key":"fe80::211:25ff:fe82:95b5","ops":
[{"field":"tags","len":6,"value":"ipwise"},
{"field":"tags","len":7,"value":"ipwise2"},
{"field":"tags","len":9,"value":"wisebyip3"},
{"field":"mysql.ver","len":21,"value":"wisebyip3mysqlversion"},
{"field":"test.ip","len":11,"value":"22.22.22.22"}]
}
]', {relaxed=>1}), "file:ip dump");

$wise = "[" . $MolochTest::userAgent->get("http://$MolochTest::host:8081/dump/file:ipcsv")->content . "]";
@wise = sort { $a->{key} cmp $b->{key}} @{from_json($wise, {relaxed=>1})};
eq_or_diff(\@wise,
from_json('[
{"key":"10.0.0.2","ops":
[{"field":"tags","len":9,"value":"ipwisecsv"}]
},
{"key":"10.0.0.3","ops":
[{"field":"tags","len":9,"value":"ipwisecsv"}]
}
]', {relaxed=>1}), "file:ipcsv dump");

# Email Query
$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/email/fudge\@aol.com")->content;
eq_or_diff($wise, '[]',"ALL fudge\@aol.com");

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/email/fudge\@fudge.com")->content;
eq_or_diff($wise, '[{"field":"tags","len":9,"value":"emailwise"}]',"ALL fudge\@fudge.com");

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/email/12345678\@aol.com")->content;
eq_or_diff(from_json($wise), from_json('[{"field":"email.dst","len":10,"value":"wiseadded1"},
{"field":"tags","len":12,"value":"wisesrcmatch"},
{"field":"wise.str","len":5,"value":"house"},
{"field":"wise.str","len":4,"value":"boat"},
{"field":"wise.float","len":6,"value":"1.2345"},
{"field":"tags","len":9,"value":"emailwise"}]
'),"ALL 12345678\@aol.com");

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/rightClicks")->content;
eq_or_diff(from_json($wise), from_json('{"VTIP":{"url":"https://www.virustotal.com/en/ip-address/%TEXT%/information/","name":"Virus Total IP","category":"ip"},"VTHOST":{"url":"https://www.virustotal.com/en/domain/%HOST%/information/","name":"Virus Total Host","category":"host"},"VTURL":{"url":"https://www.virustotal.com/latest-scan/%URL%","name":"Virus Total URL","category":"url"}}'),"right clicks");

my $pwd = "*/pcap";

# wise tests 2


    #UDP Issues
    countTest(4, "date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=domainwise"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&host=cluster5.us.messagelabs.com"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=wisebyhost1&&irc.channel=wisebyhost1channel&&email.x-priority=777"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&host=www.example.com"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=wisebyhost2&&mysql.ver=wisebyhost2mysqlversion&&test.ip=101.101.101.101"));

    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/bt-udp.pcap||file=$pwd/bigendian.pcap)&&tags=ipwise"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/bt-udp.pcap||file=$pwd/bigendian.pcap)&&tags=ipwise2"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/bt-udp.pcap||file=$pwd/bigendian.pcap)&&ip=10.0.0.3"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/bt-udp.pcap||file=$pwd/bigendian.pcap)&&tags=wisebyip1&&irc.channel=wisebyip1channel&&email.x-priority=999"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/bt-udp.pcap||file=$pwd/bigendian.pcap)&&ip=192.168.177.160"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/bt-udp.pcap||file=$pwd/bigendian.pcap)&&tags=wisebyip2&&mysql.ver=wisebyip2mysqlversion&&test.ip=21.21.21.21"));

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
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&wise.int=EXISTS!"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&wise.float=EXISTS!"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&wise.float>=1.2345"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&wise.float<=1.2345"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&wise.float=1.2345"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&wise.float=-5.4321"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&wise.float!=1.2345"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&wise.int=1"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-500-head.pcap||file=$pwd/http-wrapped-header.pcap)&&http.referer=added1wise&&tags=firstmatchwise"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-500-head.pcap||file=$pwd/http-wrapped-header.pcap)&&http.user-agent=added2wise&&tags=secondmatchwise"));

#MAC
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/6-4-gre-ppp-udp-4-dns.pcap||file=$pwd/http-wrapped-header.pcap)&&tags=macwise"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/6-4-gre-ppp-udp-4-dns.pcap||file=$pwd/http-wrapped-header.pcap)&&tags=wisebymac1"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/6-4-gre-ppp-udp-4-dns.pcap||file=$pwd/http-wrapped-header.pcap)&&tags=wisebymac2"));

$wise = "[" . $MolochTest::userAgent->get("http://$MolochTest::host:8081/dump/file:mac")->content . "]";
my @wise = sort { $a->{key} cmp $b->{key}} @{from_json($wise, {relaxed=>1})};
eq_or_diff(\@wise,
from_json('[
{"key":"00:12:1e:f2:61:3d","ops":
[{"field":"tags","len":7,"value":"macwise"},
{"field":"tags","len":10,"value":"wisebymac1"}]
},
{"key":"00:19:06:e6:82:c4","ops":
[{"field":"tags","len":7,"value":"macwise"},
{"field":"tags","len":10,"value":"wisebymac2"}]
}
]', {relaxed=>1}), "file:mac dump");

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/mac/00:12:1e:f2:61:3d")->content;
eq_or_diff($wise, '[{"field":"tags","len":10,"value":"wisebymac1"},
{"field":"tags","len":7,"value":"macwise"}]',"mac query");

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/file:mac/mac/00:12:1e:f2:61:3d")->content;
eq_or_diff($wise, '[{"field":"tags","len":10,"value":"wisebymac1"},
{"field":"tags","len":7,"value":"macwise"}]',"file:mac query");

# Sources
$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/sources")->content;
eq_or_diff($wise, '["fieldactions:test","file:domain","file:email","file:ip","file:ipcsv","file:ipjson","file:ja3","file:mac","file:md5","file:sha256","file:url","reversedns","url:aws-ips","url:azure-ips","url:gcloud-ips4","url:gcloud-ips6","valueactions:test"]',"/sources");

# Types
$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/types")->content;
eq_or_diff($wise, '["domain","email","ip","ja3","mac","md5","sha256","url"]',"sources");

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/types/file:ip")->content;
eq_or_diff($wise, '["ip"]',"types file:ip");

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/types/unknown")->content;
eq_or_diff($wise, '[]',"types unknown");

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/types/reversedns")->content;
eq_or_diff($wise, '["ip"]',"types reversedns");

# Stats
$wise = from_json($MolochTest::userAgent->get("http://$MolochTest::host:8081/stats")->content);
ok (exists $wise->{"sources"});
ok (exists $wise->{"types"});

$wise = from_json($MolochTest::userAgent->get("http://$MolochTest::host:8081/stats?search=domain")->content);
is (scalar @{$wise->{"sources"}}, 1);
is (scalar @{$wise->{"types"}}, 1);

my $wise2 = from_json($MolochTest::userAgent->get("http://$MolochTest::host:8081/stats?search=do.*n")->content);

eq_or_diff($wise, $wise2);

# Get
$wise = $MolochTest::userAgent->post("http://$MolochTest::host:8081/get", Content => "XXX")->content;
is ($wise, 'Received malformed packet');

# Views
$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/views")->content;
eq_or_diff($wise, '{"cloud":"if (session.cloud)\\n  div.sessionDetailMeta.bold Public Cloud\\n  dl.sessionDetailMeta\\n    +arrayList(session.cloud, \'service\', \'Service\', \'cloud.service\')\\n    +arrayList(session.cloud, \'region\', \'Region\', \'cloud.region\')\\n","file:email":"if (session.wise)\\n  div.sessionDetailMeta.bold Wise\\n  dl.sessionDetailMeta\\n    +arrayList(session.wise, \'str\', \'Str\', \'wise.str\')\\n    +arrayList(session.wise, \'int\', \'Int\', \'wise.int\')\\n    +arrayList(session.wise, \'float\', \'Float\', \'wise.float\')\\n"}');

# Fields
$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/fields")->content;
is(length($wise), 658);;

my $info = viewerGet("/fields");
eq_or_diff($info->{"wise.int.cnt"}, from_json('{"friendlyName":"Int Cnt","type":"integer","exp":"wise.int.cnt","help":"Unique number of Help Int","dbField":"wise.intCnt","group":"wise","dbField2":"wise.intCnt"}'));

# Field Actions
$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/fieldActions")->content;
eq_or_diff($wise, '{"ASDF":{"url":"https://www.asdf.com?expression=%EXPRESSION%&date=%DATE%&field=%FIELD%&dbField=%DBFIELD%","name":"Field Action %FIELDNAME%!","category":"ip","users":{"admin":1,"test1":1}}}');

# Value Actions
$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/valueActions")->content;
eq_or_diff($wise, '{"VTIP":{"url":"https://www.virustotal.com/en/ip-address/%TEXT%/information/","name":"Virus Total IP","category":"ip"},"VTHOST":{"url":"https://www.virustotal.com/en/domain/%HOST%/information/","name":"Virus Total Host","category":"host"},"VTURL":{"url":"https://www.virustotal.com/latest-scan/%URL%","name":"Virus Total URL","category":"url"}}');

# __proto__
$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/file:mac/__proto__/00:12:1e:f2:61:3d")->content;
is($wise, 'Not found');

$wise = $MolochTest::userAgent->get("http://$MolochTest::host:8081/__proto__/00:12:1e:f2:61:3d")->content;
is($wise, 'Not found');
