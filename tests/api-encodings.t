use Test::More tests => 216;
use Cwd;
use URI::Escape;
use MolochTest;
use Data::Dumper;
use JSON;
use Test::Differences;
use strict;
my $json;

my $prefix = int(rand()*100000);
my $token = getTokenCookie();

sub doTest {
    my ($encryption, $gzip, $shortheader) = @_;
    my ($tag, $cmd, $id, $json, $content, $result);

  ###### socks-http-pass.pcap
    $tag = "socks-$prefix-$encryption-$gzip-$shortheader";
    #diag $tag;
    $cmd = "../capture/capture -c config.test.ini -n test --copy -r pcap/socks-http-pass.pcap --tag $tag";
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

    $json = countTest(1, "date=-1&expression=" . uri_escape("tags=$tag && port.src == 54072"));
    $id = $json->{data}->[0]->{id};

    $json = countTest(1, "date=-1&expression=" . uri_escape("tags=$tag && port.src == 54068"));
    my $sid = $json->{data}->[0]->{id};

    $content = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/packets?line=false&ts=false&base=ascii")->content;

    # Test string crosses 2 packets
    ok ($content =~ /domain in examples without prior coordination or asking for permission/);

    # Scrub
    $result = viewerPostToken("/delete?removePcap=true&removeSpi=false&date=-1", "ids=$sid", $token);
    #diag Dumper($result);

    # Test again, not the same that was scrubbed
    $content = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/packets?line=false&ts=false&base=ascii")->content;
    #diag $content;

    # Test string crosses 2 packets
    ok ($content =~ /domain in examples without prior coordination or asking for permission/);

  ###### wireshark-bdat.pcap - test big packets
    $tag = "bdat-$prefix-$encryption-$gzip-$shortheader";
    #diag $tag;
    $cmd = "../capture/capture -c config.test.ini -n test --copy -r pcap/wireshark-bdat.pcap --tag $tag";
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

    $json = countTest(1, "date=-1&expression=" . uri_escape("tags=$tag"));
    $id = $json->{data}->[0]->{id};

    $content = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/packets?line=false&ts=false&base=ascii")->content;
    #diag $content;

    # Test string crosses 2 packets (/s matches across lines)
    ok ($content =~ /BDAT 8380 LAST.1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890/s);
}

### MAIN ###
foreach my $e (undef, "xor-2048", "aes-256-ctr") {
  foreach my $g (undef, 0, 8000, 64000) {
      foreach my $s ("true", "false") {
          doTest($e, $g, $s);
      }
  }
}
