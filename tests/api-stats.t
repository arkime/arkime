use Test::More tests => 48;
use Cwd;
use URI::Escape;
use MolochTest;
use Data::Dumper;
use JSON;
use Test::Differences;
use strict;

my $pwd = "*/pcap";

my $token = getTokenCookie();
my $test1Token = getTokenCookie("test1");

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

    $indices = viewerGet("/esindices/list?desc=true");
    cmp_ok ($indices->[0]->{index} cmp $indices->[1]->{index}, ">", 0, "indices index sorted reverse");

    $indices = viewerGet("/esindices/list?sortField=store.size");
    cmp_ok ($indices->[0]->{"store.size"}, "<=", $indices->[1]->{"store.size"}, "indices store.size sorted");

    $indices = viewerGet("/esindices/list?desc=true&sortField=store.size");
    cmp_ok ($indices->[0]->{"store.size"}, ">=", $indices->[1]->{"store.size"}, "indices store.size sorted reverse");

# estasks
    my $tasks = viewerGet("/estask/list");
    cmp_ok (@{$tasks}, ">=", 1, "tasks array size");

# esshards
    my $shards = viewerGet("/esshard/list");
    cmp_ok (@{$shards->{indices}}, ">=", 30, "esshards: indices array size");
    cmp_ok ($shards->{indices}->[0]->{name}, "lt", $shards->{indices}->[1]->{name}, "esshard: index[0] before index[1]");
    eq_or_diff($shards->{nodeExcludes}, [], "esshard: nodeExcludes empty");
    eq_or_diff($shards->{ipExcludes}, [], "esshard: ipExcludes empty");

    my $result = viewerPost("/esshard/exclude/ip/1.2.3.4", "");
    eq_or_diff($result, from_json('{"success": false, "text": "Missing token"}'), "esshard: exclude no token");

    $result = viewerPostToken("/esshard/exclude/ip/1.2.3.4", "", $token);
    eq_or_diff($result, from_json('{"success": true, "text": "Excluded"}'), "esshard: exclude ip");

    $result = viewerPostToken("/esshard/exclude/name/thenode", "", $token);
    eq_or_diff($result, from_json('{"success": true, "text": "Excluded"}'), "esshard: exclude node");

    $result = viewerPostToken("/esshard/exclude/foobar/1.2.3.4", "", $token);
    eq_or_diff($result, from_json('{"success": false, "text": "Unknown exclude type"}'), "esshard: exclude foobar");

    $result = viewerPostToken("/esshard/exclude/foobar/1.2.3.4?molochRegressionUser=test1", "", $test1Token);
    eq_or_diff($result, from_json('{"success": false, "text": "Need admin privileges"}'), "esshard: exclude not admin");

    $shards = viewerGet("/esshard/list");
    eq_or_diff($shards->{nodeExcludes}, ["thenode"], "esshard: nodeExcludes empty");
    eq_or_diff($shards->{ipExcludes}, ["1.2.3.4"], "esshard: ipExcludes empty");

    $result = viewerPost("/esshard/include/ip/1.2.3.4", "");
    eq_or_diff($result, from_json('{"success": false, "text": "Missing token"}'), "esshard: include no token");

    $result = viewerPostToken("/esshard/include/ip/1.2.3.4", "", $token);
    eq_or_diff($result, from_json('{"success": true, "text": "Included"}'), "esshard: include ip");

    $result = viewerPostToken("/esshard/include/name/thenode", "", $token);
    eq_or_diff($result, from_json('{"success": true, "text": "Included"}'), "esshard: include node");

    $result = viewerPostToken("/esshard/include/foobar/1.2.3.4", "", $token);
    eq_or_diff($result, from_json('{"success": false, "text": "Unknown include type"}'), "esshard: include foodbar");

    $result = viewerPostToken("/esshard/include/foobar/1.2.3.4?molochRegressionUser=test1", "", $test1Token);
    eq_or_diff($result, from_json('{"success": false, "text": "Need admin privileges"}'), "esshard: include not admin");

    $shards = viewerGet("/esshard/list");
    eq_or_diff($shards->{nodeExcludes}, [], "esshard: nodeExcludes empty");
    eq_or_diff($shards->{ipExcludes}, [], "esshard: ipExcludes empty");
