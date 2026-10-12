use strict;
use FindBin;

# Run standalone Node tests through the TAP harness.
open(my $out, '-|', 'node', '--test', '--test-reporter=tap', "$FindBin::Bin/esProxy-logging.js")
    or die "can't run esProxy logging tests: $!";

# Emit Node diagnostics as TAP comments.
my $diagnostic = 0;
while (my $line = <$out>) {
    $diagnostic = 1 if $line =~ /^  ---\s*$/;
    print $diagnostic ? "# $line" : $line;
    $diagnostic = 0 if $line =~ /^  \.\.\.\s*$/;
}
close($out);
exit($? ? 1 : 0);
