use Test::More tests => 70;
use Cwd;
use URI::Escape;
use MolochTest;
use strict;

my $pwd = getcwd() . "/pcap";

countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&protocols==dns"));

# dns.query.class tests
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.query.class==IN"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.query.class!=IN"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.query.class==*N"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.query.class==/IN/"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.query.class==[IN]"));
# dns.query.type tests
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.query.type==A"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.query.type!=A"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.query.type==*A"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.query.type==/A/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.query.type==[A]"));
# dns ip tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.ip==192.30.252.128"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.ip!=192.30.252.128"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.ip==192.30.252"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.ip==192.30.252.0/24"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.ip!=192.30.252"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.ip!=192.30.252.0/24"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&ip==192.30.252.128"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&ip!=192.30.252.128"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&ip==192.30.252"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&ip==192.30.252.0/24"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&ip!=192.30.252"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&ip!=192.30.252.0/24"));
# dns.status tests
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.status==NOERROR"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.status!=NOERROR"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.status==*ERROR"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.status==/.*ERROR/"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.status==[NOERROR]"));
# dns.host tests
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host==github.com"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host==mx.com"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host==[github.com,mx.com]"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host!=[github.com,mx.com]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host==*hub.com"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host==/.*hub.com/"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host!=/.*hub.com/"));
