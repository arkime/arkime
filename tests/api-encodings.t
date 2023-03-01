use Test::More tests => 325;
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
my $es = "-o 'elasticsearch=$MolochTest::elasticsearch' $ENV{INSECURE}";

sub doTest {
    my ($encryption, $compression, $blocksize, $shortheader) = @_;
    my ($cmd, $id, $json, $content, $result);

    my $stag = "socks-$prefix-$encryption-$compression-$blocksize-$shortheader";
    my $btag = "bdat-$prefix-$encryption-$compression-$blocksize-$shortheader";

    #diag $btag;

  ###### wireshark-bdat.pcap - run
    $cmd = "../capture/capture $es -c config.test.ini -n test --copy -r pcap/wireshark-bdat.pcap --tag $btag -o simpleCompression=$compression";
    if (defined $encryption) {
        $cmd .= " -o simpleEncoding=$encryption -o simpleKEKId=test";
    }
    if (defined $blocksize) {
        $cmd .= " -o simpleCompressionBlockSize=$blocksize";
    }
    if (defined $shortheader) {
        $cmd .= " -o simpleShortHeader=$shortheader";
    }
    #diag "$cmd\n";
    system("$cmd");

  ###### socks-http-pass.pcap - run
    $cmd = "../capture/capture $es -c config.test.ini -n test --copy -r pcap/socks-http-pass.pcap --tag $stag -o simpleCompression=$compression";
    if (defined $encryption) {
        $cmd .= " -o simpleEncoding=$encryption -o simpleKEKId=test";
    }
    if (defined $blocksize) {
        $cmd .= " -o simpleCompressionBlockSize=$blocksize";
    }
    if (defined $shortheader) {
        $cmd .= " -o simpleShortHeader=$shortheader";
    }
    #diag "$cmd\n";
    system($cmd);

  ###### socks-http-pass.pcap - test
    $json = countTest(1, "date=-1&expression=" . uri_escape("tags=$stag && port.src == 54072"));
    $id = $json->{data}->[0]->{id};

    $json = countTest(1, "date=-1&expression=" . uri_escape("tags=$stag && port.src == 54068"));
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
    esGet("/_refresh");
    $json = countTest(1, "date=-1&expression=" . uri_escape("tags=$btag"));
    $id = $json->{data}->[0]->{id};

    $content = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/session/$id/packets?line=false&ts=false&base=ascii")->content;
    #diag $content;

    # Test string crosses 2 packets (/s matches across lines)
    ok ($content =~ /BDAT 8380 LAST.1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890/s);
}

### MAIN ###
foreach my $e (undef, "xor-2048", "aes-256-ctr") {
    foreach my $c ("none", "gzip", "zstd") {
        foreach my $b (8000, 64000) {
            foreach my $s ("true", "false") {
                doTest($e, $c, $b, $s);
            }
        }
    }
}

# Test decode by find the last encrypted  non compressed file
$json = esGet("/tests_files/_search?q=node:test&sort=num:desc&size=20");
foreach my $item (@{$json->{hits}->{hits}}) {
    next if (exists $item->{_source}->{uncompressedBits});
    next if (!exists $item->{_source}->{dek});
    my $cmd = "(cd ../viewer; node decryptPcap.js $es -n test -c ../tests/config.test.ini $item->{_source}->{name} > $item->{_source}->{name}.pcap)";
    system($cmd);
    open my $in, '<', "$item->{_source}->{name}.pcap" or die "error opening $item->{_source}->{name}.pcap $!";
    read ($in, my $data, 4);
    is(unpack("H*", $data), "d4c3b2a1");
    close ($in);
    unlink("$item->{_source}->{name}.pcap");
    last;
}
