#!/usr/bin/perl -I.
#
# SPDX-License-Identifier: Apache-2.0

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
use Socket6 qw(AF_INET6 inet_pton);

$main::userAgent = LWP::UserAgent->new(timeout => 20);

my $ELASTICSEARCH = $ENV{ELASTICSEARCH} = "http://127.0.0.1:9200";

$ENV{'PERL5LIB'} = getcwd();
$ENV{'TZ'} = 'US/Eastern';
my $INSECURE = "";

################################################################################
sub doGeo {
    if (! -f "ipv4-address-space.csv") {
        system("wget https://s3.amazonaws.com/files.molo.ch/testing/ipv4-address-space.csv");
    }

    if (! -f "oui.txt") {
        system("wget https://s3.amazonaws.com/files.molo.ch/testing/oui.txt");
    }

    if (! -f "GeoLite2-Country.mmdb") {
        system("wget https://s3.amazonaws.com/files.molo.ch/testing/GeoLite2-Country.mmdb");
    }

    if (! -f "GeoLite2-ASN.mmdb") {
        system("wget https://s3.amazonaws.com/files.molo.ch/testing/GeoLite2-ASN.mmdb");
    }

    if (! -f "plugins/test.so" || (stat('../capture/moloch.h'))[9] > (stat('plugins/test.so'))[9]) {
        system("cd plugins ; make");
    }
}
################################################################################
sub doFuzz2Pcap {
    my @files = @ARGV;
    foreach my $file (@files) {
        print "$file\n";;
        open my $in, '<', "$file" or die "error opening $file: $!";
        open my $out, '>', "$file.pcap" or die "error opening $file.pcap: $!";
        binmode($in);
        binmode($out);

        my $buf;
        read($in, $buf, 1000000);

        my $len = length($buf);

        # Pcap header
        syswrite($out, pack('H*', "d4c3b2a1020004000000000000000000ffff000001000000"));

        my $pos = 0;
        my $num = 0;
        while ($pos < $len) {
            my $ilen = unpack("x${pos}n", $buf);
            $pos += 2;
            syswrite($out, pack('VH*VV', $num, "00000000", $ilen, $ilen));
            syswrite($out, $buf, $ilen, $pos);
            $pos += $ilen;
            $num++;
        }

        close($in);
        close($out);
    }
}
################################################################################
sub sortObj {
    my ($parentkey,$obj) = @_;
    for my $key (keys %{$obj}) {
        my $r = ref $obj->{$key};
        if ($r eq "HASH") {
            sortObj($key, $obj->{$key});
        } elsif ($r eq "ARRAY") {
            next if (scalar (@{$obj->{$key}}) < 2);
            next if ($key =~ /(packetPos|packetLen|cert)/);
            if ("$parentkey.$key" =~ /vlan.id|http.statuscode|icmp.type|icmp.code/) {
                my @tmp = sort { $a <=> $b } (@{$obj->{$key}});
                $obj->{$key} = \@tmp;
            } else {
                my @tmp = sort (@{$obj->{$key}});
                $obj->{$key} = \@tmp;
            }
        }
    }
}
################################################################################
sub sortJson {
    my ($json) = @_;

    foreach my $session (@{$json->{sessions3}}) {
        sortObj("", $session->{body});
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


        my $cmd = "../capture/capture --tests -c config.test.ini -n test -r $filename.pcap 2>&1 1>/dev/null | ./tests.pl --fix";

        if ($main::valgrind) {
            $cmd = "G_SLICE=always-malloc valgrind --leak-check=full --log-file=$filename.val " . $cmd;
        }

        if ($main::debug) {
            print "$cmd\n";
        }

        my $testData = `$cmd`;
        my $testJson;

        eval {
            $testJson = sortJson(from_json($testData, {relaxed => 1}));
            1;
        } or do {
            my $e = $@;
            print "$e\n";
            print $testData, "\n";
            exit 1;
        };

        eq_or_diff($testJson, $savedJson, "$filename", { context => 3 });
    }
}
################################################################################
sub doFix {
    my $data = do { local $/; <> };
    my $json;
    eval {
        $json = from_json($data, {relaxed => 1});
        1;
    } or do {
        my $e = $@;
        print "$e\n";
        print $data, "\n";
        exit 1;
    };

    fix($json);
    $json = to_json($json, {pretty => 1, canonical => 1});
    print $json, "\n";
}

################################################################################
sub fix {
my ($json) = @_;
    my $json = sortJson($json);
    foreach my $session (@{$json->{sessions3}}) {
        my $body = $session->{body};

        delete $session->{header}->{index}->{_id};
        if (exists $body->{rootId}) {
            $body->{rootId} = "SET";
        }
        if (exists $body->{timestamp}) {
            $body->{timestamp} = "SET";
        }
        if (exists $body->{"\@timestamp"}) {
            $body->{"\@timestamp"} = "SET";
        }

        if ($body->{srcIp} =~ /:/) {
            $body->{srcIp} = join ":", (unpack("H*", inet_pton(AF_INET6, $body->{srcIp})) =~ m/(....)/g );
        }
        if ($body->{dstIp} =~ /:/) {
            $body->{dstIp} = join ":", (unpack("H*", inet_pton(AF_INET6, $body->{dstIp})) =~ m/(....)/g );
        }

        if (exists $body->{dns} && exists $body->{dns}->{ip}) {
            for (my $i = 0; $i < @{$body->{dns}->{ip}}; $i++) {
                if ($body->{dns}->{ip}[$i] =~ /:/) {
                    $body->{dns}->{ip}[$i] = join ":", (unpack("H*", inet_pton(AF_INET6, $body->{dns}->{ip}[$i])) =~ m/(....)/g );
                }
            }
        }
        if (exists $body->{dns} && exists $body->{dns}->{nameserverIp}) {
            for (my $i = 0; $i < @{$body->{dns}->{nameserverIp}}; $i++) {
                if ($body->{dns}->{nameserverIp}[$i] =~ /:/) {
                    $body->{dns}->{nameserverIp}[$i] = join ":", (unpack("H*", inet_pton(AF_INET6, $body->{dns}->{nameserverIp}[$i])) =~ m/(....)/g );
                }
            }
        }
        if (exists $body->{dns} && exists $body->{dns}->{mailserverIp}) {
            for (my $i = 0; $i < @{$body->{dns}->{mailserverIp}}; $i++) {
                if ($body->{dns}->{mailserverIp}[$i] =~ /:/) {
                    $body->{dns}->{mailserverIp}[$i] = join ":", (unpack("H*", inet_pton(AF_INET6, $body->{dns}->{mailserverIp}[$i])) =~ m/(....)/g );
                }
            }
        }
    }

    @{$json->{sessions3}} = sort {
        return $a->{body}->{firstPacket} <=> $b->{body}->{firstPacket} if ($a->{body}->{firstPacket} != $b->{body}->{firstPacket});
        return $a->{body}->{source}->{ip} <=> $b->{body}->{source}->{ip};
    } @{$json->{sessions3}};
}

################################################################################
sub doMake {
    foreach my $filename (@ARGV) {
        $filename = substr($filename, 0, -5) if ($filename =~ /\.pcap$/);
        if ($main::debug) {
          print("../capture/capture --tests -c config.test.ini -n test -r $filename.pcap 2>&1 1>/dev/null | ./tests.pl --fix > $filename.test\n");
        }
        system("../capture/capture --tests -c config.test.ini -n test -r $filename.pcap 2>&1 1>/dev/null | ./tests.pl --fix > $filename.test");
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

    my $node = "node";
    $node = "c8 node" if ($main::c8);

    if ($cmd ne "--viewernostart" && $cmd ne "--viewerstart" && $cmd ne "--viewerhang") {
        print ("Initializing ES\n");
        if ($main::debug) {
            system("../db/db.pl $INSECURE --prefix tests $ELASTICSEARCH initnoprompt");
            system("../db/db.pl $INSECURE --prefix tests2 $ELASTICSEARCH initnoprompt");
        } else {
            system("../db/db.pl $INSECURE --prefix tests $ELASTICSEARCH initnoprompt 2>&1 1>/dev/null");
            system("../db/db.pl $INSECURE --prefix tests2 $ELASTICSEARCH initnoprompt 2>&1 1>/dev/null");
        }

        print ("Loading tagger\n");
        print("../capture/plugins/taggerUpload.pl $INSECURE $ELASTICSEARCH ip ip.tagger1.json iptaggertest1\n");
        system("../capture/plugins/taggerUpload.pl $INSECURE $ELASTICSEARCH ip ip.tagger1.json iptaggertest1");
        system("../capture/plugins/taggerUpload.pl $INSECURE $ELASTICSEARCH host host.tagger1.json hosttaggertest1");
        system("../capture/plugins/taggerUpload.pl $INSECURE $ELASTICSEARCH md5 md5.tagger1.json md5taggertest1");
        system("../capture/plugins/taggerUpload.pl $INSECURE $ELASTICSEARCH ip ip.tagger2.json iptaggertest2");
        system("../capture/plugins/taggerUpload.pl $INSECURE $ELASTICSEARCH host host.tagger2.json hosttaggertest2");
        system("../capture/plugins/taggerUpload.pl $INSECURE $ELASTICSEARCH md5 md5.tagger2.json md5taggertest2");
        system("../capture/plugins/taggerUpload.pl $INSECURE $ELASTICSEARCH email email.tagger2.json emailtaggertest2");
        system("../capture/plugins/taggerUpload.pl $INSECURE $ELASTICSEARCH uri uri.tagger2.json uritaggertest2");
    }

    if ($cmd ne "--viewernostart") {
        my $wes = "-o 'wiseService.usersElasticsearch=$ELASTICSEARCH'";
        print ("Starting WISE\n");
        if ($main::debug) {
            system("cd ../wiseService ; $node wiseService.js $wes $INSECURE --webcode thecode --webconfig --regressionTests -c ../tests/config.test.json > /tmp/moloch.wise &");
        } else {
            system("cd ../wiseService ; $node wiseService.js $wes $INSECURE --webcode thecode --webconfig --regressionTests -c ../tests/config.test.json > /dev/null &");
        }

        waitFor($MolochTest::host, 8081, 1);
    }

    my $es = "-o 'elasticsearch=$ELASTICSEARCH'";
    my $ces = "-o 'cont3xt.elasticsearch=$ELASTICSEARCH'";
    my $ues = "-o 'usersElasticsearch=$ELASTICSEARCH'";
    my $cues = "-o 'cont3xt.usersElasticsearch=$ELASTICSEARCH'";
    my $pues = "-o 'parliament.usersElasticsearch=$ELASTICSEARCH'";
    my $mes = "-o 'multiESNodes=$ELASTICSEARCH,prefix:tests,name:test;$ELASTICSEARCH,prefix:tests2_,name:test2'";
    my $s3 = "-o 's3AccessKeyId=$ENV{s3AccessKeyId}' -o 's3SecretAccessKey=$ENV{s3SecretAccessKey}'";

    if ($cmd ne "--viewernostart" && $cmd ne "--viewerstart" && $cmd ne "--viewerhang") {
        $main::userAgent->get("$ELASTICSEARCH/_flush");
        $main::userAgent->get("$ELASTICSEARCH/_refresh");

        print ("Loading PCAP\n");

        my $mcmd = "../capture/capture $es $INSECURE $main::copy -c config.test.ini -n test -R pcap --flush";
        if (!$main::debug) {
            $mcmd .= " 2>&1 1>/dev/null";
        } else {
            $mcmd .= " --debug 1>/tmp/moloch.capture 2>&1";
        }


        if ($main::valgrind) {
            $mcmd = "G_SLICE=always-malloc valgrind --leak-check=full --log-file=moloch.val " . $mcmd;
        }

        print "$mcmd\n" if ($main::debug);
        system($mcmd);

        die "Loaded" if ($cmd eq "--viewerload");

        esCopy("tests_fields", "tests2_fields", "field");
    }

    if ($cmd ne "--viewernostart") {
        print ("Starting viewer\n");
        if ($main::debug) {
            system("cd ../viewer ; $node --trace-warnings multies.js --regressionTests $mes -c ../tests/config.test.ini -n all --debug $INSECURE > /tmp/multies.all &");
            waitFor($MolochTest::host, 8200, 1);
            system("cd ../viewer ; $node --trace-warnings viewer.js --regressionTests $es $ues -c ../tests/config.test.ini -n test --debug $INSECURE > /tmp/moloch.test &");
            system("cd ../viewer ; $node --trace-warnings viewer.js --regressionTests $es $ues -c ../tests/config.test.ini -n test2 --debug $INSECURE $s3 > /tmp/moloch.test2 &");
            system("cd ../viewer ; $node --trace-warnings viewer.js --regressionTests $es $ues -c ../tests/config.test.ini -n test3 --debug -o s2sRegressionTests=true $INSECURE > /tmp/moloch.test3 &");
            system("cd ../viewer ; $node --trace-warnings viewer.js --regressionTests $ues -c ../tests/config.test.ini -n all --debug $INSECURE > /tmp/moloch.all &");
            system("cd ../parliament ; $node --trace-warnings parliament.js --regressionTests $pues -c ../tests/parliament.tests.ini -n parliamenttest --debug $INSECURE > /tmp/moloch.parliament 2>&1 &");
            system("cd ../cont3xt ; $node --trace-warnings cont3xt.js $ces $cues --regressionTests -c ../tests/cont3xt.tests.ini --debug $INSECURE > /tmp/moloch.cont3xt 2>&1 &");
            system("cd ../viewer ; $node --trace-warnings esProxy.js --regressionTests $es -c ../tests/config.test.ini -n esproxy --debug $INSECURE > /tmp/moloch.esproxy &");
        } else {
            system("cd ../viewer ; $node multies.js --regressionTests $mes -c ../tests/config.test.ini -n all $INSECURE > /dev/null &");
            waitFor($MolochTest::host, 8200, 1);
            system("cd ../viewer ; $node viewer.js --regressionTests $es $ues -c ../tests/config.test.ini -n test $INSECURE > /dev/null &");
            system("cd ../viewer ; $node viewer.js --regressionTests $es $ues -c ../tests/config.test.ini -n test2 $INSECURE $s3 > /dev/null &");
            system("cd ../viewer ; $node viewer.js --regressionTests $es $ues -c ../tests/config.test.ini -n test3 -o s2sRegressionTests=true $INSECURE > /dev/null &");
            system("cd ../viewer ; $node viewer.js --regressionTests $ues -c ../tests/config.test.ini -n all $INSECURE > /dev/null &");
            system("cd ../parliament ; $node parliament.js --regressionTests $pues -c ../tests/parliament.tests.ini -n parliamenttest $INSECURE > /dev/null 2>&1 &");
            system("cd ../cont3xt ; $node cont3xt.js $ces $cues --regressionTests -c ../tests/cont3xt.tests.ini $INSECURE > /dev/null 2>&1 &");
            system("cd ../viewer ; $node --trace-warnings esProxy.js --regressionTests $es -c ../tests/config.test.ini -n esproxy --debug $INSECURE >> /dev/null 2>&1 &");
        }
        sleep (10000) if ($cmd eq "--viewerhang");
    }

    waitFor($MolochTest::host, 8123);
    waitFor($MolochTest::host, 8124);
    waitFor($MolochTest::host, 8125);
    waitFor($MolochTest::host, 8008);
    waitFor($MolochTest::host, 3218);
    waitFor($MolochTest::host, 7200);
    sleep 1;

    $main::userAgent->get("$ELASTICSEARCH/_flush");
    $main::userAgent->get("$ELASTICSEARCH/_refresh");
    sleep 1;

    my $harness = TAP::Harness->new();

    my @tests = @ARGV;
    @tests = glob ("*.t") if ($#tests == -1);
    my $parser = $harness->runtests(@tests);

# Cleanup
    if ($cmd ne "--viewernostart") {
        $main::userAgent->post("http://localhost:8123/regressionTests/shutdown");
        $main::userAgent->post("http://localhost:8124/regressionTests/shutdown");
        $main::userAgent->post("http://localhost:8125/regressionTests/shutdown");
        $main::userAgent->post("http://localhost:8126/regressionTests/shutdown");
        $main::userAgent->post("http://localhost:8200/regressionTests/shutdown");
        $main::userAgent->post("http://localhost:8081/regressionTests/shutdown");
        $main::userAgent->post("http://localhost:8008/regressionTests/shutdown");
        $main::userAgent->post("http://localhost:3218/regressionTests/shutdown");
        $main::userAgent->post("http://localhost:7200/regressionTests/shutdown");
    }

# Coverage
    if ($main::c8) {
        system("cd ../viewer ; c8 report");
        system("cd ../wiseService ; c8 report");
        system("cd ../parliament ; c8 report");
        system("cd ../cont3xt ; c8 report");
    }

    exit(1) if ( $parser->has_errors );
}
################################################################################
$main::debug = 0;
$main::c8 = 0;
$main::valgrind = 0;
$main::copy = "";
$main::cmd = "--capture";

while (scalar (@ARGV) > 0) {
    if ($ARGV[0] eq "--debug") {
        $main::debug = 1;
        shift @ARGV;
    } elsif ($ARGV[0] eq "--elasticsearch") {
        shift @ARGV;
        $MolochTest::elasticsearch = $ELASTICSEARCH = $ENV{ELASTICSEARCH} = $ARGV[0];
        shift @ARGV;
    } elsif ($ARGV[0] eq "--c8") {
        $main::c8 = 1;
        $main::debug = 1;
        system("rm -rf ../viewer/coverage");
        system("rm -rf ../wiseService/coverage");
        system("rm -rf ../parliament/coverage");
        system("rm -rf ../cont3xt/coverage");
        shift @ARGV;
    } elsif ($ARGV[0] eq "--insecure") {
        $MolochTest::userAgent->ssl_opts(
            SSL_verify_mode => 0,
            verify_hostname=> 0
        );
        $ENV{INSECURE} = $INSECURE = "--insecure";
        shift @ARGV;
    } elsif ($ARGV[0] eq "--valgrind") {
        $main::valgrind = 1;
        shift @ARGV;
    } elsif ($ARGV[0] eq "--copy") {
        $main::copy = "--copy";
        shift @ARGV;
    } elsif ($ARGV[0] =~ /^--(viewer|fix|make|capture|viewernostart|viewerstart|viewerhang|viewerload|help|reip|fuzz|fuzz2pcap)$/) {
        $main::cmd = $ARGV[0];
        shift @ARGV;
    } elsif ($ARGV[0] =~ /^--/) {
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
} elsif ($main::cmd eq "--fuzz") {
    doGeo();
    my $cmd = "ASAN_OPTIONS=fast_unwind_on_malloc=0 G_SLICE=always-malloc ../capture/fuzzloch-capture -max_len=8196 -timeout=5 @ARGV";
    print "$cmd\n";
    system($cmd);
} elsif ($main::cmd eq "--fuzz2pcap") {
    doFuzz2Pcap();
} elsif ($main::cmd eq "--help") {
    print "$ARGV[0] [OPTIONS] [COMMAND] <pcap> files\n";
    print "Options:\n";
    print "  --elasticsearch <url> Set elasticsearch URL\n";
    print "  --debug               Turn on debuggin\n";
    print "  --valgrind            Use valgrind on capture\n";
    print "\n";
    print "Commands:\n";
    print "  --help                This help\n";
    print "  --make                Create a .test file for each .pcap file on command line\n";
    print "  --reip file ip newip  Create file.tmp, replace ip with newip\n";
    print "  --viewer              viewer tests\n";
    print "                        This will init local ES, import data, start a viewer, run tests\n";
    print "  --viewerstart         Viewer tests without reloading pcap\n";
    print "  --fuzz [fuzzoptions]  Run fuzzloch\n";
    print "  --fuzz2pcap           Convert a fuzzloch crash file into a pcap file\n";
    print " [default] [pcap files] Run each .pcap (default pcap/*.pcap) file thru ../capture/capture and compare to .test file\n";
} elsif ($main::cmd =~ "^--viewer") {
    doGeo();
    setpgrp $$, 0;
    doViewer($main::cmd);
} else {
    doGeo();
    doTests();
}
