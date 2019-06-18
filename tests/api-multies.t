use Test::More tests => 28;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Data::Dumper;
use Test::Differences;
use strict;

my $json;

    $json = mesGet("/");
    is ($json->{tagline}, "You Know, for Search", "ES tagline");
    ok (!exists $json->{status} || $json->{status} == 200, "ES no status or 200 status");

    $json = mesGet("/_template/MULTIPREFIX_sessions2_template?filter_path=**._meta&include_type_name=true");
    cmp_ok($json->{MULTIPREFIX_sessions2_template}->{mappings}->{session}->{_meta}->{molochDbVersion}, '>=', 50, "dstats version is at least 50");

#_stats
    $json = mesGet("/MULTIPREFIX_stats/_stats");
    is ($json->{_node}, "127.0.0.1:9200,prefix:tests", "Correct _node status");
    is (exists $json->{indices}->{MULTIPREFIX_stats_v4}, 1, "Correct stats/_stats index");

    $json = mesGet("/MULTIPREFIX_files/_stats");
    cmp_ok($json->{indices}->{MULTIPREFIX_files_v6}->{total}->{docs}->{count}, '>=', 60, "files count is at least 60");

    $json = mesGet("/MULTIPREFIX_sequence/_stats");
    cmp_ok($json->{indices}->{MULTIPREFIX_sequence_v3}->{total}->{docs}->{count}, '>=', 1, "sequence count is at least 1");

    $json = mesGet("/MULTIPREFIX_dstats/_stats");
    cmp_ok($json->{indices}->{MULTIPREFIX_dstats_v4}->{total}->{docs}->{count}, '>=', 1, "dstats count is at least 1");

# _count
    $json = mesPost("/MULTIPREFIX_users/_count?ignore_unavailable=true", "");
    is ($json->{count}, 0, "Correct count number of users");

    $json = mesPost("/MULTIPREFIX_sessions2-*/_count?ignore_unavailable=true", "");
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

# _nodes

    $json = mesGet("/_nodes/stats?fs=1");
    is ($json->{_node}, "127.0.0.1:9200,prefix:tests", "Correct _node status");

    $json = mesGet("/_nodes/stats?jvm=1&process=1&fs=1&search=1&os=1");
    is ($json->{_node}, "127.0.0.1:9200,prefix:tests", "Correct _node status");

# aliases
    $json = mesGet("/MULTIPREFIX_sessions2-*/_alias");
    is (exists $json->{"MULTIPREFIX_sessions2-050330"}, 1, "Correct session alias");
    is (exists $json->{"MULTIPREFIX_sessions2-140113"}, 1, "Correct session alias");

# _search

    $json = mesGet("/MULTIPREFIX_stats/stat/_search?rest_total_hits_as_int=true");
    cmp_ok($json->{hits}->{total}, '>=', 1, "stats/search count is at least 1");
    is ($json->{hits}->{hits}->[0]->{_index}, "MULTIPREFIX_stats_v4", "Correct stats index name");

    $json = mesPost("/MULTIPREFIX_stats/stat/_search?rest_total_hits_as_int=true", "{}");
    cmp_ok($json->{hits}->{total}, '>=', 1, "stats/search count is at least 1");
    is ($json->{hits}->{hits}->[0]->{_index}, "MULTIPREFIX_stats_v4", "Correct stats index name");

    $json = mesPost("/MULTIPREFIX_fields/field/_search?rest_total_hits_as_int=true", "{\"size\":1000}");
    cmp_ok($json->{hits}->{total}, '>=', 190, "fields count is at least 190");
    cmp_ok($json->{hits}->{total}, '<',  400, "fields count is less then 400");
    is ($json->{hits}->{hits}->[0]->{_index}, "MULTIPREFIX_fields_v3", "Correct fields index name");

    $json = mesGet("/MULTIPREFIX_sessions2-141015/session/_search?preference=primary_first&ignore_unavailable=true&rest_total_hits_as_int=true");
    is ($json->{hits}->{hits}->[0]->{_index}, "MULTIPREFIX_sessions2-141015", "Correct sessions index name");
    cmp_ok($json->{hits}->{total}, '>=', 6, "sessions count is at least 6");

    #print Dumper($json);
