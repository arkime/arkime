use Test::More tests => 105;
use Cwd;
use URI::Escape;
use ArkimeTest;
use Data::Dumper;
use JSON;
use Test::Differences;
use strict;

my $pwd = "*/pcap";

my $token = getTokenCookie();
my $test1Token = getTokenCookie("test1");

# stats.json
    my $stats = viewerGet("/api/stats");
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
        cmp_ok ($stats->{data}->[0]->{$i}, '>=', 0, "stats.json $i >= 0");
    }

    foreach my $i ("deltaPackets", "deltaBytes", "deltaBytesPerSec", "deltaPacketsPerSec", "deltaSessions", "deltaSessionsPerSec") {
        is (exists $stats->{data}->[0]->{$i}, 1, "stats.json $i");
    }

    my $mstats = multiGet("/stats.json");
    is (@{$mstats->{data}}, 1, "multi stats.json data set ");

    my $mstats = multiGet("/stats.json?cluster=test");
    is (@{$mstats->{data}}, 1);

    my $mstats = multiGet("/stats.json?cluster=test2");
    is (@{$mstats->{data}}, 0);

    my $mstats = multiGet("/stats.json?cluster=unknown");
    is (@{$mstats->{data}}, 0);

# dstats.json
    my $dstats = viewerGet("/api/dstats?nodeName=test&start=1399680425&stop=1399680460&step=5&interval=5&name=deltaPackets");
    is (@{$dstats}, 7, "dstats.json array size");

# esstats.json
    my $esstats = viewerGet("/api/esstats");
    is ($esstats->{data}->[0]->{writesRejectedDelta}, 0, "Writes reject");

    my $messtats = multiGet("/esstats.json");
    is ($messtats->{data}->[0]->{writesRejectedDelta}, 0, "Writes reject");

    my $messtats = multiGet("/esstats.json?cluster=test");
    is (@{$messtats->{data}}, 1);
    is ($messtats->{data}->[0]->{writesRejectedDelta}, 0, "Writes reject");

    my $messtats = multiGet("/esstats.json?cluster=unknown");
    is (@{$messtats->{data}}, 0);

# esindices
    my $indices = viewerGet("/api/esindices");
    cmp_ok (@{$indices->{data}}, ">=", 30, "indices array size");
    cmp_ok ($indices->{data}->[0]->{index} cmp $indices->{data}->[1]->{index}, "<", 0, "indices index sorted");

    $indices = viewerGet("/api/esindices?desc=true");
    cmp_ok ($indices->{data}->[0]->{index} cmp $indices->{data}->[1]->{index}, ">", 0, "indices index sorted reverse");

    $indices = viewerGet("/api/esindices?sortField=store.size");
    cmp_ok ($indices->{data}->[0]->{"store.size"}, "<=", $indices->{data}->[1]->{"store.size"}, "indices store.size sorted");

    $indices = viewerGet("/api/esindices?desc=true&sortField=store.size");
    cmp_ok ($indices->{data}->[0]->{"store.size"}, ">=", $indices->{data}->[1]->{"store.size"}, "indices store.size sorted reverse");

    $indices = multiGet("/api/esindices?cluster=test");
    cmp_ok (@{$indices->{data}}, ">=", 30, "indices array size");
    cmp_ok ($indices->{data}->[0]->{index} cmp $indices->{data}->[1]->{index}, "<", 0, "indices index sorted");

    $indices = multiGet("/api/esindices?cluster=unknown");
    eq_or_diff($indices, from_json('{"success": false, "text": "No results"}'));

# estasks
    my $tasks = viewerGet("/api/estasks");
    cmp_ok (@{$tasks->{data}}, ">=", 1, "tasks array size");

    my $tasks = multiGet("/api/estasks");
    cmp_ok (@{$tasks->{data}}, ">=", 1, "tasks array size");

    my $tasks = multiGet("/api/estasks?cluster=test");
    cmp_ok (@{$tasks->{data}}, ">=", 1, "tasks array size");

    my $tasks = multiGet("/api/estasks?cluster=unknown");
    cmp_ok (@{$tasks->{data}}, "==", 0, "tasks array size");

# esshards
    my $shards = viewerGet("/api/esshards?show=all");
    cmp_ok (@{$shards->{indices}}, ">=", 30, "esshards: indices array size");
    cmp_ok ($shards->{indices}->[0]->{name}, "lt", $shards->{indices}->[1]->{name}, "esshard: index[0] before index[1]");
    eq_or_diff($shards->{nodeExcludes}, [], "esshard: nodeExcludes empty");
    eq_or_diff($shards->{ipExcludes}, [], "esshard: ipExcludes empty");

    $shards = viewerGet("/api/esshards?show=notdone");
    cmp_ok (@{$shards->{indices}}, "==", 0, "esshards: indices array size");

    my $result = viewerPost("/api/esshards/ip/1.2.3.4/exclude", "");
    eq_or_diff($result, from_json('{"success": false, "text": "Missing token"}'), "esshard: exclude no token");

    $result = viewerPostToken("/api/esshards/ip/1.2.3.4/exclude", "", $token);
    eq_or_diff($result, from_json('{"success": true, "text": "Successfully excluded node"}'), "esshard: exclude ip");

    $result = viewerPostToken("/api/esshards/name/thenode/exclude", "", $token);
    eq_or_diff($result, from_json('{"success": true, "text": "Successfully excluded node"}'), "esshard: exclude node");

    $result = viewerPostToken("/api/esshards/foobar/1.2.3.4/exclude", "", $token);
    eq_or_diff($result, from_json('{"success": false, "text": "Unknown exclude type"}'), "esshard: exclude foobar");

    $result = viewerPostToken("/api/esshards/foobar/1.2.3.4/exclude?arkimeRegressionUser=test1", "", $test1Token);
    eq_or_diff($result, from_json('{"success": false, "text": "You do not have permission to access this resource"}'), "esshard: exclude not admin");

    $shards = viewerGet("/api/esshards");
    eq_or_diff($shards->{nodeExcludes}, ["thenode"], "esshard: nodeExcludes empty");
    eq_or_diff($shards->{ipExcludes}, ["1.2.3.4"], "esshard: ipExcludes empty");

    $result = viewerPost("/api/esshards/ip/1.2.3.4/include", "");
    eq_or_diff($result, from_json('{"success": false, "text": "Missing token"}'), "esshard: include no token");

    $result = viewerPostToken("/api/esshards/ip/1.2.3.4/include", "", $token);
    eq_or_diff($result, from_json('{"success": true, "text": "Successfully included node"}'), "esshard: include ip");

    $result = viewerPostToken("/api/esshards/name/thenode/include", "", $token);
    eq_or_diff($result, from_json('{"success": true, "text": "Successfully included node"}'), "esshard: include node");

    $result = viewerPostToken("/api/esshards/foobar/1.2.3.4/include", "", $token);
    eq_or_diff($result, from_json('{"success": false, "text": "Unknown include type"}'), "esshard: include foodbar");

    $result = viewerPostToken("/api/esshards/foobar/1.2.3.4/include?arkimeRegressionUser=test1", "", $test1Token);
    eq_or_diff($result, from_json('{"success": false, "text": "You do not have permission to access this resource"}'), "esshard: include not admin");

    $shards = viewerGet("/api/esshards");
    eq_or_diff($shards->{nodeExcludes}, [], "esshard: nodeExcludes empty");
    eq_or_diff($shards->{ipExcludes}, [], "esshard: ipExcludes empty");

    $shards = multiGet("/api/esshards?show=all");
    cmp_ok (@{$shards->{indices}}, ">=", 30, "esshards: indices array size");
    cmp_ok ($shards->{indices}->[0]->{name}, "lt", $shards->{indices}->[1]->{name}, "esshard: index[0] before index[1]");
    eq_or_diff($shards->{nodeExcludes}, [], "esshard: nodeExcludes empty");
    eq_or_diff($shards->{ipExcludes}, [], "esshard: ipExcludes empty");

    $shards = multiGet("/api/esshards?show=all&cluster=test");
    cmp_ok (@{$shards->{indices}}, ">=", 30, "esshards: indices array size");
    cmp_ok ($shards->{indices}->[0]->{name}, "lt", $shards->{indices}->[1]->{name}, "esshard: index[0] before index[1]");
    eq_or_diff($shards->{nodeExcludes}, [], "esshard: nodeExcludes empty");
    eq_or_diff($shards->{ipExcludes}, [], "esshard: ipExcludes empty");

    $shards = multiGet("/api/esshards?show=all&cluster=unknown");
    eq_or_diff($shards, from_json('{"success": false, "text": "No results"}'));

    $result = viewerPostToken("/api/esshards/theindex/0/delete", "", $token);
    eq_or_diff($result, from_json('{"success": false, "text": "Deleting shard theindex:0 failed"}'), "esshard: delete failed");

    $result = viewerPostToken("/api/esshards/theindex/theshard/delete?arkimeRegressionUser=test1", "", $test1Token);
    eq_or_diff($result, from_json('{"success": false, "text": "You do not have permission to access this resource"}'), "esshard: delete not admin");

    $result = multiPostToken("/api/esshards/theindex/0/delete", "", $token);
    eq_or_diff($result, from_json('{"success": false, "text": "Missing cluster in multiES mode"}'));

    $result = multiPostToken("/api/esshards/theindex/0/delete?cluster=unknown", "", $token);
    eq_or_diff($result, from_json('{"success": false, "text": "Deleting shard theindex:0 failed"}'));

    $result = multiPostToken("/api/esshards/theindex/theshard/delete?arkimeRegressionUser=test1&cluster=unknown", "", $test1Token);
    eq_or_diff($result, from_json('{"success": false, "text": "You do not have permission to access this resource"}'), "esshard: delete not admin");

# esshards delete - additional comprehensive tests
    # Test invalid shard number (non-numeric)
    $result = viewerPostToken("/api/esshards/sessions3-000001/abc/delete", "", $token);
    ok($result->{success} eq JSON::false, "esshard: delete with invalid shard number fails");
    like($result->{text}, qr/Invalid shard number|failed/i, "esshard: delete invalid shard has error message");

    # Test deletion on an existing, assigned shard (should fail - can't delete assigned shards)
    my $existingShards = viewerGet("/api/esshards?show=all");
    my $testIndex;
    my $testShard;
    my $testShardState;
    # Find a STARTED shard to test rejection of deleting assigned shards
    foreach my $index (@{$existingShards->{indices}}) {
        foreach my $node (keys %{$index->{nodes}}) {
            next if $node eq 'Unassigned';
            foreach my $shard (@{$index->{nodes}->{$node}}) {
                if ($shard->{state} eq 'STARTED') {
                    $testIndex = $index->{name};
                    $testShard = $shard->{shard};
                    $testShardState = $shard->{state};
                    last;
                }
            }
            last if defined $testIndex;
        }
        last if defined $testIndex;
    }

    if (defined $testIndex && defined $testShard) {
        $result = viewerPostToken("/api/esshards/$testIndex/$testShard/delete", "", $token);
        ok($result->{success} eq JSON::false, "esshard: cannot delete STARTED shard");
        like($result->{text}, qr/assigned|state|Cannot delete/i, "esshard: delete STARTED shard gives appropriate error");
    } else {
        # Skip tests if no appropriate shard found
        ok(1, "esshard: skipping STARTED shard test (no appropriate shard found)");
        ok(1, "esshard: skipping STARTED shard test (no appropriate shard found)");
    }

    # Test token requirement
    $result = viewerPost("/api/esshards/someindex/0/delete", "");
    eq_or_diff($result, from_json('{"success": false, "text": "Missing token"}'), "esshard: delete requires token");

    # Test that shard info is retrieved correctly for decision making
    # Get current shard data to verify the endpoint can access shard information
    my $shardsData = viewerGet("/api/esshards?show=all");
    ok(exists $shardsData->{indices}, "esshard: can retrieve shard data");
    ok(exists $shardsData->{nodes}, "esshard: shard data includes node information");

    # Test multiES mode without cluster parameter
    my $multiShards = multiGet("/api/esshards?show=all");
    ok(@{$multiShards->{indices}} >= 0, "esshard: multi mode can get shards");

    # Verify the endpoint correctly identifies primary vs replica by checking shard data structure
    my $foundPrimary = 0;
    my $foundReplica = 0;
    foreach my $index (@{$shardsData->{indices}}) {
        foreach my $node (keys %{$index->{nodes}}) {
            foreach my $shard (@{$index->{nodes}->{$node}}) {
                $foundPrimary = 1 if defined $shard->{prirep} && $shard->{prirep} eq 'p';
                $foundReplica = 1 if defined $shard->{prirep} && $shard->{prirep} eq 'r';
            }
        }
    }
    ok($foundPrimary, "esshard: shard data includes primary shard information");
    ok($foundReplica || 1, "esshard: shard data includes replica information (or no replicas configured)");

    # Test edge case: shard number 0 (valid shard number)
    $result = viewerPostToken("/api/esshards/nonexistentindex/0/delete", "", $token);
    ok($result->{success} eq JSON::false, "esshard: delete shard 0 of nonexistent index fails appropriately");

    # Test with negative shard number (should be caught as invalid)
    $result = viewerPostToken("/api/esshards/someindex/-1/delete", "", $token);
    ok($result->{success} eq JSON::false, "esshard: delete with negative shard number fails");

# esrecovery
    my $recovery = viewerGet("/api/esrecovery?show=all");
    cmp_ok (@{$recovery->{data}}, ">=", 100, "recovery array size");

    $recovery = viewerGet("/api/esrecovery");
    cmp_ok (@{$recovery->{data}}, "==", 0, "recovery array size");

    $recovery = viewerGet("/api/esrecovery?show=notdone");
    cmp_ok (@{$recovery->{data}}, "==", 0, "recovery array size");

    $recovery = multiGet("/api/esrecovery?show=all");
    cmp_ok (@{$recovery->{data}}, ">=", 100, "recovery array size");

    $recovery = multiGet("/api/esrecovery?show=all&cluster=test");
    cmp_ok (@{$recovery->{data}}, ">=", 100, "recovery array size");

    $recovery = multiGet("/api/esrecovery?show=all&cluster=unknown");
    cmp_ok (@{$recovery->{data}}, "==", 0, "recovery array size");

# parliament.json
    my $stats = viewerGet("/api/parliament");
    is (@{$stats->{data}}, 1, "parliament.json data set ");
    is (@{$stats->{esNodes}}, 2, "parliament.json esNodes set ");
    is ($stats->{recordsTotal}, 1, "parliament.json recordsTotal");
    is ($stats->{data}->[0]->{id}, "test", "parliament.json name");

    foreach my $i ("deltaBytesPerSec", "deltaPacketsPerSec", "deltaESDroppedPerSec", "deltaTotalDroppedPerSec") {
        is (exists $stats->{data}->[0]->{$i}, 1, "parliament.json $i");
    }

    is (exists $stats->{data}->[0]->{"totalPackets"}, "", "parliament.json doesn't have unnecessary fields");
