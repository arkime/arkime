use Test::More tests => 36;
use Cwd;
use URI::Escape;
use MolochTest;
use strict;

my $pwd = "*/pcap";
my $files = "(file=$pwd/socks-http-example.pcap||file=$pwd/socks-http-pass.pcap||file=$pwd/socks-https-example.pcap||file=$pwd/socks5-http-302.pcap||file=$pwd/socks5-rdp.pcap||file=$pwd/socks5-reverse.pcap||file=$pwd/socks5-smtp-503.pcap)";

countTest(12, "date=-1&expression=" . uri_escape("$files&&protocols==socks"));

# host.socks
    countTest(1, "date=-1&expression=" . uri_escape("$files&&host.socks==www.google.com"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&host.socks==WWW.google.com"));

# ip.socks
    countTest(5, "date=-1&expression=" . uri_escape("$files&&ip.socks==93.184.216.119"));
    countTest(3, "date=-1&expression=" . uri_escape("$files&&ip.socks==93.184.216.119:80"));

# country.socks
    countTest(6, "date=-1&expression=" . uri_escape("$files&&country.socks==US"));
    countTest(6, "date=-1&expression=" . uri_escape("$files&&country.socks==us"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&country.socks==EU"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&country.socks==eu"));

# socks.rir
    countTest(5, "date=-1&expression=" . uri_escape("$files&&rir.socks==RIPE"));
    countTest(5, "date=-1&expression=" . uri_escape("$files&&rir.socks==ripe"));

# socks.asn
    countTest(1, "date=-1&expression=" . uri_escape("$files&&asn.socks==\"AS0000 This is neat\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&asn.socks==\"AS0000*\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&asn.socks==\"aS0000*\""));

# socks.port
    countTest(6, "date=-1&expression=" . uri_escape("$files&&socks.port==80"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&socks.port==1"));

# socks.user
    countTest(2, "date=-1&expression=" . uri_escape("$files&&socks.user==testuser"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&socks.port==Testuser"));
