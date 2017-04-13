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
use TAP::Harness;
use MolochTest;

$main::userAgent = LWP::UserAgent->new(timeout => 20);

my $ELASTICSEARCH = $ENV{ELASTICSEARCH} = "http://127.0.0.1:9200";
#my $ELASTICSEARCH = $ENV{ELASTICSEARCH} = "http://elastic:changeme\@127.0.0.1:9200";

################################################################################
sub doGeo {
    if (! -f "ipv4-address-space.csv") {
        # Certs are hard MKAY
        system("wget --no-check-certificate https://www.iana.org/assignments/ipv4-address-space/ipv4-address-space.csv");
    }

    if (! -f "GeoIPASNum.dat") {
        system("wget http://www.maxmind.com/download/geoip/database/asnum/GeoIPASNum.dat.gz; gunzip GeoIPASNum.dat.gz");
    }

    if (! -f "GeoIPASNumv6.dat") {
        system("wget http://download.maxmind.com/download/geoip/database/asnum/GeoIPASNumv6.dat.gz; gunzip GeoIPASNumv6.dat.gz");
    }

    if (! -f "GeoIP.dat") {
        system("wget http://www.maxmind.com/download/geoip/database/GeoLiteCountry/GeoIP.dat.gz; gunzip GeoIP.dat.gz");
    }

    if (! -f "GeoIPv6.dat") {
        system("wget http://geolite.maxmind.com/download/geoip/database/GeoIPv6.dat.gz; gunzip GeoIPv6.dat.gz");
    }

    if (! -f "plugins/test.so" || (stat('../capture/moloch.h'))[9] > (stat('plugins/test.so'))[9]) {
        system("cd plugins ; make");
    }
}
################################################################################
sub sortJson {
    my ($json) = @_;

    foreach my $session (@{$json->{sessions}}) {
        my $body = $session->{body};
        foreach my $i ("dnsip", "tags-term", "ta") {
            if (exists $body->{$i}) {
                my @tmp = sort (@{$body->{$i}});
                $body->{$i} = \@tmp;
            }
        }
    }
    return $json;
}
################################################################################
sub doTests {
    my @files = @ARGV;
    @files = glob ("pcap/*.pcap") if ($#files == -1);

    plan tests => scalar @files;

    foreach my $filename (@files) {
        $filename = substr($filename, 0, -5) if ($filename =~ /\.pcap$/);
        die "Missing $filename.test" if (! -f "$filename.test");

        open my $fh, '<', "$filename.test" or die "error opening $filename.test: $!";
        my $savedData = do { local $/; <$fh> };
        my $savedJson = sortJson(from_json($savedData, {relaxed => 1}));


        my $cmd = "../capture/moloch-capture --tests -c config.test.ini -n test -r $filename.pcap 2>&1 1>/dev/null | ./tests.pl --fix";

        if ($main::valgrind) {
            $cmd = "G_SLICE=always-malloc valgrind --leak-check=full --log-file=$filename.val " . $cmd;
        }

        if ($main::debug) {
            print "$cmd\n";
        }

        my $testData = `$cmd`;
        my $testJson = sortJson(from_json($testData, {relaxed => 1}));
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
    foreach my $session (@{$json->{sessions}}) {
        my $body = $session->{body};

        delete $session->{header}->{index}->{_id};
        if (exists $body->{ro}) {
            $body->{ro} = "SET";
        }
        if (exists $body->{timestamp}) {
            $body->{timestamp} = "SET";
        }
        foreach my $field ("a1", "a2", "dnsip", "socksip", "eip") {
            $body->{$field} = fixIp($body->{$field}) if (exists $body->{$field});
        }
        if ($body->{radius}) {
            foreach my $field ("eip", "fip") {
                $body->{radius}->{$field} = fixIp($body->{radius}->{$field}) if (exists $body->{radius}->{$field});
            }
        }

        foreach my $field ("ta", "hh1", "hh2") {
            $body->{$field} = fixTags($json, $body->{$field}) if (exists $body->{$field});
        }
    }

    @{$json->{sessions}} = sort {$a->{body}->{fpd} <=> $b->{body}->{fpd}} @{$json->{sessions}};

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
sub ip2bin {
    my @parts = split(/\./, $_[0]);
    return pack('H*', sprintf("%02x%02x%02x%02x", $parts[0], $parts[1], $parts[2], $parts[3]));
}
################################################################################
sub doReip {
    open my $in, '<', "$ARGV[0]" or die "error opening $ARGV[0]: $!";
    open my $out, '>', "$ARGV[0].tmp" or die "error opening $ARGV[0].tmp: $!";
    binmode($in);
    binmode($out);

    my $buf;
    read($in, $buf, 1000000);

    for (my $i = 1; $i < $#ARGV; $i+=2) {
        my $src = ip2bin($ARGV[$i]);
        my $dst = ip2bin($ARGV[$i+1]);
        $buf =~ s/\Q$src\E/$dst/g;
    }

    syswrite($out, $buf);

    close($in);
    close($out);
}
################################################################################
sub doViewer {
my ($cmd) = @_;

    die "Must run in tests directory" if (! -f "../db/db.pl");

    if ($cmd eq "--viewernostart") {
        print "Skipping ES Init and PCAP load\n";
        $main::userAgent->post("http://localhost:8123/flushCache");
    } elsif ($cmd eq "--viewerstart" || $cmd eq "--viewerhang") {
        print "Skipping ES Init and PCAP load\n";
        $main::userAgent->post("http://localhost:8123/flushCache");
        print ("Starting viewer\n");
        if ($main::debug) {
            system("cd ../capture/plugins/wiseService ; node wiseService.js -c ../../../tests/config.test.ini > /tmp/moloch.wise &");
            system("cd ../viewer ; node multies.js -c ../tests/config.test.ini -n all --debug > /tmp/multies.all &");
            system("cd ../viewer ; node viewer.js -c ../tests/config.test.ini -n test --debug > /tmp/moloch.test &");
            system("cd ../viewer ; node viewer.js -c ../tests/config.test.ini -n test2 --debug > /tmp/moloch.test2 &");
            system("cd ../viewer ; node viewer.js -c ../tests/config.test.ini -n all --debug > /tmp/moloch.all &");
        } else {
            system("cd ../capture/plugins/wiseService ; node wiseService.js -c ../../../tests/config.test.ini > /dev/null &");
            system("cd ../viewer ; node multies.js -c ../tests/config.test.ini -n all > /dev/null &");
            system("cd ../viewer ; node viewer.js -c ../tests/config.test.ini -n test > /dev/null &");
            system("cd ../viewer ; node viewer.js -c ../tests/config.test.ini -n test2 > /dev/null &");
            system("cd ../viewer ; node viewer.js -c ../tests/config.test.ini -n all > /dev/null &");
        }
        sleep 1;
        sleep (10000) if ($cmd eq "--viewerhang");
    } else {
        print ("Initializing ES\n");
        if ($main::debug) {
            system("../db/db.pl --prefix tests $ELASTICSEARCH initnoprompt");
            system("../db/db.pl --prefix tests2 $ELASTICSEARCH initnoprompt");
        } else {
            system("../db/db.pl --prefix tests $ELASTICSEARCH initnoprompt 2>&1 1>/dev/null");
            system("../db/db.pl --prefix tests2 $ELASTICSEARCH initnoprompt 2>&1 1>/dev/null");
        }

        print ("Loading tagger\n");
        system("../capture/plugins/taggerUpload.pl $ELASTICSEARCH ip ip.tagger1.json iptaggertest1");
        system("../capture/plugins/taggerUpload.pl $ELASTICSEARCH host host.tagger1.json hosttaggertest1");
        system("../capture/plugins/taggerUpload.pl $ELASTICSEARCH md5 md5.tagger1.json md5taggertest1");
        system("../capture/plugins/taggerUpload.pl $ELASTICSEARCH ip ip.tagger2.json iptaggertest2");
        system("../capture/plugins/taggerUpload.pl $ELASTICSEARCH host host.tagger2.json hosttaggertest2");
        system("../capture/plugins/taggerUpload.pl $ELASTICSEARCH md5 md5.tagger2.json md5taggertest2");
        system("../capture/plugins/taggerUpload.pl $ELASTICSEARCH email email.tagger2.json emailtaggertest2");
        system("../capture/plugins/taggerUpload.pl $ELASTICSEARCH uri uri.tagger2.json uritaggertest2");

        # Start Wise
        system("cd ../capture/plugins/wiseService ; node wiseService.js -c ../../../tests/config.test.ini > /tmp/moloch.wise &");

        sleep 1;
        $main::userAgent->get("$ELASTICSEARCH/_flush");
        $main::userAgent->get("$ELASTICSEARCH/_refresh");

        print ("Loading PCAP\n");

        my $cmd = "../capture/moloch-capture -c config.test.ini -n test -R pcap --flush";
        if (!$main::debug) {
            $cmd .= " 2>&1 1>/dev/null";
        } else {
            $cmd .= " --debug 2>&1 1>/tmp/moloch.capture";
        }


        if ($main::valgrind) {
            $cmd = "G_SLICE=always-malloc valgrind --leak-check=full --log-file=moloch.val " . $cmd;
        }

        print "$cmd\n" if ($main::debug);
        system($cmd);

        esCopy("tests_fields", "tests2_fields", "field");

        print ("Starting viewer\n");
        if ($main::debug) {
            system("cd ../viewer ; node multies.js -c ../tests/config.test.ini -n all --debug > /tmp/multies.all &");
            system("cd ../viewer ; node viewer.js -c ../tests/config.test.ini -n test --debug > /tmp/moloch.test &");
            system("cd ../viewer ; node viewer.js -c ../tests/config.test.ini -n test2 --debug > /tmp/moloch.test2 &");
            system("cd ../viewer ; node viewer.js -c ../tests/config.test.ini -n all --debug > /tmp/moloch.all &");
        } else {
            system("cd ../viewer ; node multies.js -c ../tests/config.test.ini -n all > /dev/null &");
            system("cd ../viewer ; node viewer.js -c ../tests/config.test.ini -n test > /dev/null &");
            system("cd ../viewer ; node viewer.js -c ../tests/config.test.ini -n test2 > /dev/null &");
            system("cd ../viewer ; node viewer.js -c ../tests/config.test.ini -n all > /dev/null &");
        }
        sleep 1;
    }

    $main::userAgent->get("$ELASTICSEARCH/_flush");
    $main::userAgent->get("$ELASTICSEARCH/_refresh");
    sleep 1;

    my $harness = TAP::Harness->new();

    my @tests = @ARGV;
    @tests = glob ("*.t") if ($#tests == -1);
    $harness->runtests(@tests);


# Cleanup
    if ($cmd ne "--viewernostart") {
        $main::userAgent->post("http://localhost:8123/shutdown");
        $main::userAgent->post("http://localhost:8124/shutdown");
        $main::userAgent->post("http://localhost:8125/shutdown");
        $main::userAgent->post("http://localhost:8200/shutdown");
        $main::userAgent->post("http://localhost:8081/shutdown");
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
    } elsif ($ARGV[0] =~ /^--(viewer|fix|make|capture|viewernostart|viewerstart|viewerhang|help|reip)$/) {
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
} elsif ($main::cmd eq "--reip") {
    doReip();
} elsif ($main::cmd eq "--help") {
    print "$ARGV[0] [OPTIONS] [COMMAND] <pcap> files\n";
    print "Options:\n";
    print "  --debug       Turn on debuggin\n";
    print "  --valgrind    Use valgrind on capture\n";
    print "\n";
    print "Commands:\n";
    print "  --help                This help\n";
    print "  --make                Create a .test file for each .pcap file on command line\n";
    print "  --reip file ip newip  Create file.tmp, replace ip with newip\n";
    print "  --viewer              viewer tests\n";
    print "                        This will init local ES, import data, start a viewer, run tests\n";
    print " [default]              Run each .pcap file thru ../capture/moloch-capture and compare to .test file\n";
} elsif ($main::cmd =~ "^--viewer") {
    doGeo();
    setpgrp $$, 0;
    doViewer($main::cmd);
} else {
    doGeo();
    doTests();
}
