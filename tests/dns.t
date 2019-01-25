use Test::More tests => 106;
use Cwd;
use URI::Escape;
use MolochTest;
use strict;

my $pwd = "*/pcap";

countTest(4, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&protocols==dns"));

# dns.query.class tests
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.query.class==IN"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.query.class!=IN"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.query.class==*N"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.query.class==/IN/"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.query.class==[IN]"));
# dns.query.type tests
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.query.type==A"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.query.type!=A"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.query.type==*A"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.query.type==/A/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.query.type==[A]"));
# dns ip tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.ip==192.30.252.128"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.ip!=192.30.252.128"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.ip==192.30.252"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.ip==192.30.252.0/24"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.ip!=192.30.252"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.ip!=192.30.252.0/24"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&ip==192.30.252.128"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&ip!=192.30.252.128"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&ip==192.30.252"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&ip==192.30.252.0/24"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&ip!=192.30.252"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&ip!=192.30.252.0/24"));
# dns.status tests
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.status==NOERROR"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.status!=NOERROR"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.status==*ERROR"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.status==/.*ERROR/"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.status==[NOERROR]"));
# dns.host tests
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host==github.com"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host==mx.com"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host==[github.com,mx.com]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host!=[github.com,mx.com]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host==*hub.com"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host==/.*hub.com/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host!=/.*hub.com/"));
# dns.host.tokens tests
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host.tokens==github"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host.tokens==mx"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host.tokens==[github,mx]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host.tokens!=[github,mx]"));
# dns.puny tests
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-punycode.pcap||file=$pwd/dns-mx.pcap)&&dns.puny==xn--80a0adav.xn--80ao21a"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-punycode.pcap||file=$pwd/dns-mx.pcap)&&dns.host==\"уаноо.қаз\""));


# dns ip v6 tests
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/v6.pcap&&dns.ip==3ffe:501:410::2c0:dfff:fe47:33e"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/v6.pcap&&dns.ip==3ffe:501:410:0:2c0:dfff:fe47:33e"));
    countTest(17, "date=-1&expression=" . uri_escape("file=$pwd/v6.pcap&&ip==3ffe:501:410:0:2c0:dfff:fe47:33e"));

# dns.mailserver.host
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap||file=$pwd/v6.pcap)&&host.dns.mailserver == EXISTS!"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap||file=$pwd/v6.pcap)&&host.dns.mailserver == coconut.itojun.org"));

# dns.mailserver.ip
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap||file=$pwd/v6.pcap)&&ip.dns.mailserver == EXISTS!"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap||file=$pwd/v6.pcap)&&ip.dns.mailserver == 210.160.95.97"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap||file=$pwd/v6.pcap)&&ip.dns.mailserver == 3ffe:501:410::2c0:dfff:fe47:33e"));

# dns.nameserver.host
    countTest(21, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap||file=$pwd/v6.pcap)&&host.dns.nameserver == EXISTS!"));
    countTest(4, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap||file=$pwd/v6.pcap)&&host.dns.nameserver == coconut.itojun.org"));

# dns.nameserver.ip
    countTest(21, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap||file=$pwd/v6.pcap)&&ip.dns.nameserver == EXISTS!"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap||file=$pwd/v6.pcap)&&ip.dns.nameserver == 210.145.33.242"));

