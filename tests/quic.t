use Test::More tests => 12;
use Cwd;
use URI::Escape;
use MolochTest;
use strict;

my $pwd = "*/pcap";
my $files = "(file=$pwd/quic24-wireshark.pcap||file=$pwd/fbzero-android.pcap)";

countTest(2, "date=-1&expression=" . uri_escape("$files&&protocols==quic"));

#
countTest(1, "date=-1&expression=" . uri_escape("$files&&quic.host==graph.facebook.com"));
countTest(1, "date=-1&expression=" . uri_escape("$files&&quic.host==Graph.facebook.COM"));

#
countTest(1, "date=-1&expression=" . uri_escape("$files&&quic.version==\"Q024\""));
countTest(0, "date=-1&expression=" . uri_escape("$files&&quic.version==\"q024\""));

#
countTest(1, "date=-1&expression=" . uri_escape("$files&&quic.user-agent==\"canary Chrome/44.0.2375.0\""));
