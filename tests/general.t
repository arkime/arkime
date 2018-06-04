use Test::More tests => 593;
use Cwd;
use URI::Escape;
use MolochTest;
use strict;

my $pwd = "*/pcap";

# Regex missing backslash tests
    errTest("date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri==/js/xxxxxx/"));
    errTest("date=-1&expression=" . uri_escape("(file=$pwd/http-no-length.pcap)&&http.uri==[/js/xxxxxx/]"));

# Exists check
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&node==test&&node==EXISTS!"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&node==test&&node!=EXISTS!"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&node==test&&asn==EXISTS!"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&node==test&&asn!=EXISTS!"));

# file tests
    countTest(0, "date=-1&expression=file=nofile.pcap");
    countTest(3, "date=-1&expression=file=$pwd/bt-udp.pcap");
    countTest(2, "date=-1&expression=file=$pwd/bt-tcp.pcap");
    countTest(5, "date=-1&expression=file=$pwd/bt-*.pcap");
    countTest(5, "date=-1&expression=file=/.*\\/bt-.*.pcap/");
    errTest("date=-1&expression=file=[$pwd/bt-udp.pcap,$pwd/bt-tcp.pcap]");
    countTest(2, "date=-1&expression=file=$pwd/dns-tcp.pcap");
    countTest(4, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/smtp-starttls.pcap)"));
# node tests
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&node==test"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&node==foobar"));
# asn tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn.dst==\"AS0000 This is neat\""));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn.dst==\"AS0000 This is bad\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn==\"AS0000 This is neat\""));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn==\"AS0000 This is bad\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn.src==\"AS0001 Cool Beans!\""));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn.src==\"Cool\""));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn==\"Cool\""));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn==\"Coo\""));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn==\"Coo*\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn==\"*Cool*\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn.src==\"*Cool*\""));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn==/Cool/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn==/.*Cool.*/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn.src==/.*Cool.*/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&test.ip.asn==/.*nea.*/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&test.ip.asn==*nea*"));
# country tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src==CA"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src==ca"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src!=CA"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src!=ca"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country==CA"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country==ca"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country!=CA"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country!=ca"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src==/CA.*/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src==/ca.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src!=/CA.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src!=/ca.*/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src==*A"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src==*a"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src!=*A"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src!=*a"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&country==RU"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&test.ip.country==RU"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&country==Ru"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&test.ip.country==Ru"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&country!=RU"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&test.ip.country!=RU"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&country==/.*U/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&test.ip.country==/.*U/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&country==*U"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&test.ip.country==*U"));
# rir tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir==\"TEST\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir==\"test\""));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir!=\"test\""));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir!=test"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir==badrir"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir!=badrir"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src!=test"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.dst!=test"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src!=tes*"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.dst!=tes*"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src==\"TEST\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src==\"test\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src==TES*"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src==tes*"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src==/TES.*/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src==/tes.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src!=/TES.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src!=/tes.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/smtp-starttls.pcap)&&rir==[TEST,ARIN]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/smtp-starttls.pcap)&&rir==[TEST,ARIN,BADRIR]"));

# ip tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.src=10.0.0.2"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.src!=10.0.0.2"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=10.0.0.1"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst!=10.0.0.1"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=10.0.0.0/24"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=[10.0.0.0/24]"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=10.0.0/24"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=[10.0.0/24]"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=10.0.0"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=0"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst!=0"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&test.ip=10.0.0.1"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&test.ip!=10.0.0.1"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=[10.0.0.1]"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=[10.0.0.1,10.0.0.3]"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=[10.0.0.1/32,10.0.0.3/32]"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip=[10.0.0.1/32]"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.src=10.0.0.2:"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.src!=10.0.0.2:"));

# ipv6 tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/v6-http.pcap&&ip.dst=2001:6f8:900:7c0::2"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/v6-http.pcap&&ip.dst=2001:6f8:900:7c0:0:0:0:2"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/v6-http.pcap&&ip.dst=2001:6f8:900:7c0::2.80"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/v6-http.pcap&&ip.src=.59201"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/v6-http.pcap&&ip.dst=2001:6f8:900:7c0::2."));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/v6-http.pcap&&ip.dst=2001:6f8:900:7c0:0:0:0:2."));

# ipv6 all tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/v6-http.pcap&&ip=2001:6f8:900:7c0::2"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/v6-http.pcap&&ip=2001:6f8:900:7c0:0:0:0:2"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/v6-http.pcap&&ip=2001:6f8:900:7c0::2.80"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/v6-http.pcap&&ip=.59201"));

# ip boundary tests
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/ip-boundaries.pcap&&ip.src=0.0.0.0"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/ip-boundaries.pcap&&ip.src=255.255.255.254"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/ip-boundaries.pcap&&ip.src=255.255.255.255"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/ip-boundaries.pcap&&ip.src=255.255.255/24"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/ip-boundaries.pcap&&ip.src=255.255.255.255:50759"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/ip-boundaries.pcap&&ip.src=[255.255.255.255:50759]"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/ip-boundaries.pcap&&ip.src=255.255.255/24:50759"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/ip-boundaries.pcap&&ip.src=:50759"));

    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/ip-boundaries.pcap&&ip.dst=255.255.255.255"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/ip-boundaries.pcap&&ip.dst=0.0.0.1"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/ip-boundaries.pcap&&ip.dst=0.0.0.0"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/ip-boundaries.pcap&&ip.dst=0.0.0/24"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/ip-boundaries.pcap&&ip.dst=0.0.0.0:3207"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/ip-boundaries.pcap&&ip.dst=[0.0.0.0:3207]"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/ip-boundaries.pcap&&ip.dst=0.0.0/24:3207"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/ip-boundaries.pcap&&ip.dst=:3207"));

# ip.protocol
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol=1"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol=6"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol=17"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol!=17"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol=icmp"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol=udp"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol!=udp"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol=tcp"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol=[tcp,6]"));
    countTest(5, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol=[tcp,17]"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/sctp-www.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol=132"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/sctp-www.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol=sctp"));
    countTest(24, "date=-1&expression=" . uri_escape("(file=$pwd/wireshark-esp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol=esp"));
# IP:Port tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.src=10.0.0.2:50759"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.src=10.0.0:50759"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.src=10.0.0.1/24:50759"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.src=10.0.0.2:50758"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=10.0.0.2:50758"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip=10.0.0.2:50759"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.src=[10.0.0.2:50759]"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip=[10.0.0.2/32:50759]"));
# port tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port.src=50759"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port.src!=50759"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port.src>=50759"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port.src>50759"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port.src<50759"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port.src<=50759"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port=50759"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port!=50759"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port>=50759"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port>50759"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port<50759"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port<=10000"));
# packets tests
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&packets==1"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&packets.src==1"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&packets.dst==1"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&packets.dst==0"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&packets.dst!=1"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&packets.dst!=1"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets>0"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets>30"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets.src>17"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets.src<17"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets.src<=17"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets.src>=17"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets.src==17"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets.src!=17"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets.src==[17]"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets.src==[17,123]"));
# bytes tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes>11000"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes<11000"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes==10911"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes!=10911"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes.src>1900"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes.src<1900"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes.src==1912"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes.src!=1912"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes.dst>9200"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes.dst<9200"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes.dst==9215"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes.dst!=9215"));
# databytes tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes>8928"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes<8928"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes==8929"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes!=8929"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes.src>656"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes.src<656"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes.src==646"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes.src!=646"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes.dst>8282"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes.dst<8282"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes.dst==8283"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes.dst!=8283"));
# tags tests
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&tags==nosuchtag"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&tags==nosuch*"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&tags==srcip"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&tags!=srcip"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&tags==srci*"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&tags==[srcip]"));
# protocols tests
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols==tcp"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols==socks"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols==http"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols==udp"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols==[socks,foo]"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols==[socks,tcp]"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols==*cp"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols==/.*ttp/"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/sctp-www.pcap&&protocols==tcp"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/sctp-www.pcap&&protocols==sctp"));
    countTest(24, "date=-1&expression=" . uri_escape("file=$pwd/wireshark-esp.pcap&&protocols=esp"));
# protocols.cnt tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols.cnt==1"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols.cnt!=1"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols.cnt>1"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols.cnt>=1"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols.cnt<2"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols.cnt<=2"));
# payload8 tests
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.hex!=64313a6164323a69"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.hex==64313a6164323a69"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.hex==\"64313A6164323A69\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.hex==*13A6164323A69"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.hex==/.*13A6164323A69/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.hex==[64313A6164323A69]"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.hex!=[64313A6164323A69]"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.hex==[64313A6164323A69,64313a71393a6669]"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.utf8=\"GET / HT\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.utf8=\"HTTP/1.1\""));
# payload8.src tests
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.src.hex!=64313a6164323a69"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.dst.hex==64313a6164323a69"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.src.hex==64313a6164323a69"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.src.hex==64313A6164323A69"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.src.hex==*13A6164323A69"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.src.hex==/.*13A6164323A69/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.src.hex==[64313A6164323A69]"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.src.hex!=[64313A6164323A69,64313a71393a6669]"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.src.hex==[64313A6164323A69,64313a71393a6669]"));

    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.src.utf8=\"GET / HT\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.src.utf8=GET*"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.src.utf8=/GET.*/"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.src.utf8=/.*NOT.*/"));

# payload8.dst tests
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap&&payload8.dst.hex!=0500050000010ab4"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap&&payload8.dst.hex==0500050000010ab4"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap&&payload8.dst.hex==\"0500050000010Ab4\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap&&payload8.dst.hex==*0000010ab4"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap&&payload8.dst.hex==/.*50000010Ab4/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap&&payload8.dst.hex==[0500050000010ab4]"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap&&payload8.dst.hex!=[0500050000010ab4,005adfb20ab49cf9]"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap&&payload8.dst.hex==[0500050000010ab4,005adfb20ab49cf9]"));

    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.utf8=HTTP/1.1"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.utf8=HTTP*"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.utf8=/.*TP.*/"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.utf8=/.*NOT.*/"));

if (0) {
# session.segments tests
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/long-session.pcap||file=$pwd/socks5-reverse.pcap)&&session.segments=2"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/long-session.pcap||file=$pwd/socks5-reverse.pcap)&&session.segments=1"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/long-session.pcap||file=$pwd/socks5-reverse.pcap)&&session.segments=[2]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/long-session.pcap||file=$pwd/socks5-reverse.pcap)&&session.segments!=[2]"));
# sessions.length tests
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/long-session.pcap||file=$pwd/socks5-reverse.pcap)&&session.length=908493"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/long-session.pcap||file=$pwd/socks5-reverse.pcap)&&session.length>=908493"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/long-session.pcap||file=$pwd/socks5-reverse.pcap)&&session.length<908493"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/long-session.pcap||file=$pwd/socks5-reverse.pcap)&&session.length=[908493,908494]"));
}

# vlan tests
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&vlan=500"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&vlan.cnt=1"));

# mac.src tests
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&mac.src=00:1a:e3:dc:2e:c0"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&mac.src=00:*"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&mac.src=/.*e.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&mac.src=[00:1a:e3:dc:2e:c0,00:19:e2:ba:2f:c1]"));

# mac.dst tests
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&mac.dst=00:23:04:17:9b:00"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&mac.dst=*:e*"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&mac.dst=/00:.*/"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&mac.dst=[00:23:04:17:9b:00,00:1a:e3:dc:2e:c0]"));

# mac tests
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&mac=00:1a:e3:dc:2e:c0"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&mac=00:*"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&mac=/.*e.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&mac=[00:1a:e3:dc:2e:c0,00:19:e2:ba:2f:c1]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&mac=00:23:04:17:9b:00"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&mac=*:e*"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&mac=/00:.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&mac=[00:23:04:17:9b:00,00:1a:e3:dc:2e:c0]"));

# oui.src tests
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&oui.src=\"Cisco Systems, Inc\""));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&oui.src=Cisco*"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&oui.src=\"cisco Systems, Inc\""));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&oui.src=cisco*"));

# oui.dst tests
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&oui.dst=\"Juniper Networks\""));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&oui.dst=Juniper*"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&oui.dst=\"juniper Networks\""));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/dns-dnskey.pcap)&&oui.dst=juniper*"));

#starttime
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/socks5-reverse.pcap)&&starttime==\"2014/02/26 10:27:57\""));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/socks5-reverse.pcap)&&starttime==\"2014/02/26 10:27:58\""));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/socks5-reverse.pcap)&&starttime<=\"2014/02/26 10:27:57\""));

#stoptime
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/socks5-reverse.pcap)&&stoptime==\"2014/02/26 10:27:57\""));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/socks5-reverse.pcap)&&stoptime==\"2014/02/26 10:27:58\""));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/socks5-reverse.pcap)&&stoptime<=\"2014/02/26 10:27:57\""));

#gre
    countTest(6, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/gre-sample.pcap)&&gre.ip==172.27.1.66"));
    countTest(6, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/gre-sample.pcap)&&gre.ip.cnt==2"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/gre-sample.pcap)&&gre.ip.cnt==1"));
    countTest(6, "date=-1&expression=" . uri_escape("(file=$pwd/dns-flags0110.pcap||file=$pwd/gre-sample.pcap)&&gre.ip.rir==ARIN"));

#tcpflags
    countTest(9, "date=-1&expression=" . uri_escape("(file=$pwd/socks-http-pass.pcap||file=$pwd/gre-sample.pcap)"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/socks-http-pass.pcap||file=$pwd/gre-sample.pcap)&&tcpflags.urg > 0"));
    countTest(4, "date=-1&expression=" . uri_escape("(file=$pwd/socks-http-pass.pcap||file=$pwd/gre-sample.pcap)&&tcpflags.syn > 0"));
    countTest(4, "date=-1&expression=" . uri_escape("(file=$pwd/socks-http-pass.pcap||file=$pwd/gre-sample.pcap)&&tcpflags.syn-ack < 2"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/socks-http-pass.pcap||file=$pwd/gre-sample.pcap)&&tcpflags.ack < 7"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/socks-http-pass.pcap||file=$pwd/gre-sample.pcap)&&tcpflags.psh==8"));
    countTest(4, "date=-1&expression=" . uri_escape("(file=$pwd/socks-http-pass.pcap||file=$pwd/gre-sample.pcap)&&tcpflags.fin==2"));
    countTest(4, "date=-1&expression=" . uri_escape("(file=$pwd/socks-http-pass.pcap||file=$pwd/gre-sample.pcap)&&tcpflags.rst==0"));
