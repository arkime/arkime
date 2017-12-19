use Test::More tests => 64;
use Cwd;
use URI::Escape;
use MolochTest;
use strict;

my $pwd = "*/pcap";
my $files = "(file=$pwd/openssl-ssl3.pcap||file=$pwd/openssl-tls1.pcap||file=$pwd/https3-301-get.pcap)";

# cert.alt tests
    countTest(2, "date=-1&expression=" . uri_escape("$files&&cert.alt==youtube.com"));
    countTest(2, "date=-1&expression=" . uri_escape("$files&&cert.alt==you*.com"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&cert.alt==youfoo.com"));

# cert.alt.cnt tests
    countTest(2, "date=-1&expression=" . uri_escape("$files&&cert.alt.cnt == 49"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&cert.alt.cnt == 48"));

# cert.cnt tests
    countTest(2, "date=-1&expression=" . uri_escape("$files&&cert.cnt == 3"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&cert.cnt == 1"));

# cert.issuer.cn
    countTest(2, "date=-1&expression=" . uri_escape("$files&&cert.issuer.cn==\"google internet authority g2\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&cert.issuer.cn==\"google internet authority g3\""));

# cert.issuer.cn
    countTest(2, "date=-1&expression=" . uri_escape("$files&&cert.issuer.on==\"*Google Inc*\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&cert.issuer.on==\"*Foo Inc*\""));
    countTest(2, "date=-1&expression=" . uri_escape("$files&&cert.issuer.on==\"*Google*\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&cert.issuer.on==\"*Foo*\""));

# cert.notafter
    countTest(2, "date=-1&expression=" . uri_escape("$files&&cert.notafter==\"2018/08/21 00:00:00\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&cert.notafter==\"2018/08/21 11:11:11\""));
    countTest(3, "date=-1&expression=" . uri_escape("$files&&cert.notafter>=\"2018/08/21 00:00:00\""));

# cert.notbefore
    countTest(2, "date=-1&expression=" . uri_escape("$files&&cert.notbefore==\"2014/09/24 06:08:05\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&cert.notbefore==\"2014/09/23 06:08:05\""));
    countTest(3, "date=-1&expression=" . uri_escape("$files&&cert.notbefore<=\"2014/09/24 06:08:05\""));

# cert.serial
    countTest(2, "date=-1&expression=" . uri_escape("$files&&cert.serial==7a5b0bd895632f87"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&cert.serial!=7a5b0bd895632f87"));

# cert.subject.cn
    countTest(2, "date=-1&expression=" . uri_escape("$files&&cert.subject.cn==\"google internet authority g2\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&cert.subject.cn==\"google internet authority g3\""));

# cert.subject.cn
    countTest(2, "date=-1&expression=" . uri_escape("$files&&cert.subject.on==\"Google Inc\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&cert.subject.on==\"Foo Inc\""));
    countTest(2, "date=-1&expression=" . uri_escape("$files&&cert.subject.on==\"*Google*\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&cert.subject.on==\"Foo\""));

# cert.validfor
    countTest(2, "date=-1&expression=" . uri_escape("$files&&cert.validfor==5936"));
    countTest(3, "date=-1&expression=" . uri_escape("$files&&cert.validfor>=5114"));

# cert.hash
    countTest(2, "date=-1&expression=" . uri_escape("$files&&cert.hash==0e:a3:27:7c:eb:7f:b2:8c:2b:5d:7d:d7:6b:e9:ba:1a:ec:0d:ff:91"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&cert.hash!=0e:a3:27:7c:eb:7f:b2:8c:2b:5d:7d:d7:6b:e9:ba:1a:ec:0d:ff:91"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&cert.hash==d*"));
