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
my $topJson = from_json(`npx license-checker --production --json`);
#my $viewerJson = from_json(`cd $ARGV[0]/viewer; npx license-checker --production --json`);
my $cont3xtJson = from_json(`cd $ARGV[0]/cont3xt; npx license-checker --production --json`);

shift @ARGV;

for my $file (@ARGV) {
    system("cat $file");
}

for my $key (keys %$topJson) {
    $DONE{$key} = 1;
    handle($key, $topJson->{$key});
}

#for my $key (keys %$viewerJson) {
#    $DONE{$key} = 1;
#    handle($key, $viewerJson->{$key});
#}

for my $key (keys %$cont3xtJson) {
    next if ($DONE{$key} == 1);
    $DONE{$key} = 1;
    handle($key, $cont3xtJson->{$key});
}

# The Vue apps are bundled by vite from devDependencies that license-checker
# never sees. common/vite-plugin-third-party-licenses.mjs records what each
# production bundle actually contains when 'npm run bundle:min' runs.
for my $app (qw(viewer cont3xt parliament wiseService)) {
    my $file = "$app/vueapp/third-party-licenses.json";
    if (! -f $file) {
        print STDERR "notice.txt.pl: missing $file, run 'npm run bundle:min' in $app first\n";
        exit 1;
    }
    open(my $fh, '<', $file) or die "$file: $!";
    my $deps = from_json(do { local $/; <$fh> });
    close($fh);
    for my $dep (@$deps) {
        my $key = "$dep->{name}\@$dep->{version}";
        next if ($DONE{$key});
        $DONE{$key} = 1;
        print "================================================================================\n";
        print "$key - $dep->{license} - $dep->{repository}\n\n";
        print $dep->{licenseText} // "", "\n";
    }
}
