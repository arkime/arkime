use Test::More tests => 120;
use Cwd;
use URI::Escape;
use MolochTest;
use Data::Dumper;
use JSON;
use Test::Differences;
use strict;
my $json;

my $prefix = int(rand()*100000);
sub doTest {
    my ($encryption, $gzip, $shortheader) = @_;
    my $tag = "$prefix-$encryption-$gzip-$shortheader";
    #diag $tag;
    my $cmd = "../capture/capture -c config.test.ini -n test --copy -r pcap/socks-http-pass.pcap --tag $tag";
    if (defined $encryption) {
        $cmd .= " -o simpleEncoding=$encryption -o simpleKEKId=test";
    }
    if (defined $gzip) {
        $cmd .= " -o simpleGzipBlockSize=$gzip";
    }
    if (defined $shortheader) {
        $cmd .= " -o simpleShortHeader=$shortheader";
    }
    #diag "$cmd\n";
    system($cmd);

    countTest(3, "date=-1&expression=" . uri_escape("tags=$tag"));
    my $json = countTest(1, "date=-1&expression=" . uri_escape("tags=$tag && port.src == 54072"));

    my $id = $json->{data}->[0]->{id};
    my $content = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/packets?line=false&ts=false&base=ascii")->content;
    #diag $content;

    # Test string crosses 2 packets
    ok ($content =~ /domain in examples without prior coordination or asking for permission/);
}

### MAIN ###
foreach my $e (undef, "xor-2048", "aes-256-ctr") {
  foreach my $g (undef, 0, 8000, 64000) {
      foreach my $s ("true", "false") {
          doTest($e, $g, $s);
      }
  }
}
