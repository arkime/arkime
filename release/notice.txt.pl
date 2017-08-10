#!/usr/bin/perl
use JSON;
use Data::Dumper;
use POSIX;
use strict;


sub handle {
    my ($key, $item) = @_;

    print "================================================================================\n";
    print "$key - $item->{licenses} - $item->{repository}\n\n";
    system("cat $item->{licenseFile}") if (exists $item->{licenseFile});
    print "\n";
}

my %DONE;
my $viewerJson = from_json(`cd $ARGV[0]/viewer; license-checker --production --json`);
my $wiseServiceJson = from_json(`cd $ARGV[0]/capture/plugins/wiseService; license-checker --production --json`);

shift @ARGV;

for my $file (@ARGV) {
    system("cat $file");
}

for my $key (keys %$viewerJson) {
    $DONE{$key} = 1;
    handle($key, $viewerJson->{$key});
}

for my $key (keys %$wiseServiceJson) {
    next if ($DONE{$key} == 1);
    $DONE{$key} = 1;
    handle($key, $wiseServiceJson->{$key});
}
