#!/usr/bin/perl
use strict;
use Data::Dumper;

my %settings;
my $capture = `egrep -h 'moloch_config_(str|int|boolean|double).*"' ../capture/*.c ../capture/*/*.c ../capture/*/*/*.c`;
foreach my $line (split("\n", $capture)) {
    my ($match) = $line =~ /"([^"]*)"/;
    $settings{$match} = $line;
}

my $viewer = `egrep -h 'Config.get(\\(|Full)' ../viewer/*.js`;
foreach my $line (split("\n", $viewer)) {
    my ($match) = $line =~ /get[^"']*["']([^"']*)["']/;
    $settings{$match} = $line;
}

foreach my $setting (keys (%settings)) {
    if ($setting =~ /[^0-9a-zA-Z_-]/) {
        print "***Not a real setting $setting\n";
        next;
    }
    my $lcsetting = lc($setting);

    my $output = `egrep  '(id="$lcsetting"|^$setting\\|)' ../../molochweb/settings.html ../../molochweb/_wiki/wise.md`;
    if ($output eq "") {
        print "MISSING id tag id=\"$lcsetting\" => $settings{$setting}\n";
    }

    $output = `egrep  '( $setting\$|>$setting<|^$setting\\|)' ../../molochweb/settings.html ../../molochweb/_wiki/wise.md`;
    if ($output eq "") {
        print "MISSING $setting\n";
    }
}
