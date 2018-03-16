use Test::More tests => 12;
use Cwd;
use URI::Escape;
use MolochTest;
use strict;

my $pwd = "*/pcap";
my $files = "(file=$pwd/wireshark-dhcp.pcap)";

countTest(2, "date=-1&expression=" . uri_escape("$files&&protocols==dhcp"));

# dhcp.type
countTest(1, "date=-1&expression=" . uri_escape("$files&&dhcp.type==REQUEST"));

# dhcp.mac
countTest(2, "date=-1&expression=" . uri_escape("$files&&dhcp.mac==\"00:0b:82:01:fc:42\""));

# dhcp.oui
countTest(2, "date=-1&expression=" . uri_escape("$files&&dhcp.oui==\"Grandstream*\""));

# dhcp.host
countTest(0, "date=-1&expression=" . uri_escape("$files&&dhcp.host==*"));

# dhcp.id
countTest(2, "date=-1&expression=" . uri_escape("$files&&dhcp.id==3d1d"));
