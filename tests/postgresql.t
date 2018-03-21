use Test::More tests => 14;
use Cwd;
use URI::Escape;
use MolochTest;
use strict;

my $pwd = "*/pcap";
my $files = "(file=$pwd/postgres-badpass.pcap||file=$pwd/postgres-good.pcap||file=$pwd/postgres-no-sslrequest.pcap)";

countTest(3, "date=-1&expression=" . uri_escape("$files&&protocols==postgresql"));

# postgresql.user
    countTest(1, "date=-1&expression=" . uri_escape("$files&&postgresql.user==cooluser"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&postgresql.user==Cooluser"));

# postgresql.db
    countTest(1, "date=-1&expression=" . uri_escape("$files&&postgresql.db==dbdbdbdb"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&postgresql.db==Dbdbdbdb"));

# postgresql.app
    countTest(1, "date=-1&expression=" . uri_escape("$files&&postgresql.app==psql"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&postgresql.app==psql"));
