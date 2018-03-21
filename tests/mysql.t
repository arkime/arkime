use Test::More tests => 10;
use Cwd;
use URI::Escape;
use MolochTest;
use strict;

my $pwd = "*/pcap";
my $files = "(file=$pwd/mysql-allow.pcap||file=$pwd/mysql-deny.pcap)";

countTest(2, "date=-1&expression=" . uri_escape("$files&&protocols==mysql"));

# mysql.user
    countTest(1, "date=-1&expression=" . uri_escape("$files&&mysql.user==user0"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&mysql.user==User0"));

# mysql.ver
    countTest(1, "date=-1&expression=" . uri_escape("$files&&mysql.ver==\"5.5.35-0FUNntu0.12.04.2\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&mysql.ver==\"5.5.35-0funntu0.12.04.2\""));
