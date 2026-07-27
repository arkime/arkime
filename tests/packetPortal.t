# Test the packet portal transport: test2 (Sensor) opens an outbound link to the
# central (test) viewer, which can then reach test2 back over that link.
use Test::More tests => 2;
use ArkimeTest;
use Data::Dumper;
use strict;

# test2 dials the central viewer at startup; wait for its link to register.
my $connected = 0;
for (my $i = 0; $i < 50; $i++) {
    my $nodes = viewerGet("/regressionTests/packetPortal");
    if (ref($nodes) eq 'ARRAY' && grep { $_ eq 'test2' } @$nodes) {
        $connected = 1;
        last;
    }
    select(undef, undef, undef, 0.2);
}
ok($connected, "test2 established a packet portal to the central (test) viewer");

# A node-to-node request from the central to test2 now travels over the link
# (test2 is behind a link, so there is no direct fallback) -- confirm it works.
my $result = viewerGet("/regressionTests/packetPortalRequest/test2");
ok($result->{success}, "central reached test2 over the packet portal") or diag(Dumper($result));
