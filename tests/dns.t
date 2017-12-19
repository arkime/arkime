use Test::More tests => 76;
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

# dns ip v6 tests
    SKIP: {
        skip "Upgrade test", 6 if ($ENV{MOLOCH_REINDEX_TEST}); # reindex doesn't have ipv6 dns
        countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/v6.pcap&&dns.ip==3ffe:501:410::2c0:dfff:fe47:33e"));
        countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/v6.pcap&&dns.ip==3ffe:501:410:0:2c0:dfff:fe47:33e"));
        countTest(16, "date=-1&expression=" . uri_escape("file=$pwd/v6.pcap&&ip==3ffe:501:410:0:2c0:dfff:fe47:33e"));
    }
