use Test::More tests => 192;
use Cwd;
use URI::Escape;
use ArkimeTest;
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

### DNS Answer Tests
my $dnsfiles = "(file=*/pcap/dns-udp.pcap||file=*/pcap/dns-mx.pcap||file=*/pcap/dns-https.pcap||file=*/pcap/dns-wiresharkrepo.pcap||file=*/pcap/dns-caa.pcap)";
# dns.cnt
countTest(14, "date=-1&expression=" . uri_escape("$dnsfiles&&protocols==dns"));

# dns.header_flags
countTest(7, "date=-1&expression=" . uri_escape("$dnsfiles&&dns.header_flags==aa"));
countTest(13, "date=-1&expression=" . uri_escape("$dnsfiles&&dns.header_flags==RD"));

# dns.answer.ip
countTest(1, "date=-1&expression=" . uri_escape("$dnsfiles&&dns.answer.ip==69.20.95.4"));

# dns.answer.cname
countTest(2, "date=-1&expression=" . uri_escape("$dnsfiles&&dns.answer.cname==github.com"));
countTest(2, "date=-1&expression=" . uri_escape("$dnsfiles&&dns.answer.cname==GITHUB.COM"));

# dns.answer.ns
countTest(2, "date=-1&expression=" . uri_escape("$dnsfiles&&dns.answer.ns==ns2.p16.dynect.net"));

# dns.answer.mx
countTest(1, "date=-1&expression=" . uri_escape("$dnsfiles&&dns.answer.mx==cluster5.us.messagelabs.com"));

# dns.answer.priority
countTest(2, "date=-1&expression=" . uri_escape("$dnsfiles&&dns.answer.priority==10"));

# dns.answer.https
countTest(1, "date=-1&expression=" . uri_escape("$dnsfiles&&dns.answer.https==\"HTTPS 1 . alpn=h3,h2 ipv4hint=104.16.132.229,104.16.133.229 ipv6hint=2606:4700::6810:84e5,2606:4700::6810:85e5\""));

# dns.answer.txt
countTest(1, "date=-1&expression=" . uri_escape("$dnsfiles&&dns.answer.txt==\"v=spf1 ptr ?all\""));

# dns.answer.caa
countTest(1, "date=-1&expression=" . uri_escape("$dnsfiles&&dns.answer.caa==\"CAA 0 issue amazon.com\""));

# dns.answer.name
countTest(1, "date=-1&expression=" . uri_escape("$dnsfiles&&dns.answer.name==alephium.org"));

# dns.answer.cnt
countTest(1, "date=-1&expression=" . uri_escape("$dnsfiles&&dns.answer.cnt==2"));

# dns.answer.type
countTest(2, "date=-1&expression=" . uri_escape("$dnsfiles&&dns.answer.type==MX"));

# dns.answer.class
countTest(8, "date=-1&expression=" . uri_escape("$dnsfiles&&dns.answer.class==in"));
countTest(8, "date=-1&expression=" . uri_escape("$dnsfiles&&dns.answer.class==IN"));

# dns.answer.ttl
countTest(1, "date=-1&expression=" . uri_escape("$dnsfiles&&dns.answer.ttl==1980"));

# Raw UTF-8 names, see sec_dns_utf8_names in gen/gen_dnssec_synthetic.py
my $u8 = "file=$pwd/dnssec_synthetic.pcap";

# valid UTF-8 names are saved as sent instead of cat -v escaped
countTest(1, "date=-1&expression=" . uri_escape("$u8&&host.dns==\"andy’s macbook pro._airplay._tcp.local\""));
countTest(0, "date=-1&expression=" . uri_escape("$u8&&host.dns==\"andym-bm-^\@m-^ys macbook pro._airplay._tcp.local\""));
countTest(3, "date=-1&expression=" . uri_escape("$u8&&host.dns==\"café*\""));

# raw UTF-8 and decoded punycode with uppercase letters are both saved lowercase
countTest(2, "date=-1&expression=" . uri_escape("$u8&&host.dns==café.example.com"));
countTest(2, "date=-1&expression=" . uri_escape("$u8&&host.dns==CAFÉ.EXAMPLE.COM"));
countTest(1, "date=-1&expression=" . uri_escape("$u8&&host.dns==café.example.com&&dns.puny==xn--caf-dma.example.com"));

# names that aren't valid hostnames lowercase their non ASCII letters too, mDNS and LLMNR don't tag them
countTest(1, "date=-1&expression=" . uri_escape("$u8&&protocols==llmnr&&dns.query.host==\"büro pc\""));
countTest(1, "date=-1&expression=" . uri_escape("$u8&&protocols==llmnr&&dns.query.host==\"BÜRO PC\""));
countTest(0, "date=-1&expression=" . uri_escape("$u8&&protocols==[llmnr,mdns]&&tags==[bad-hostname,dns:raw-utf8]"));
countTest(3, "date=-1&expression=" . uri_escape("$u8&&protocols==dns&&tags==dns:raw-utf8"));
countTest(1, "date=-1&expression=" . uri_escape("$u8&&protocols==dns&&tags==bad-hostname&&tags==dns:raw-utf8"));

# answer names are saved lowercase and searched ignoring case
countTest(1, "date=-1&expression=" . uri_escape("$u8&&dns.answer.ptr==\"café speaker._airplay._tcp.local\""));
countTest(1, "date=-1&expression=" . uri_escape("$u8&&dns.answer.ptr==\"Café Speaker._airplay._tcp.local\""));
countTest(1, "date=-1&expression=" . uri_escape("$u8&&dns.answer.name==\"Café Shop.example.com\""));
countTest(1, "date=-1&expression=" . uri_escape("$u8&&dns.answer.cname==ÉCOLE.example.com"));
countTest(1, "date=-1&expression=" . uri_escape("$u8&&dns.answer.mx==\"Mail Größe.example.com\""));
countTest(1, "date=-1&expression=" . uri_escape("$u8&&dns.answer.ns==\"NS Zürich.example.com\""));
countTest(1, "date=-1&expression=" . uri_escape("$u8&&host.dns==école.example.com&&host.dns.mailserver==\"mail größe.example.com\"&&host.dns.nameserver==\"ns zürich.example.com\""));

# fullwidth letters and ideographic full stops aren't folded into another hostname
countTest(1, "date=-1&expression=" . uri_escape("$u8&&host.dns==\xef\xbd\x87oogle.com"));
countTest(0, "date=-1&expression=" . uri_escape("$u8&&host.dns==google.com"));
countTest(1, "date=-1&expression=" . uri_escape("$u8&&host.dns==\"evil\xe3\x80\x82google.com\""));
countTest(0, "date=-1&expression=" . uri_escape("$u8&&host.dns==evil.google.com"));

# right-to-left override and zero width space are escaped, emoji joined with a zero width joiner are kept
countTest(1, "date=-1&expression=" . uri_escape("$u8&&dns.answer.ptr==\"abcm-bm-^\@m-.cod.exe._airplay._tcp.local\""));
countTest(1, "date=-1&expression=" . uri_escape("$u8&&dns.answer.ptr==\"zerom-bm-^\@m-^kwidth._airplay._tcp.local\""));
countTest(1, "date=-1&expression=" . uri_escape("$u8&&dns.answer.ptr==\"family \xf0\x9f\x91\xa8\xe2\x80\x8d\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa7 tv._airplay._tcp.local\""));
