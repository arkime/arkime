use strict;
use FindBin;

# Run the standalone Node tests through the existing TAP harness. No ES needed.
open(my $out, '-|', 'node', '--test', '--test-reporter=tap', "$FindBin::Bin/esProxy-logging.js")
    or die "can't run esProxy logging tests: $!";

# Older TAP::Parser versions cannot read Node's YAML block scalars. Preserve
# diagnostics as comments without changing the test results or exit status.
my $diagnostic = 0;
while (my $line = <$out>) {
    $diagnostic = 1 if $line =~ /^  ---\s*$/;
    print $diagnostic ? "# $line" : $line;
    $diagnostic = 0 if $line =~ /^  \.\.\.\s*$/;
}
close($out);
exit($? ? 1 : 0);
