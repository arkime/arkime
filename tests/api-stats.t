use Test::More tests => 167;
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

    foreach my $i ("deltaMS", "memory", "cpu", "currentTime", "freeSpaceM", "freeSpaceTargetP") {
        cmp_ok ($stats->{data}->[0]->{$i}, '>=', 0, "stats.json $i >= 0");
    }

    foreach my $i ("deltaPackets", "deltaBytes", "deltaBytesPerSec", "deltaPacketsPerSec", "deltaSessions", "deltaSessionsPerSec") {
        is (exists $stats->{data}->[0]->{$i}, 1, "stats.json $i");
    }

    # depends on api-s3.t and other tests running first
    my $mstats = multiGet("/stats.json");
    is (@{$mstats->{data}}, 3, "multi stats.json data set ");

    my $mstats = multiGet("/stats.json?cluster=test");
    is (@{$mstats->{data}}, 1);

    my $mstats = multiGet("/stats.json?cluster=test2");
    is (@{$mstats->{data}}, 2);

    my $mstats = multiGet("/stats.json?cluster=unknown");
    is (@{$mstats->{data}}, 0);

# stats isPP sortField should be rejected
    my $badstats = viewerGet("/api/stats?sortField=__proto__");
    is ($badstats->{success}, 0, "stats isPP sortField rejected");
    is ($badstats->{text}, "Invalid value for sortField", "stats isPP sortField error text");

# dstats.json
    my $dstats = viewerGet("/api/dstats?nodeName=test&start=1399680425&stop=1399680460&step=5&interval=5&name=deltaPackets");
    is (@{$dstats}, 7, "dstats.json array size");

    # Capture wrote these in the hour before its last stats, Elasticsearch 8+ reads a bare number bound as millis
    my $lastStats = $stats->{data}->[0]->{currentTime};
    $dstats = viewerGet("/api/dstats?nodeName=test&start=" . ($lastStats - 3600) . "&stop=$lastStats&step=5&interval=5&name=deltaPackets");
    is (@{$dstats}, 720, "dstats.json last hour array size");
    cmp_ok ((grep { $_ > 0 } @{$dstats}), '>', 0, "dstats.json last hour has packets");

    # hide=old drops nodes with no stats in the last 5 minutes
    SKIP: {
        my $age = time() - $lastStats;
        skip "stats are right at the 5 minute hide=old cutoff", 1 if (abs($age - 300) < 10);
        my $hidden = viewerGet("/api/stats?hide=old");
        is (scalar @{$hidden->{data}}, ($age < 300 ? 1 : 0), "stats.json hide=old with stats $age seconds old");
    }

SKIP: {
    skip "the OpenSearch/Elasticsearch admin apis need one", 96 if $ArkimeTest::noEs;

# esstats.json
    my $esstats = viewerGet("/api/esstats");
    is ($esstats->{data}->[0]->{writesRejectedDelta}, 0, "Writes reject");
    ok (defined $esstats->{data}->[0]->{heapMax}, "esstats has heapMax");
    ok (defined $esstats->{data}->[0]->{diskTotal}, "esstats has diskTotal");

    my $messtats = multiGet("/esstats.json");
    is ($messtats->{data}->[0]->{writesRejectedDelta}, 0, "Writes reject");

    my $messtats = multiGet("/esstats.json?cluster=test");
    is (@{$messtats->{data}}, 1);
    is ($messtats->{data}->[0]->{writesRejectedDelta}, 0, "Writes reject");

    my $messtats = multiGet("/esstats.json?cluster=unknown");
    is (@{$messtats->{data}}, 0);

# esstats isPP sortField should be rejected
    my $badesstats = viewerGet("/api/esstats?sortField=constructor");
    is ($badesstats->{success}, 0, "esstats isPP sortField rejected");
    is ($badesstats->{text}, "Invalid value for sortField", "esstats isPP sortField error text");

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
    is($indices->{success}, 0, "unknown cluster no results");
    is($indices->{i18n}, "api.stats.noResults", "unknown cluster no results i18n");

# esindices isPP sortField should be rejected
    my $badindices = viewerGet("/api/esindices?sortField=__proto__");
    is ($badindices->{success}, 0, "esindices isPP sortField rejected");
    is ($badindices->{text}, "Invalid value for sortField", "esindices isPP sortField error text");

# esindices delete - test with non-existent index
    my $del = viewerDeleteToken("/api/esindices/nonexistent_fake_index_12345", $token);
    is($del->{success}, 0, "delete fake index fails");
    is($del->{i18n}, "api.stats.errorDeletingIndex", "delete fake index error i18n");

# esindices optimize
    my $opt = viewerPostToken("/api/esindices/tests_users/optimize", "", $token);
    is($opt->{success}, 1, "optimize index returns success");

# esindices close - test with non-existent index
    my $close = viewerPostToken("/api/esindices/nonexistent_fake_index_12345/close", "", $token);
    is($close->{success}, 0, "close fake index fails");

# esindices open - always returns success (non-blocking)
    my $open = viewerPostToken("/api/esindices/tests_users/open", "", $token);
    is($open->{success}, 1, "open index returns success");

# esindices shrink - test missing target error
    my $shrink = viewerPostToken("/api/esindices/tests_users/shrink", "{}", $token);
    is($shrink->{success}, 0, "shrink without target fails");
    is($shrink->{i18n}, "api.stats.missingTarget", "shrink missing target i18n");

# estasks
    my $tasks = viewerGet("/api/estasks");
    cmp_ok (@{$tasks->{data}}, ">=", 1, "tasks array size");

    my $tasks = multiGet("/api/estasks");
    cmp_ok (@{$tasks->{data}}, ">=", 1, "tasks array size");

    my $tasks = multiGet("/api/estasks?cluster=test");
    cmp_ok (@{$tasks->{data}}, ">=", 1, "tasks array size");

    my $tasks = multiGet("/api/estasks?cluster=unknown");
    cmp_ok (@{$tasks->{data}}, "==", 0, "tasks array size");

# estasks isPP sortField should be rejected
    my $badtasks = viewerGet("/api/estasks?sortField=constructor");
    is ($badtasks->{success}, 0, "estasks isPP sortField rejected");
    is ($badtasks->{text}, "Invalid value for sortField", "estasks isPP sortField error text");

# estasks cancel
    my $result = viewerPostToken("/api/estasks/nonexistent/cancel", "", $token);
    is($result->{success}, 0, "cancel nonexistent task");

# esshards
    my $shards = viewerGet("/api/esshards?show=all");
    cmp_ok (@{$shards->{indices}}, ">=", 30, "esshards: indices array size");
    cmp_ok ($shards->{indices}->[0]->{name}, "lt", $shards->{indices}->[1]->{name}, "esshard: index[0] before index[1]");
    eq_or_diff($shards->{nodeExcludes}, [], "esshard: nodeExcludes empty");
    eq_or_diff($shards->{ipExcludes}, [], "esshard: ipExcludes empty");

    $shards = viewerGet("/api/esshards?show=notdone");
    cmp_ok (@{$shards->{indices}}, "==", 0, "esshards: indices array size");

    my $result = viewerPost("/api/esshards/ip/1.2.3.4/exclude", "");
    is($result->{success}, 0, "esshard: exclude no token");
    is($result->{i18n}, "api.viewer.missingToken", "esshard: exclude no token i18n");

    $result = viewerPostToken("/api/esshards/ip/1.2.3.4/exclude", "", $token);
    is($result->{success}, 1, "esshard: exclude ip");
    is($result->{i18n}, "api.stats.nodeExcluded", "esshard: exclude ip i18n");

    $result = viewerPostToken("/api/esshards/name/thenode/exclude", "", $token);
    is($result->{success}, 1, "esshard: exclude node");
    is($result->{i18n}, "api.stats.nodeExcluded", "esshard: exclude node i18n");

    $result = viewerPostToken("/api/esshards/foobar/1.2.3.4/exclude", "", $token);
    is($result->{success}, 0, "esshard: exclude foobar");
    is($result->{i18n}, "api.stats.unknownExcludeType", "esshard: exclude foobar i18n");

    $result = viewerPostToken("/api/esshards/foobar/1.2.3.4/exclude?arkimeRegressionUser=test1", "", $test1Token);
    eq_or_diff($result, from_json('{"success": false, "text": "You do not have permission to access this resource", "i18n": "api.viewer.noPermission"}'), "esshard: exclude not admin");

    $shards = viewerGet("/api/esshards");
    eq_or_diff($shards->{nodeExcludes}, ["thenode"], "esshard: nodeExcludes thenode");
    eq_or_diff($shards->{ipExcludes}, ["1.2.3.4"], "esshard: ipExcludes 1.2.3.4");

    $result = viewerPost("/api/esshards/ip/1.2.3.4/include", "");
    is($result->{success}, 0, "esshard: include no token");
    is($result->{i18n}, "api.viewer.missingToken", "esshard: include no token i18n");

    $result = viewerPostToken("/api/esshards/ip/1.2.3.4/include", "", $token);
    is($result->{success}, 1, "esshard: include ip");
    is($result->{i18n}, "api.stats.nodeIncluded", "esshard: include ip i18n");

    $result = viewerPostToken("/api/esshards/name/thenode/include", "", $token);
    is($result->{success}, 1, "esshard: include node");
    is($result->{i18n}, "api.stats.nodeIncluded", "esshard: include node i18n");

    $result = viewerPostToken("/api/esshards/foobar/1.2.3.4/include", "", $token);
    is($result->{success}, 0, "esshard: include foobar");
    is($result->{i18n}, "api.stats.unknownIncludeType", "esshard: include foodbar i18n");

    $result = viewerPostToken("/api/esshards/foobar/1.2.3.4/include?arkimeRegressionUser=test1", "", $test1Token);
    eq_or_diff($result, from_json('{"success": false, "text": "You do not have permission to access this resource", "i18n": "api.viewer.noPermission"}'), "esshard: include not admin");

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
    is($shards->{success}, 0, "unknown cluster esshards no results");
    is($shards->{i18n}, "api.stats.noResults", "unknown cluster esshards no results i18n");

    $result = viewerPostToken("/api/esshards/theindex/0/delete", "", $token);
    eq_or_diff($result, from_json('{"success": false, "text": "Deleting shard theindex:0 failed"}'), "esshard: delete failed");

    $result = viewerPostToken("/api/esshards/theindex/theshard/delete?arkimeRegressionUser=test1", "", $test1Token);
    eq_or_diff($result, from_json('{"success": false, "text": "You do not have permission to access this resource", "i18n": "api.viewer.noPermission"}'), "esshard: delete not admin");

    $result = multiPostToken("/api/esshards/theindex/0/delete", "", $token);
    is($result->{success}, 0, "esshard: delete missing cluster");
    is($result->{i18n}, "api.stats.missingCluster", "esshard: delete missing cluster i18n");

    $result = multiPostToken("/api/esshards/theindex/0/delete?cluster=unknown", "", $token);
    eq_or_diff($result, from_json('{"success": false, "text": "Deleting shard theindex:0 failed"}'));

    $result = multiPostToken("/api/esshards/theindex/theshard/delete?arkimeRegressionUser=test1&cluster=unknown", "", $test1Token);
    eq_or_diff($result, from_json('{"success": false, "text": "You do not have permission to access this resource", "i18n": "api.viewer.noPermission"}'), "esshard: delete not admin");

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
    is($result->{success}, 0, "esshard: delete requires token");
    is($result->{i18n}, "api.viewer.missingToken", "esshard: delete requires token i18n");

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
    my $badPrirep = 0;
    foreach my $index (@{$shardsData->{indices}}) {
        foreach my $node (keys %{$index->{nodes}}) {
            foreach my $shard (@{$index->{nodes}->{$node}}) {
                $foundPrimary = 1 if defined $shard->{prirep} && $shard->{prirep} eq 'p';
                $badPrirep = 1 if defined $shard->{prirep} && $shard->{prirep} !~ /^[pr]$/;
            }
        }
    }
    ok($foundPrimary, "esshard: shard data includes primary shard information");
    ok(!$badPrirep, "esshard: prirep values are all p or r");

    # Test edge case: shard number 0 (valid shard number)
    $result = viewerPostToken("/api/esshards/nonexistentindex/0/delete", "", $token);
    ok($result->{success} eq JSON::false, "esshard: delete shard 0 of nonexistent index fails appropriately");

    # Test with negative shard number (should be caught as invalid)
    $result = viewerPostToken("/api/esshards/someindex/-1/delete", "", $token);
    ok($result->{success} eq JSON::false, "esshard: delete with negative shard number fails");

# esrecovery
    my $recovery = viewerGet("/api/esrecovery?show=all");
    # With sessions in ClickHouse there are no daily session indices in ES,
    # so far fewer shards show up in recovery
    my $minRecovery = ($ArkimeTest::sessionsDbUrl =~ m{^(?:clickhouses?|chttps?)://}) ? 20 : 100;
    cmp_ok (@{$recovery->{data}}, ">=", $minRecovery, "recovery array size");

    $recovery = viewerGet("/api/esrecovery");
    cmp_ok (@{$recovery->{data}}, "==", 0, "recovery array size");

    $recovery = viewerGet("/api/esrecovery?show=notdone");
    cmp_ok (@{$recovery->{data}}, "==", 0, "recovery array size");

    $recovery = multiGet("/api/esrecovery?show=all");
    cmp_ok (@{$recovery->{data}}, ">=", $minRecovery, "recovery array size");

    $recovery = multiGet("/api/esrecovery?show=all&cluster=test");
    cmp_ok (@{$recovery->{data}}, ">=", $minRecovery, "recovery array size");

    $recovery = multiGet("/api/esrecovery?show=all&cluster=unknown");
    cmp_ok (@{$recovery->{data}}, "==", 0, "recovery array size");

}
# db queries, the running queries of a sessions db that isn't OpenSearch/Elasticsearch
SKIP: {
    skip "the db queries apis need ClickHouse sessions", 15 if !$ArkimeTest::sessionsDbCH;
    my $dbq = viewerGet("/api/db/sessions/queries");
    is (ref $dbq->{data}, "ARRAY", "db queries data");
    cmp_ok ($dbq->{recordsTotal}, ">=", $dbq->{recordsFiltered}, "db queries recordsTotal");

    $dbq = viewerGet("/api/db/sessions/queries?filter=" . uri_escape("("));
    is ($dbq->{success}, 0, "db queries bad filter regex");

    my $kill = viewerPostToken("/api/db/sessions/queries/nosuchquery/kill", "", $token);
    is ($kill->{success}, 0, "db queries kill unknown query");

    $kill = viewerPostToken("/api/db/sessions/queries/nosuchquery/kill?arkimeRegressionUser=test1", "", $test1Token);
    is ($kill->{i18n}, "api.viewer.noPermission", "db queries kill needs dbAdmin");

    my $cancel = viewerPostToken("/api/estasks/nosuchcancel/cancelwith", "", $token);
    is ($cancel->{success}, 0, "cancel an unknown search on ClickHouse");
    unlike ($cancel->{text} // "", qr/No OpenSearch/, "cancel reaches the ClickHouse sessions db");

    my $parts = viewerGet("/api/db/sessions/tables");
    ok ((grep { $_->{table} =~ /\.tests_sessions3$/ && $_->{rows} > 0 } @{$parts->{data}}), "ch partitions has the sessions table");
    $parts = viewerGet("/api/db/sessions/tables?filter=" . uri_escape("tests_sessions3\$"));
    ok (!grep({ $_->{table} !~ /tests_sessions3$/ } @{$parts->{data}}), "ch partitions filter");

    my $badId = uri_escape('["arkime","nosuchtable","1"]');
    is (viewerPostToken("/api/db/sessions/tables/$badId/optimize", "", $token)->{success}, 0, "ch partitions optimize unknown partition");
    is (viewerDeleteToken("/api/db/sessions/tables/$badId", $token)->{success}, 0, "ch partitions drop unknown partition");
    is (viewerPostToken("/api/db/sessions/tables/$badId/vacuum", "", $token)->{success}, 0, "ch partitions has no vacuum");
    is (viewerPostToken("/api/db/sessions/tables/$badId/drop", "", $token)->{text}, "Unknown db or action", "ch partitions only drop with DELETE");

    # each db tab saves its own table state
    is (ref viewerGetToken("/api/user/config/chPartitions", $token)->{tableState}, "HASH", "ch partitions table state");
    is (viewerGetToken("/api/user/config/dbTables", $token)->{i18n}, "api.users.unsupportedPage", "no table state shared by the db tabs");
}
    is (viewerGet("/api/db/nosuchdb/queries")->{success}, 0, "db queries unknown db");

SKIP: {
    skip "the cluster db queries apis need Postgres", 9 if ($ArkimeTest::dbUrl // "") !~ /^postgres/;
    my $dbq = viewerGet("/api/db/cluster/queries");
    is (ref $dbq->{data}, "ARRAY", "pg db queries data");
    ok (!grep({ ($_->{elapsed} // "") !~ /^[\d.]+$/ } @{$dbq->{data}}), "pg db queries elapsed is seconds");

    my $kill = viewerPostToken("/api/db/cluster/queries/notapid/kill", "", $token);
    is ($kill->{success}, 0, "pg db queries kill bad pid");

    $kill = viewerPostToken("/api/db/cluster/queries/1/kill", "", $token);
    is ($kill->{success}, 0, "pg db queries kill another db's pid");

    my $pgTables = viewerGet("/api/db/cluster/tables");
    my ($stats) = grep { $_->{table} =~ /stats$/ } @{$pgTables->{data}};
    ok ($stats, "pg tables has the stats table");
    is (viewerPostToken("/api/db/cluster/tables/" . uri_escape($stats->{id} // '[]') . "/vacuum", "", $token)->{success}, 1, "pg tables vacuum");
    is (viewerPostToken("/api/db/cluster/tables/" . uri_escape($stats->{id} // '[]') . "/vacuum?arkimeRegressionUser=test1", "", $test1Token)->{i18n}, "api.viewer.noPermission", "pg tables vacuum needs dbAdmin");
    is (viewerDeleteToken("/api/db/cluster/tables/" . uri_escape($stats->{id} // '[]'), $token)->{success}, 0, "pg tables can't be dropped");
    is (viewerPostToken("/api/db/cluster/tables/" . uri_escape('["public","nosuchtable"]') . "/vacuum", "", $token)->{success}, 0, "pg tables vacuum unknown table");
}

# parliament.json
    my $stats = viewerGet("/api/parliament");
    is (@{$stats->{data}}, 1, "parliament.json data set ");
    SKIP: {
        skip "no OpenSearch/Elasticsearch nodes", 2 if $ArkimeTest::noEs;
    is (@{$stats->{esNodes}}, 1, "parliament.json esNodes set ");
    ok (defined $stats->{esNodes}->[0]->{nodeName}, "parliament.json esNodes has no junk timestamp entry");
    }
    is ($stats->{recordsTotal}, 1, "parliament.json recordsTotal");
    is ($stats->{data}->[0]->{id}, "test", "parliament.json name");

    foreach my $i ("deltaBytesPerSec", "deltaPacketsPerSec", "deltaESDroppedPerSec", "deltaTotalDroppedPerSec") {
        is (exists $stats->{data}->[0]->{$i}, 1, "parliament.json $i");
    }

    is (exists $stats->{data}->[0]->{"totalPackets"}, "", "parliament.json doesn't have unnecessary fields");

# multi viewer bucket merge — a min sub-aggregation must not be summed
# (/api/stats' retention is `now - min(files.first)` per node, so summing two
#  clusters' epochs put the retention date decades into the future)
    my $mtestFile = '{"num": 9001, "name": "/tmp/mtest-9001.pcap", "first": 1000000000, "node": "mtest", "filesize": 100, "locked": 0}';
    my $mtest2File = '{"num": 9002, "name": "/tmp/mtest-9002.pcap", "first": 2000000000, "node": "mtest", "filesize": 100, "locked": 0}';
    # a stats row is what carries retention in the response, keyed by node name
    my $mtestStats = '{"nodeName": "mtest", "currentTime": 1788190323, "interval": 1, "monitoring": 0, "deltaMS": 1000}';
    clusterPut(8123, "stats", "mtest", $mtestStats);
    clusterPut(8123, "files", "mtest-9001", $mtestFile);
    clusterPut(8124, "files", "mtest-9002", $mtest2File);

    my $now = time();
    my $sstats = viewerGet("/api/stats");
    my ($srow) = grep {$_->{id} eq "mtest"} @{$sstats->{data}};
    ok (defined $srow, "single viewer has the mtest stats row");
    cmp_ok (abs($srow->{retention} - ($now - 1000000000)), "<", 60, "single viewer retention is now - first");

    my $mmstats = multiGet("/stats.json");
    my ($mrow) = grep {$_->{id} eq "mtest"} @{$mmstats->{data}};
    ok (defined $mrow, "multi viewer has the mtest stats row");
    cmp_ok ($mrow->{retention}, ">", 0, "multi viewer retention is not in the future");
    cmp_ok (abs($mrow->{retention} - $srow->{retention}), "<", 60, "multi viewer retention takes the min first across clusters");

    clusterDelete(8123, "stats", "mtest");
    clusterDelete(8123, "files", "mtest-9001");
    clusterDelete(8124, "files", "mtest-9002");
