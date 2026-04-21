use Test::More tests => 43;
use Cwd;
use URI::Escape;
use ArkimeTest;
use JSON;
use Data::Dumper;
use Test::Differences;
use strict;

# clean old users
    viewerGet("/regressionTests/deleteAllUsers");

my $json;

    $json = mesGet("/");
    ok ($json->{tagline} eq "You Know, for Search" || $json->{tagline} eq "The OpenSearch Project: https://opensearch.org/", "ES tagline");
    ok (!exists $json->{status} || $json->{status} == 200, "ES no status or 200 status");

    $json = mesGet("/_template/MULTIPREFIX_sessions3_template?filter_path=**._meta");
    cmp_ok($json->{MULTIPREFIX_sessions3_template}->{mappings}->{_meta}->{molochDbVersion}, '>=', 50, "dstats version is at least 50");

#_stats
    $json = mesGet("/MULTIPREFIX_stats/_stats");
    is ($json->{cluster}, "test", "Correct cluster status");
    is (exists $json->{indices}->{MULTIPREFIX_stats_v30}, 1, "Correct stats/_stats index");

    $json = mesGet("/MULTIPREFIX_files/_stats");
    cmp_ok($json->{indices}->{MULTIPREFIX_files_v30}->{total}->{docs}->{count}, '>=', 60, "files count is at least 60");

    $json = mesGet("/MULTIPREFIX_sequence/_stats");
    cmp_ok($json->{indices}->{MULTIPREFIX_sequence_v30}->{total}->{docs}->{count}, '>=', 1, "sequence count is at least 1");

    $json = mesGet("/MULTIPREFIX_dstats/_stats");
    cmp_ok($json->{indices}->{MULTIPREFIX_dstats_v30}->{total}->{docs}->{count}, '>=', 1, "dstats count is at least 1");

# _count
    $json = mesPost("/MULTIPREFIX_users/_count?ignore_unavailable=true", "");
    is ($json->{count}, 0, "Correct count number of users");

    $json = mesPost("/MULTIPREFIX_sessions2-*,MULTIPREFIX_sessions3-*/_count?ignore_unavailable=true", "");
    cmp_ok ($json->{count}, '>=', 80, "Correct count number of sessions");

    $json = mesPost("/MULTIPREFIX_stats*/_count?ignore_unavailable=true", "");
    cmp_ok ($json->{count}, '>=', 1, "Correct count number of stats");

    $json = mesPost("/MULTIPREFIX_files*/_count?ignore_unavailable=true", "");
    cmp_ok ($json->{count}, '>=', 60, "Correct count number of files");

# _cluster
    $json = mesGet("/_cluster/health");
    is ($json->{number_of_data_nodes}, 2, "Correct _cluster/health number of nodes");
    is ($json->{cluster_name}, "COMBINED", "Correct _cluster/health name");

# _cluster/settings
    $json = mesGet("/_cluster/settings");
    eq_or_diff($json, from_json('{"persistent": {}, "transient": {}}'));

# _cluster/_doc/details
    $json = mesGet("/_cluster/_doc/details");
    is ($json->{available}->[0], "test", "Correct available ES cluster");
    is ($json->{available}->[1], "test2", "Correct acvilable ES cluster");
    is ($json->{active}->[0], "test", "Correct active ES cluster");
    is ($json->{active}->[1], "test2", "Correct active ES cluster");
    is (scalar @{$json->{inactive}}, 0, "Correct number of ES cluster name");

# _node
    $json = mesGet("/_nodes/stats?fs=1");
    is ($json->{cluster}, "test", "Correct _node status");

    $json = mesGet("/_nodes/stats?jvm=1&process=1&fs=1&search=1&os=1");
    is ($json->{cluster}, "test", "Correct _node status");

# aliases
    $json = mesGet("/MULTIPREFIX_sessions2-*,MULTIPREFIX_sessions3-*/_alias");
    is (exists $json->{"MULTIPREFIX_sessions2-05m03"} || exists $json->{"MULTIPREFIX_sessions3-05m03"}, 1, "Correct session alias");
    is (exists $json->{"MULTIPREFIX_sessions2-14m01"} || exists $json->{"MULTIPREFIX_sessions3-14m01"}, 1, "Correct session alias");

# _search

    $json = mesGet("/MULTIPREFIX_stats/_search?rest_total_hits_as_int=true");
    cmp_ok($json->{hits}->{total}, '>=', 1, "stats/search count is at least 1");
    is ($json->{hits}->{hits}->[0]->{_index}, "MULTIPREFIX_stats_v30", "Correct stats index name");

    $json = mesPost("/MULTIPREFIX_stats/_search?rest_total_hits_as_int=true", "{}");
    cmp_ok($json->{hits}->{total}, '>=', 1, "stats/search count is at least 1");
    is ($json->{hits}->{hits}->[0]->{_index}, "MULTIPREFIX_stats_v30", "Correct stats index name");

    $json = mesPost("/MULTIPREFIX_fields/_search?rest_total_hits_as_int=true", "{\"size\":1000}");
    cmp_ok($json->{hits}->{total}, '>=', 590, "fields count is at least 590");
    cmp_ok($json->{hits}->{total}, '<',  800, "fields count is less then 800");
    is ($json->{hits}->{hits}->[0]->{_index}, "MULTIPREFIX_fields_v30", "Correct fields index name");

    $json = mesGet("/MULTIPREFIX_sessions3-14m10/_search?preference=primary_first&ignore_unavailable=true&rest_total_hits_as_int=true");
    if ($json->{hits}->{total} == 0) {
        $json = mesGet("/MULTIPREFIX_sessions2-14m10/_search?preference=primary_first&ignore_unavailable=true&rest_total_hits_as_int=true");
        is ($json->{hits}->{hits}->[0]->{_index}, "MULTIPREFIX_sessions2-14m10", "Correct sessions index name");
    } else {
        is ($json->{hits}->{hits}->[0]->{_index}, "MULTIPREFIX_sessions3-14m10", "Correct sessions index name");
    }
    cmp_ok($json->{hits}->{total}, '>=', 6, "sessions count is at least 6");

    $json = mesPost("/MULTIPREFIX_sessions3-14m10/_search?preference=primary_first&ignore_unavailable=true&rest_total_hits_as_int=true", '{"size":20000}');
    is ($json->{hits}->{hits}->[0]->{_index}, "MULTIPREFIX_sessions3-14m10", "Correct sessions index name");
    cmp_ok($json->{hits}->{total}, '>=', 6, "sessions count is at least 6");

    $json = mesPost("/MULTIPREFIX_sessions3-14m10/_search?preference=primary_first&ignore_unavailable=true&rest_total_hits_as_int=true", '{"from": 100, "size":20000}');
    is (scalar @{$json->{hits}->{hits}}, 0);
    cmp_ok($json->{hits}->{total}, '>=', 6, "sessions count is at least 6");

# invalid JSON body - simpleGather1Cluster
    my $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8200/_tasks/_cancel?cluster=test", Content => "NOT_JSON");
    is($response->code, 400, "invalid JSON body returns 400 for simpleGather1Cluster");
    my $body = from_json($response->content);
    is($body->{error}, "Invalid JSON body", "invalid JSON body error message for simpleGather1Cluster");

# invalid JSON body - _update route
    $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8200/MULTIPREFIX_stats/_update/foobar", Content => "NOT_JSON");
    is($response->code, 400, "invalid JSON body returns 400 for _update");
    $body = from_json($response->content);
    is($body->{error}, "Invalid JSON body", "invalid JSON body error message for _update");

# invalid JSON body - _search route
    $response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8200/MULTIPREFIX_stats/_search", Content => "NOT_JSON");
    is($response->code, 400, "invalid JSON body returns 400 for _search");
    $body = from_json($response->content);
    is($body->{error}, "Invalid JSON body", "invalid JSON body error message for _search");
