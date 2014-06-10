#!/usr/bin/perl

use strict;
use HTTP::Request::Common;
use LWP::UserAgent;
use JSON;
use Data::Dumper;
use Test::More;
use Test::Differences;
use Cwd;
use URI::Escape;

$main::userAgent = LWP::UserAgent->new(timeout => 20);

################################################################################
sub doGeo {
    if (! -f "ipv4-address-space.csv") {
        system("wget https://www.iana.org/assignments/ipv4-address-space/ipv4-address-space.csv");
    }

    if (! -f "GeoIPASNum.dat") {
        system("wget http://www.maxmind.com/download/geoip/database/asnum/GeoIPASNum.dat.gz; gunzip GeoIPASNum.dat.gz");
    }

    if (! -f "GeoIP.dat") {
        system("wget http://www.maxmind.com/download/geoip/database/GeoLiteCountry/GeoIP.dat.gz; gunzip GeoIP.dat.gz");
    }

    if (! -f "plugins/test.so") {
        system("cd plugins ; make");
    }
}
################################################################################
sub doTests {
    my @files = @ARGV;
    @files = glob ("*.pcap") if ($#files == -1);

    plan tests => scalar @files;

    foreach my $filename (@files) {
        $filename = substr($filename, 0, -5) if ($filename =~ /\.pcap$/);
        die "Missing $filename.test" if (! -f "$filename.test");

        open my $fh, '<', "$filename.test" or die "error opening $filename.test: $!";
        my $savedData = do { local $/; <$fh> };
        my $savedJson = from_json($savedData, {relaxed => 1});

        my $cmd = "../capture/moloch-capture --tests -c config.test.ini -n test -r $filename.pcap 2>&1 1>/dev/null | ./tests.pl --fix";

        if ($main::valgrind) {
            $cmd = "G_SLICE=allows-malloc valgrind --leak-check=full --log-file=$filename.val " . $cmd;
        }

        if ($main::debug) {
            print "$cmd\n";
        }

        my $testData = `$cmd`;
        my $testJson = from_json($testData, {relaxed => 1});

        eq_or_diff($testJson, $savedJson, "$filename", { context => 3 });
    }
}
################################################################################
sub doFix {
    my $data = do { local $/; <> };
    my $json = from_json($data, {relaxed => 1});
    fix($json);
    $json = to_json($json, {pretty => 1});
    print $json, "\n";
}

################################################################################
sub fix {
my ($json) = @_;
    foreach my $packet (@{$json->{packets}}) {
        my $body = $packet->{body};

        delete $packet->{header}->{index}->{_id};
        if (exists $body->{ro}) {
            $body->{ro} = "SET";
        }
        foreach my $field ("a1", "a2", "dnsip", "socksip", "eip") {
            $body->{$field} = fixIp($body->{$field}) if (exists $body->{$field});
        }

        foreach my $field ("ta", "hh1", "hh2") {
            $body->{$field} = fixTags($json, $body->{$field}) if (exists $body->{$field});
        }
    }

    @{$json->{packets}} = sort {$a->{body}->{fpd} <=> $b->{body}->{fpd}} @{$json->{packets}};

    delete $json->{tags};
}

################################################################################
sub fixTags {
my ($json, $tags) = @_;
    my @list = ();
    foreach my $tag (@{$tags}) {
      push(@list, $json->{tags}->{$tag});
    }
    @list = sort(@list);
    return \@list;
}

################################################################################
sub fixIp {
    if(ref($_[0]) eq 'ARRAY') {
        my $ips = $_[0];
        my @list = ();
        foreach my $ip (@{$ips}) {
            push(@list, join '.', unpack 'C4', pack 'N', $ip);
        }
        return \@list;
    } else {
        return join '.', unpack 'C4', pack 'N', $_[0];
    }

}
################################################################################
sub doMake {
    foreach my $filename (@ARGV) {
        $filename = substr($filename, 0, -5) if ($filename =~ /\.pcap$/);
        if ($main::debug) {
          print("../capture/moloch-capture --tests -c config.test.ini -n test -r $filename.pcap 2>&1 1>/dev/null | ./tests.pl --fix > $filename.test\n");
        }
        system("../capture/moloch-capture --tests -c config.test.ini -n test -r $filename.pcap 2>&1 1>/dev/null | ./tests.pl --fix > $filename.test");
    }
}
################################################################################
sub viewerGet {
my ($url) = @_;

    my $response = $main::userAgent->get("http://localhost:8123$url");
    #print $response->content;
    my $json = from_json($response->content);
    return ($json);
}
################################################################################
sub countTest {
my ($count, $test, $debug) = @_;
    my $json = viewerGet("/sessions.json?$test");
    print Dumper($json) if ($debug);
    is ($json->{iTotalDisplayRecords}, $count, uri_unescape($test) . " iTotalDisplayRecords");
    is (scalar @{$json->{aaData}}, $count, uri_unescape($test) . " aaData count");
}
################################################################################
sub doViewer {
my ($cmd) = @_;

    plan tests => 736;

    die "Must run in tests directory" if (! -f "../db/db.pl");

    if ($cmd eq "--viewerfast") {
        print "Skipping ES Init and PCAP load\n";
        $main::userAgent->post("http://localhost:8123/flushCache");
        system("/bin/cp socks-http-example.pcap copytest.pcap");
        if ($main::debug) {
            system("../capture/moloch-capture --debug -c config.test.ini -n test -r copytest.pcap --tag testisfun");
        } else {
            system("../capture/moloch-capture -c config.test.ini -n test -r copytest.pcap --tag testisfun 2>&1 1>/dev/null");
        }
    } else {
        print ("Initializing ES\n");
        if ($main::debug) {
            system("../db/db.pl localhost:9200 initnoprompt");
        } else {
            system("../db/db.pl localhost:9200 initnoprompt 2>&1 1>/dev/null");
        }
        $main::userAgent->get("http://localhost:9200/_refresh");

        print ("Loading PCAP\n");
        system("/bin/cp socks-http-example.pcap copytest.pcap");
        my $cmd = "../capture/moloch-capture -c config.test.ini -n test -R .";

        if (!$main::debug) {
            $cmd .= " 2>&1 1>/dev/null";
        }

        if ($main::valgrind) {
            $cmd = "G_SLICE=allows-malloc valgrind --leak-check=full --log-file=moloch.val " . $cmd;
        }

        print "$cmd\n" if ($main::debug);
        system($cmd);

        print ("Starting viewer\n");
        if ($main::debug) {
            system("cd ../viewer ; node viewer.js -c ../tests/config.test.ini -n test &");
        } else {
            system("cd ../viewer ; node viewer.js -c ../tests/config.test.ini -n test > /dev/null &");
        }
        sleep 2;
    }

    $main::userAgent->get("http://localhost:9200/_flush");
    $main::userAgent->get("http://localhost:9200/_refresh");
    sleep 1;

    my $pwd = getcwd();
# file tests
    countTest(0, "date=-1&expression=file=nofile.pcap");
    countTest(3, "date=-1&expression=file=$pwd/bt-udp.pcap");
    countTest(1, "date=-1&expression=file=$pwd/bt-tcp.pcap");
    #TODO - countTest(4, "date=-1&expression=file=$pwd/bt-*.pcap");
    #TODO - countTest(4, "date=-1&expression=file=[$pwd/bt-udp.pcap,$pwd/bt-tcp.pcap]");
    countTest(2, "date=-1&expression=file=$pwd/dns-tcp.pcap");
    countTest(4, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/smtp-starttls.pcap)"));
# node tests
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&node==test"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&node==foobar"));
# asn tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn.dst==\"AS0000 This is neat\""));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn.dst==\"AS0000 This is bad\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn==\"AS0000 This is neat\""));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn==\"AS0000 This is bad\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn.src==\"AS0001 Cool Beans!\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn.src==\"Cool\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn==\"Cool\""));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn==\"Coo\""));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn==\"Coo*\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn==\"*Cool*\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn.src==\"*Cool*\""));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn==/Cool/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn==/.*Cool.*/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&asn.src==/.*Cool.*/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&test.ip.asn==/.*nea.*/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&test.ip.asn==*nea*"));
# country tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src==CAN"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src==can"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src!=CAN"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src!=can"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country==CAN"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country==can"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country!=CAN"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country!=can"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src==/CA.*/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src==/ca.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src!=/CA.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src!=/ca.*/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src==*AN"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src==*an"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src!=*AN"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&country.src!=*an"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&country==RUS"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&test.ip.country==RUS"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&country==Rus"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&test.ip.country==Rus"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&country!=RUS"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&test.ip.country!=RUS"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&country==/.*US/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&test.ip.country==/.*US/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&country==*US"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&test.ip.country==*US"));
# rir tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir==\"TEST\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir==\"test\""));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir!=\"test\""));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir!=test"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir==badrir"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir!=badrir"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src!=test"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.dst!=test"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src!=tes*"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.dst!=tes*"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src==\"TEST\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src==\"test\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src==TES*"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src==tes*"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src==/TES.*/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src==/tes.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src!=/TES.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&rir.src!=/tes.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/smtp-starttls.pcap)&&rir==[TEST,ARIN]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/smtp-starttls.pcap)&&rir==[TEST,ARIN,BADRIR]"));
# ip tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.src=10.0.0.2"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.src!=10.0.0.2"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=10.0.0.1"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst!=10.0.0.1"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=10.0.0.0/24"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=[10.0.0.0/24]"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=10.0.0"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=0"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst!=0"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&test.ip=10.0.0.1"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-tcp.pcap&&test.ip!=10.0.0.1"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=[10.0.0.1]"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=[10.0.0.1,10.0.0.3]"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=[10.0.0.1/32,10.0.0.3/32]"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip=[10.0.0.1/32]"));
# ip.protocol
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol=1"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol=6"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol=17"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol!=17"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol=icmp"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol=udp"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol!=udp"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol=tcp"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol=[tcp,6]"));
    countTest(4, "date=-1&expression=" . uri_escape("(file=$pwd/bt-udp.pcap||file=$pwd/bt-tcp.pcap)&&ip.protocol=[tcp,17]"));
# IP:Port tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.src=10.0.0.2:50759"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.src=10.0.0:50759"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.src=10.0.0.1/24:50759"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.src=10.0.0.2:50758"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.dst=10.0.0.2:50758"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip=10.0.0.2:50759"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip.src=[10.0.0.2:50759]"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&ip=[10.0.0.2:50759/32]"));
# port tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port.src=50759"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port.src!=50759"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port.src>=50759"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port.src>50759"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port.src<50759"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port.src<=50759"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port=50759"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port!=50759"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port>=50759"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port>50759"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port<50759"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&port<=10000"));
# packets tests
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&packets==1"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&packets.src==1"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&packets.dst==1"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&packets.dst==0"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&packets.dst!=1"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&packets.dst!=1"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets>0"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets>30"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets.src>17"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets.src<17"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets.src<=17"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets.src>=17"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets.src==17"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets.src!=17"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets.src==[17]"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&packets.src==[17,123]"));
# bytes tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes>11000"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes<11000"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes==10911"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes!=10911"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes.src>1900"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes.src<1900"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes.src==1912"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes.src!=1912"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes.dst>9200"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes.dst<9200"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes.dst==9215"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&bytes.dst!=9215"));
# databytes tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes>8928"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes<8928"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes==8929"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes!=8929"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes.src>656"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes.src<656"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes.src==646"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes.src!=646"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes.dst>8282"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes.dst<8282"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes.dst==8283"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-https-example.pcap&&databytes.dst!=8283"));
# tags tests
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&tags==nosuchtag"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&tags==nosuch*"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&tags==srcip"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&tags!=srcip"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&tags==srci*"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&tags==[srcip]"));
# protocols tests
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols==tcp"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols==socks"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols==http"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols==udp"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols==[socks,foo]"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols==[socks,tcp]"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols==*cp"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols==/.*ttp/"));
# protocols.cnt tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols.cnt==1"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols.cnt!=1"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols.cnt>1"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols.cnt>=1"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols.cnt<2"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-pass.pcap&&protocols.cnt<=2"));
# payload8 tests
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.hex!=64313a6164323a69"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.hex==64313a6164323a69"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.hex==\"64313A6164323A69\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.hex==*13A6164323A69"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.hex==/.*13A6164323A69/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.hex==[64313A6164323A69]"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.hex!=[64313A6164323A69]"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.hex==[64313A6164323A69,64313a71393a6669]"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.utf8=\"GET / HT\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.utf8=\"HTTP/1.1\""));
# payload8.src tests
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.src.hex!=64313a6164323a69"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.dst.hex==64313a6164323a69"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.src.hex==64313a6164323a69"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.src.hex==64313A6164323A69"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.src.hex==*13A6164323A69"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.src.hex==/.*13A6164323A69/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.src.hex==[64313A6164323A69]"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.src.hex!=[64313A6164323A69,64313a71393a6669]"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/bt-udp.pcap&&payload8.src.hex==[64313A6164323A69,64313a71393a6669]"));

    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.src.utf8=\"GET / HT\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.src.utf8=GET*"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.src.utf8=/GET.*/"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.src.utf8=/.*NOT.*/"));

# payload8.dst tests
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap&&payload8.dst.hex!=0500050000010ab4"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap&&payload8.dst.hex==0500050000010ab4"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap&&payload8.dst.hex==\"0500050000010Ab4\""));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap&&payload8.dst.hex==*0000010ab4"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap&&payload8.dst.hex==/.*50000010Ab4/"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap&&payload8.dst.hex==[0500050000010ab4]"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap&&payload8.dst.hex!=[0500050000010ab4,005adfb20ab49cf9]"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap&&payload8.dst.hex==[0500050000010ab4,005adfb20ab49cf9]"));

    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.utf8=HTTP/1.1"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.utf8=HTTP*"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.utf8=/.*TP.*/"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/http-301-get.pcap&&payload8.utf8=/.*NOT.*/"));

# dns.query.class tests
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.query.class==IN"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.query.class!=IN"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.query.class==*N"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.query.class==/IN/"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.query.class==[IN]"));
# dns.query.type tests
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.query.type==A"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.query.type!=A"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.query.type==*A"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.query.type==/A/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.query.type==[A]"));
# dns ip tests
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.ip==192.30.252.128"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.ip!=192.30.252.128"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.ip==192.30.252"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.ip==192.30.252.0/24"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.ip!=192.30.252"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.ip!=192.30.252.0/24"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&ip==192.30.252.128"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&ip!=192.30.252.128"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&ip==192.30.252"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&ip==192.30.252.0/24"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&ip!=192.30.252"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&ip!=192.30.252.0/24"));
# dns.status tests
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.status==NOERROR"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.status!=NOERROR"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.status==*ERROR"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.status==/.*ERROR/"));
    countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/dns-udp.pcap&&dns.status==[NOERROR]"));
# dns.host tests
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host==github.com"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host==mx.com"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host==[github.com,mx.com]"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host!=[github.com,mx.com]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host==*hub.com"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host==/.*hub.com/"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/dns-udp.pcap||file=$pwd/dns-mx.pcap)&&dns.host!=/.*hub.com/"));
# http.host tests
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==www.example.com"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==*.example.com"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==/.*xxx.com/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host!=www.example.com"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host!=[www.example.com,foo.com]"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==[www.example.com,foo.com]"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==www.EXample.com"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==*.EXample.com"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==/.*XXx.com/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host!=www.EXample.com"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host!=[www.EXample.com,foo.com]"));
    countTest(3, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks-http-example.pcap)&&http.host==[www.EXample.com,foo.com]"));
# http.method tests
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.method==GET"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.method!=GET"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.method==get"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.method==[GET,HEAD]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.method==[\"GET\",\"HEAD\"]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.method!=[GET,HEAD]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.method==/.*E.*/"));
# http.uri tests
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==//samples.example.com/UpdataConfig.dat"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==//samples.example.com"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==UpdataConfig.dat"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==*Config.dat"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==Config.dat"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==*config.dat"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==config.dat"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==a.zip"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/http-500-head.pcap)&&http.uri==/.*a.zip/"));
# http.hasheader, http.hasheader.src, http.hasheader.dst tests
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader==server"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.dst==server"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.src==cookie"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader==ser*"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.dst==ser*"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.src==cook*"));
    #TODO countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader==/ser.*/"));
    #TODO countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.dst==/ser.*/"));
    #TODO tTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.src==/ser.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader==[server]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.dst==[server]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.src==[cookie]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader==SeRver"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.dst==SeRver"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.src==CookIe"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader==*VeR"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.dst==*VeR"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.src==*kIe"));
    #TODO countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader==/.*VeR/"));
    #TODO countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.dst==/.*VeR/"));
    #TODO tTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.src==/.*kIe/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader==[SeRver]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.dst==[SeRver]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.src==[CooKie]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader==[content-length]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.dst==[content-length]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.hasheader.src==[accept-encoding]"));
# http.version tests
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version==1.1"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.src==1.1"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.dst==1.1"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version==not"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.src==fudge"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.dst==paste"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version==[1.1]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.src==[1.1]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.dst==[1.1]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version==1.*"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.src==1.*"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.dst==1.*"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version==/1.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.src==/1.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.version.dst==/1.*/"));
# http.user-agent tests
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent==\"Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322)\""));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent!=\"Mozilla/4.0\""));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent==\"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_8_5) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/31.0.1650.63 Safari/537.36\""));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent!=\"Mozilla/5.0\""));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent==Mozilla"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent==mozilla"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent==*Mozilla*"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent==*mozilla*"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent==/.*Mozilla.*/"));
    countTest(0, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent==/.*mozilla.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent==[Mozilla]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.user-agent==[mozilla]"));
# http.md5 tests
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5=40be8f5100e9beabab293c9d7bacaff0"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5=40Be8f5100e9beabab293c9d7bacaff0"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5=40be8f5100e9beabab293c9d7*"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5=40Be8f5100e9beabab293c9d7*"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5=/40be8f5100e9beabab293c9d7.*/"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5=/40Be8f5100e9beabab293c9d7.*/"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5!=40be8f5100e9beabab293c9d7bacaff0"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5!=40Be8f5100e9beabab293c9d7bacaff0"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5=[40be8f5100e9beabab293c9d7bacaff0,b0cecae354b9eab1f04f70e46a612cb1]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5=[40Be8f5100e9beabab293c9d7bacaff0,B0cecae354b9eab1f04f70e46a612cb1]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5!=[40be8f5100e9beabab293c9d7bacaff0,b0cecae354b9eab1f04f70e46a612cb1]"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/http-content-zip.pcap||file=$pwd/socks5-reverse.pcap)&&http.md5!=[40Be8f5100e9beabab293c9d7bacaff0,B0cecae354b9eab1f04f70e46a612cb1]"));
# session.segments tests
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/long-session.pcap||file=$pwd/socks5-reverse.pcap)&&session.segments=2"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/long-session.pcap||file=$pwd/socks5-reverse.pcap)&&session.segments=1"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/long-session.pcap||file=$pwd/socks5-reverse.pcap)&&session.segments=[2]"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/long-session.pcap||file=$pwd/socks5-reverse.pcap)&&session.segments!=[2]"));
# sessions.length tests
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/long-session.pcap||file=$pwd/socks5-reverse.pcap)&&session.length=908493"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/long-session.pcap||file=$pwd/socks5-reverse.pcap)&&session.length>=908493"));
    countTest(1, "date=-1&expression=" . uri_escape("(file=$pwd/long-session.pcap||file=$pwd/socks5-reverse.pcap)&&session.length<908493"));
    countTest(2, "date=-1&expression=" . uri_escape("(file=$pwd/long-session.pcap||file=$pwd/socks5-reverse.pcap)&&session.length=[908493,908494]"));

# adding/removing tags test expression
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/copytest.pcap"));
    countTest(0, "date=-1&expression=" . uri_escape("tags==COPYTEST1"));
    $main::userAgent->post("http://localhost:8123/addTags?date=-1&expression=file=$pwd/copytest.pcap", Content => "tags=COPYTEST1");
    $main::userAgent->get("http://localhost:9200/_refresh");
    countTest(3, "date=-1&expression=" . uri_escape("tags==COPYTEST1"));
    $main::userAgent->post("http://localhost:8123/removeTags?date=-1&expression=file=$pwd/copytest.pcap", Content => "tags=COPYTEST1");
    $main::userAgent->get("http://localhost:9200/_refresh");
    countTest(0, "date=-1&expression=" . uri_escape("tags==COPYTEST1"));

# adding/removing tags test ids
    my $idQuery = from_json($main::userAgent->get("http://localhost:8123/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/copytest.pcap"))->content);
    $main::userAgent->post("http://localhost:8123/addTags?date=-1", Content => "tags=COPYTEST1&ids=" . $idQuery->{aaData}->[0]->{id});
    $main::userAgent->get("http://localhost:9200/_refresh");
    countTest(1, "date=-1&expression=" . uri_escape("tags==COPYTEST1"));
    $main::userAgent->post("http://localhost:8123/removeTags?date=-1", Content => "tags=COPYTEST1&ids=" . $idQuery->{aaData}->[0]->{id});
    $main::userAgent->get("http://localhost:9200/_refresh");
    countTest(0, "date=-1&expression=" . uri_escape("tags==COPYTEST1"));
    $main::userAgent->post("http://localhost:8123/addTags?date=-1", Content => "tags=COPYTEST1&ids=" . $idQuery->{aaData}->[0]->{id});

# csv =
    my $csv = $main::userAgent->get("http://localhost:8123/sessions.csv?date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap"))->content;
    $csv =~ s/\r//g;
    is ($csv, 'Protocol, First Packet, Last Packet, Source IP, Source Port, Source Geo, Destination IP, Destination Port, Destination Geo, Packets, Bytes, Data Bytes, Node
tcp, 1386004309, 1386004309, 10.180.156.185, 53533, USA, 10.180.156.249, 1080, USA, 14, 2698, 1754, test
tcp, 1386004312, 1386004312, 10.180.156.185, 53534, USA, 10.180.156.249, 1080, USA, 15, 2780, 1770, test
tcp, 1386004317, 1386004317, 10.180.156.185, 53535, USA, 10.180.156.249, 1080, USA, 17, 2905, 1763, test
', "CSV Expression");
   
    $csv = $main::userAgent->get("http://localhost:8123/sessions.csv?date=-1&ids=" . $idQuery->{aaData}->[0]->{id})->content;
    $csv =~ s/\r//g;
    is ($csv, 'Protocol, First Packet, Last Packet, Source IP, Source Port, Source Geo, Destination IP, Destination Port, Destination Geo, Packets, Bytes, Data Bytes, Node
tcp, 1386004309, 1386004309, 10.180.156.185, 53533, USA, 10.180.156.249, 1080, USA, 14, 2698, 1754, test
', "CSV Ids");

# scrub tags test ids
    if (defined $idQuery->{aaData}->[0]->{id}) {
        $main::userAgent->post("http://localhost:8123/scrub?date=-1", Content => "ids=" . $idQuery->{aaData}->[0]->{id});
        $main::userAgent->get("http://localhost:9200/_refresh");
        countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/copytest.pcap&&scrubbed.by==anonymous"));
        countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/copytest.pcap&&scrubbed.by!=anonymous"));
        countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/copytest.pcap&&scrubbed.by==Anonymous"));
        countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/copytest.pcap&&scrubbed.by==[Anonymous]"));
        countTest(2, "date=-1&expression=" . uri_escape("file=$pwd/copytest.pcap&&scrubbed.by!=[Anonymous]"));
        countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/copytest.pcap&&scrubbed.by==Anon*mous"));
        countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/copytest.pcap&&scrubbed.by==/Anon.*mous/"));
    } else {
        diag "No scrubid, so skipping scrub\n";
    }

# delete tags test expression
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/copytest.pcap"));
    $main::userAgent->post("http://localhost:8123/delete?date=-1&expression=file=$pwd/copytest.pcap");
    $main::userAgent->get("http://localhost:9200/_refresh");
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/copytest.pcap"));

# users
    my $usersPage = $main::userAgent->get("http://localhost:8123/users")->content;
    $usersPage =~ /token.*value: "(.*)"/;
    my $token = $1;
    my $users = from_json($main::userAgent->post("http://localhost:8123/users.json")->content);
    is (@{$users->{aaData}}, 0, "Empty users table");

    my $json = from_json($main::userAgent->post("http://localhost:8123/addUser", Content => "token=$token&userId=test1&userName=UserName&enabled=on&password=password")->content);
    $main::userAgent->get("http://localhost:9200/_refresh");
    $users = from_json($main::userAgent->post("http://localhost:8123/users.json")->content);
    is (@{$users->{aaData}}, 1, "Single user");

    my $json = from_json($main::userAgent->post("http://localhost:8123/deleteUser/test1", Content => "token=$token")->content);
    $main::userAgent->get("http://localhost:9200/_refresh");
    $users = from_json($main::userAgent->post("http://localhost:8123/users.json")->content);
    is (@{$users->{aaData}}, 0, "Removed user");

# stats.json
    my $stats = viewerGet("/stats.json");
    is (@{$stats->{aaData}}, 1, "stats.json aaData set ");
    is ($stats->{iTotalRecords}, 1, "stats.json iTotalRecords");
    is ($stats->{aaData}->[0]->{id}, "test", "stats.json name");
    foreach my $i ("diskQueue", "deltaDroppedPerSec") {
        is ($stats->{aaData}->[0]->{$i}, 0, "stats.json $i 0");
    }

    foreach my $i ("monitoring", "diskQueue", "deltaDropped", "deltaDroppedPerSec") {
        is ($stats->{aaData}->[0]->{$i}, 0, "stats.json $i == 0");
    }

    foreach my $i ("deltaMS", "totalPackets", "deltaSessions", "deltaPackets", "deltaBytes", "memory", "currentTime", "totalK", "totalSessions", "freeSpaceM", "deltaSessionsPerSec", "deltaBytesPerSec", "deltaPacketsPerSec") {
        cmp_ok ($stats->{aaData}->[0]->{$i}, '>', 0, "stats.json $i > 0");
    }

# dstats.json
    my $dstats = viewerGet("/dstats.json?nodeName=test&start=1399680425&stop=1399680460&step=5&interval=5&name=deltaPackets");
    is (@{$dstats}, 7, "dstats.json array size");

# Cleanup
    unlink("copytest.pcap");

    if ($cmd eq "--viewer") {
        $main::userAgent->post("http://localhost:8123/shutdown");
    }

    if ($main::debug) {
        system("../db/db.pl localhost:9200 rm $pwd/copytest.pcap");
    } else {
        system("../db/db.pl localhost:9200 rm $pwd/copytest.pcap 2>&1 1>/dev/null");
    }
}
################################################################################
$main::debug = 0;
$main::valgrind = 0;
$main::cmd = "--capture";

while (scalar (@ARGV) > 0) {
    if ($ARGV[0] eq "--debug") {
        $main::debug = 1;
        shift @ARGV;
    } elsif ($ARGV[0] eq "--valgrind") {
        $main::valgrind = 1;
        shift @ARGV;
    } elsif ($ARGV[0] =~ /^--(viewer|fix|make|capture|viewerfast|help)$/) {
        $main::cmd = $ARGV[0];
        shift @ARGV;
    } elsif ($ARGV[0] =~ /^-/) {
        print "Unknown option $ARGV[0]\n";
        $main::cmd = "--help";
        last;
    } else {
        last;
    }
}

if ($main::cmd eq "--fix") {
    doFix();
} elsif ($main::cmd eq "--make") {
    doMake();
} elsif ($main::cmd eq "--help") {
    print "$ARGV[0] [OPTIONS] [COMMAND] <pcap> files\n";
    print "Options:\n";
    print "  --debug       Turn on debuggin\n";
    print "  --valgrind    Use valgrind on capture\n";
    print "\n";
    print "Commands:\n";
    print "  --help        This help\n";
    print "  --make        Create a .test file for each .pcap file on command line\n";
    print "  --viewer      viewer tests\n";
    print "                This will init local ES, import data, start a viewer, run tests\n";
    print " [default]      Run each .pcap file thru ../capture/moloch-capture and compare to .test file\n";
} elsif ($main::cmd =~ "^--viewer") {
    doGeo();
    setpgrp $$, 0;
    doViewer($main::cmd);
} else {
    doGeo();
    doTests();
}
