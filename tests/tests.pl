#!/usr/bin/perl

use strict;
use JSON;
use Data::Dumper;
use Test::More;

my $debug = 0;
if ($ARGV[0] eq "--debug") {
    $debug = 1;
    shift @ARGV;
}

if ($ARGV[0] eq "--fix") {
    shift @ARGV;
    doFix();
} elsif ($ARGV[0] eq "--make") {
    shift @ARGV;
    doMake();
} elsif ($ARGV[0] eq "--help") {
    print "$ARGV[0] [-debug] [COMMAND] <pcap> files\n";
    print "Commands:\n";
    print "  --help        This help\n";
    print "  --make        Create a .test file for each .pcap file on command line\n";
    print " [default]      Run each .pcap file thru ../capture/moloch-capture and compare to .test file\n";
} else {
    doTests();
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

        if ($debug) {
            print "../capture/moloch-capture --tests -c config.test.ini -n test -r $filename.pcap 2>&1 1>/dev/null | ./tests.pl --fix\n";
        }

        my $testData = `../capture/moloch-capture --tests -c config.test.ini -n test -r $filename.pcap 2>&1 1>/dev/null | ./tests.pl --fix`;
        my $testJson = from_json($testData, {relaxed => 1});

        is_deeply($testJson, $savedJson, "$filename");
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

sub fix {
my ($json) = @_;
    foreach my $packet (@{$json->{packets}}) {
        my $body = $packet->{body};

        delete $packet->{header}->{index}->{_id};
        foreach my $field ("a1", "a2", "dnsip", "socksip", "eip") {
            $body->{$field} = fixIp($body->{$field}) if (exists $body->{$field});
        }

        foreach my $field ("ta", "hh1", "hh2") {
            $body->{$field} = fixTags($json, $body->{$field}) if (exists $body->{$field});
        }
    }

  delete $json->{tags};
}

sub fixTags {
my ($json, $tags) = @_;
    my @list = ();
    foreach my $tag (@{$tags}) {
      push(@list, $json->{tags}->{$tag});
    }
    @list = sort(@list);
    return \@list;
}

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
        if ($debug) {
          print("../capture/moloch-capture --tests -c config.test.ini -n test -r $filename.pcap 2>&1 1>/dev/null | ./tests.pl --fix > $filename.test\n");
        }
        system("../capture/moloch-capture --tests -c config.test.ini -n test -r $filename.pcap 2>&1 1>/dev/null | ./tests.pl --fix > $filename.test");
    }
}
################################################################################
