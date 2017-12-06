use Test::More tests => 32;
use Cwd;
use URI::Escape;
use MolochTest;
use strict;

my $pwd = getcwd() . "/pcap";
my $files = "(file=$pwd/socks-http-example.pcap||file=$pwd/socks-http-pass.pcap||file=$pwd/socks-https-example.pcap||file=$pwd/socks5-http-302.pcap||file=$pwd/socks5-rdp.pcap||file=$pwd/socks5-reverse.pcap||file=$pwd/socks5-smtp-503.pcap)";
diag $files;

countTest(12, "date=-1&expression=" . uri_escape("$files&&protocols==socks"));

# host.socks
    countTest(1, "date=-1&expression=" . uri_escape("$files&&host.socks==www.google.com"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&host.socks==WWW.google.com"));

# ip.socks
    countTest(5, "date=-1&expression=" . uri_escape("$files&&ip.socks==93.184.216.119"));
    countTest(3, "date=-1&expression=" . uri_escape("$files&&ip.socks==93.184.216.119:80"));

# socks.country
    countTest(6, "date=-1&expression=" . uri_escape("$files&&socks.country==USA"));
    countTest(6, "date=-1&expression=" . uri_escape("$files&&socks.country==usa"));

# socks.rir
    countTest(5, "date=-1&expression=" . uri_escape("$files&&socks.rir==RIPE"));
    countTest(5, "date=-1&expression=" . uri_escape("$files&&socks.rir==ripe"));

# socks.asn
    countTest(1, "date=-1&expression=" . uri_escape("$files&&socks.asn==\"AS0000 This is neat\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&socks.asn==\"AS0000\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&socks.asn==\"aS0000\""));

# socks.port
    countTest(6, "date=-1&expression=" . uri_escape("$files&&socks.port==80"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&socks.port==1"));

# socks.user
    countTest(2, "date=-1&expression=" . uri_escape("$files&&socks.user==testuser"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&socks.port==Testuser"));
