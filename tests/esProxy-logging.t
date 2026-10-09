use strict;
use FindBin;

# Run the standalone Node tests through the existing TAP harness. No ES needed.
exec('node', '--test', '--test-reporter=tap', "$FindBin::Bin/esProxy-logging.js")
    or die "can't run esProxy logging tests: $!";
