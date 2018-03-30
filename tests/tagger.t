use Test::More tests => 56;
use Cwd;
use URI::Escape;
use MolochTest;
use strict;

my $pwd = "*/pcap";

# tagger tests 1
    countTest(4, "date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=hosttaggertest1"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/bt-udp.pcap||file=$pwd/bigendian.pcap)&&tags=iptaggertest1"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/http-content-gzip.pcap)&&tags=md5taggertest1"));

# tagger tests 2
    countTest(4, "date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=hosttaggertest2"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&host=cluster5.us.messagelabs.com"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=byhost1&&irc.channel=byhost1channel&&email.x-priority=666"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&host=www.example.com"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=byhost2&&mysql.user=byhost2mysqluser&&test.ip=122.122.122.122"));

    countTest(4, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/bt-udp.pcap||file=$pwd/bigendian.pcap||file=$pwd/v6-http.pcap)&&tags=iptaggertest2"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/bt-udp.pcap||file=$pwd/bigendian.pcap)&&ip=10.0.0.3"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/bt-udp.pcap||file=$pwd/bigendian.pcap)&&tags=byip1&&irc.channel=byip1channel&&email.x-priority=111"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/bt-udp.pcap||file=$pwd/bigendian.pcap)&&ip=192.168.177.160"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/bt-udp.pcap||file=$pwd/bigendian.pcap)&&tags=byip2&&mysql.user=byip2mysqluser&&test.ip=111.111.111.111"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/bt-udp.pcap||file=$pwd/bigendian.pcap||file=$pwd/v6-http.pcap)&&tags=byip2&&mysql.user=byip2mysqluser"));

    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/http-content-gzip.pcap)&&tags=md5taggertest2"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/socks5-rdp.pcap||file=$pwd/http-content-gzip.pcap)&&tags=bymd51&&mysql.user=bymd51mysqluser&&test.ip=133.133.133.133"));

    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&tags=emailtaggertest2"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&tags=srcmatch"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&tags=dstmatch"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&email.dst=added1"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&email.src=added2"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&tagger.str=house"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&tagger.str=boat"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&tagger.int=3"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-data-521.pcap)&&tagger.int=1"));

    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-500-head.pcap||file=$pwd/http-wrapped-header.pcap)&&tags=uritaggertest2"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-500-head.pcap||file=$pwd/http-wrapped-header.pcap)&&http.referer=added1&&tags=firstmatch"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-500-head.pcap||file=$pwd/http-wrapped-header.pcap)&&http.user-agent=added2&&tags=secondmatch"));
