use Test::More tests => 14;
use Cwd;
use URI::Escape;
use MolochTest;
use strict;

my $pwd = "*/pcap";
my $files = "(file=$pwd/openssl-ssl3.pcap||file=$pwd/openssl-tls1.pcap||file=$pwd/https3-301-get.pcap)";

countTest(3, "date=-1&expression=" . uri_escape("$files&&protocols==tls"));

# tls.version tests
    countTest(1, "date=-1&expression=" . uri_escape("$files&&tls.version==SSLv3"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&tls.version==sslv3"));
    countTest(2, "date=-1&expression=" . uri_escape("$files&&tls.version==TLS*"));

# tls.cipher tests
    countTest(2, "date=-1&expression=" . uri_escape("$files&&tls.cipher==TLS_ECDHE_RSA_WITH_RC4_128_SHA"));
    countTest(3, "date=-1&expression=" . uri_escape("$files&&tls.cipher==*RC4*"));
    countTest(2, "date=-1&expression=" . uri_escape("$files&&tls.cipher==tls_ECDHE_RSA_WITH_RC4_128_SHA"));

