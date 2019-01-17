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
my $topJson = from_json(`license-checker --production --json`);
my $parliamentJson = from_json(`cd $ARGV[0]/parliament; license-checker --production --json`);
my $viewerJson = from_json(`cd $ARGV[0]/viewer; license-checker --production --json`);
my $wiseServiceJson = from_json(`cd $ARGV[0]/wiseService; license-checker --production --json`);

shift @ARGV;

for my $file (@ARGV) {
    system("cat $file");
}

for my $key (keys %$topJson) {
    $DONE{$key} = 1;
    handle($key, $topJson->{$key});
}

for my $key (keys %$parliamentJson) {
    $DONE{$key} = 1;
    handle($key, $parliamentJson->{$key});
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
