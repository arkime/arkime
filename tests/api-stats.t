use Test::More tests => 30;
use Cwd;
use URI::Escape;
use MolochTest;
use Data::Dumper;
use strict;

my $pwd = getcwd() . "/pcap";

# stats.json
    my $stats = viewerGet("/stats.json");
    is (@{$stats->{data}}, 1, "stats.json data set ");
    is ($stats->{recordsTotal}, 1, "stats.json recordsTotal");
    is ($stats->{data}->[0]->{id}, "test", "stats.json name");
    foreach my $i ("diskQueue", "deltaDroppedPerSec") {
        is ($stats->{data}->[0]->{$i}, 0, "stats.json $i 0");
    }

    foreach my $i ("monitoring", "diskQueue", "deltaDropped", "deltaDroppedPerSec") {
        is ($stats->{data}->[0]->{$i}, 0, "stats.json $i == 0");
    }

    foreach my $i ("deltaMS", "totalPackets", "memory", "cpu", "currentTime", "totalK", "totalSessions", "freeSpaceM") {
        cmp_ok ($stats->{data}->[0]->{$i}, '>', 0, "stats.json $i > 0");
    }

    foreach my $i ("deltaPackets", "deltaBytes", "deltaBytesPerSec", "deltaPacketsPerSec", "deltaSessions", "deltaSessionsPerSec") {
        is (exists $stats->{data}->[0]->{$i}, 1, "stats.json $i");
    }

# dstats.json
    my $dstats = viewerGet("/dstats.json?nodeName=test&start=1399680425&stop=1399680460&step=5&interval=5&name=deltaPackets");
    is (@{$dstats}, 7, "dstats.json array size");

# esindices
    my $indices = viewerGet("/esindices/list");
    cmp_ok (@{$indices}, ">=", 30, "indices array size");
    cmp_ok ($indices->[0]->{index} cmp $indices->[1]->{index}, "<", 0, "indices index sorted");

    my $indices = viewerGet("/esindices/list?desc=true");
    cmp_ok ($indices->[0]->{index} cmp $indices->[1]->{index}, ">", 0, "indices index sorted reverse");

    my $indices = viewerGet("/esindices/list?sortField=store.size");
    cmp_ok ($indices->[0]->{"store.size"}, "<=", $indices->[1]->{"store.size"}, "indices store.size sorted");

    my $indices = viewerGet("/esindices/list?desc=true&sortField=store.size");
    cmp_ok ($indices->[0]->{"store.size"}, ">=", $indices->[1]->{"store.size"}, "indices store.size sorted reverse");

# estasks
    my $tasks = viewerGet("/estask/list");
    cmp_ok (@{$tasks}, ">=", 1, "tasks array size");
