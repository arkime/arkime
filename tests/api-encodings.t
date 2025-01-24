use Test::More tests => 325;
use Cwd;
use URI::Escape;
use ArkimeTest;
use Data::Dumper;
use JSON;
use Test::Differences;
use strict;
my $json;

my $prefix = int(rand()*100000);
my $token = getTokenCookie();

sub doTest {
    my ($encryption, $compression, $blocksize, $shortheader) = @_;
    my ($cmd, $id, $json, $content, $result);

    my $stag = "socks-$prefix-$encryption-$compression-$blocksize-$shortheader";
    my $btag = "bdat-$prefix-$encryption-$compression-$blocksize-$shortheader";

    #diag $btag;

  ###### wireshark-bdat.pcap - run
    $cmd = "../capture/capture --norefresh $ArkimeTest::es $ENV{SCHEME} -c config.test.ini -n test --copy -r pcap/wireshark-bdat.pcap --tag $btag -o simpleCompression=$compression";
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
    system("$cmd &");

  ###### socks-http-pass.pcap - run
    $cmd = "../capture/capture --norefresh $ArkimeTest::es $ENV{SCHEME} -c config.test.ini -n test --copy -r pcap/socks-http-pass.pcap --tag $stag -o simpleCompression=$compression";
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

    # Run some captures in foreground, most in background
    if ($compression eq "zstd" && $blocksize == 64000 && $shortheader eq "false") {
        # diag "$stag - WAIT";
        system("$cmd");
        esGet("/_flush");
        esGet("/_refresh");
    } else {
        #diag "$stag";
        system("$cmd &");
    }
}

sub doCheck {
    my ($encryption, $compression, $blocksize, $shortheader) = @_;
    my ($cmd, $id, $json, $content, $result);

    my $stag = "socks-$prefix-$encryption-$compression-$blocksize-$shortheader";
    my $btag = "bdat-$prefix-$encryption-$compression-$blocksize-$shortheader";

    #diag $btag;

  ###### socks-http-pass.pcap - test
    $json = countTest(1, "date=-1&expression=" . uri_escape("tags=$stag && port.src == 54072"));
    $id = $json->{data}->[0]->{id};

    $json = countTest(1, "date=-1&expression=" . uri_escape("tags=$stag && port.src == 54068"));
    my $sid = $json->{data}->[0]->{id};

    $content = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=false&ts=false&base=ascii")->content;

    # Test string crosses 2 packets
    ok ($content =~ /domain in examples without prior coordination or asking for permission/);

    # Scrub
    $result = viewerPostToken("/delete?removePcap=true&removeSpi=false&date=-1", "ids=$sid", $token);
    #diag Dumper($result);

    # Test again, not the same that was scrubbed
    $content = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=false&ts=false&base=ascii")->content;
    #diag $content;

    # Test string crosses 2 packets
    ok ($content =~ /domain in examples without prior coordination or asking for permission/);

  ###### wireshark-bdat.pcap - test big packets
    $json = countTest(1, "date=-1&expression=" . uri_escape("tags=$btag"));
    $id = $json->{data}->[0]->{id};

    $content = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$id/packets?line=false&ts=false&base=ascii")->content;
    #diag $content;

    # Test string crosses 2 packets (/s matches across lines)
    ok ($content =~ /BDAT 8380 LAST.1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890/s);
}

### MAIN ###

# Load all the pcap, running some in background
foreach my $e (undef, "xor-2048", "aes-256-ctr") {
    foreach my $c ("none", "gzip", "zstd") {
        foreach my $b (8000, 64000) {
            foreach my $s ("true", "false") {
                doTest($e, $c, $b, $s);
            }
        }
    }
}

# Wait for everything to complete
sleep(3);
esGet("/_flush");
esGet("/_refresh");

# Check the results
foreach my $e (undef, "xor-2048", "aes-256-ctr") {
    foreach my $c ("none", "gzip", "zstd") {
        foreach my $b (8000, 64000) {
            foreach my $s ("true", "false") {
                doCheck($e, $c, $b, $s);
            }
        }
    }
}

# Test decode by find the last encrypted  non compressed file
$json = esGet("/tests_files/_search?q=node:test&sort=num:desc&size=20");
foreach my $item (@{$json->{hits}->{hits}}) {
    next if (exists $item->{_source}->{uncompressedBits});
    next if (!exists $item->{_source}->{dek});
    my $cmd = "(cd ../viewer; node decryptPcap.js $ArkimeTest::es -n test -c ../tests/config.test.ini $item->{_source}->{name} > $item->{_source}->{name}.pcap)";
    #diag $cmd;
    system($cmd);
    open my $in, '<', "$item->{_source}->{name}.pcap" or die "error opening $item->{_source}->{name}.pcap $!";
    read ($in, my $data, 4);
    ok(unpack("H*", $data) eq "d4c3b2a1" || unpack("H*", $data) eq "d5c3b2a1");
    close ($in);
    unlink("$item->{_source}->{name}.pcap");
    last;
}

# Delete the files entries
esPost("/tests_files/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "wildcard": { "name": "/tmp/test-*" } } }');
