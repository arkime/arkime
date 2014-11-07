use Test::More tests => 34;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Data::Dumper;
use strict;

my $json;

    $json = mesGet("/");
    is ($json->{status}, 200, "Correct ES status");

    $json = mesGet("/MULTIPREFIX_dstats/version/version");
    cmp_ok($json->{_source}->{version}, '>=', 20, "dstats version is at least 20");
    is ($json->{_index}, "MULTIPREFIX_dstats_v1", "Correct dstats index name");

#_status
    $json = mesGet("/MULTIPREFIX_stats/_status");
    is ($json->{_node}, "127.0.0.1:9200,prefix:tests", "Correct _node status");
    is (exists $json->{indices}->{MULTIPREFIX_stats}, 1, "Correct stats/_status index");

    $json = mesGet("/MULTIPREFIX_tags/_status");
    is (exists $json->{indices}->{MULTIPREFIX_tags_v2}, 1, "Correct tags/_status version");
    cmp_ok($json->{indices}->{MULTIPREFIX_tags_v2}->{docs}->{num_docs}, '>=', 60, "tags count is at least 60");

    $json = mesGet("/MULTIPREFIX_files/_status");
    cmp_ok($json->{indices}->{MULTIPREFIX_files_v3}->{docs}->{num_docs}, '>=', 60, "files count is at least 60");

    $json = mesGet("/MULTIPREFIX_sequence/_status");
    cmp_ok($json->{indices}->{MULTIPREFIX_sequence}->{docs}->{num_docs}, '>=', 2, "sequence count is at least 2");

    $json = mesGet("/MULTIPREFIX_dstats/_status");
    cmp_ok($json->{indices}->{MULTIPREFIX_dstats_v1}->{docs}->{num_docs}, '>=', 2, "dstats count is at least 60");

# _count
    $json = mesPost("/users/_count?ignore_unavailable=true", "");
    is ($json->{count}, 0, "Correct count number of users");

    $json = mesPost("/MULTIPREFIX_sessions-*/_count?ignore_unavailable=true", "");
    cmp_ok ($json->{count}, '>=', 80, "Correct count number of sessions");

    $json = mesPost("/MULTIPREFIX_stats*/_count?ignore_unavailable=true", "");
    cmp_ok ($json->{count}, '>=', 1, "Correct count number of stats");

    $json = mesPost("/MULTIPREFIX_files*/_count?ignore_unavailable=true", "");
    cmp_ok ($json->{count}, '>=', 60, "Correct count number of files");

# _cluster
    $json = mesGet("/_cluster/health");
    is ($json->{number_of_data_nodes}, 2, "Correct _cluster/health number of nodes");
    is ($json->{cluster_name}, "COMBINED", "Correct _cluster/health name");

# _nodes

    $json = mesGet("/_nodes/stats?fs=1");
    is ($json->{_node}, "127.0.0.1:9200,prefix:tests", "Correct _node status");

    $json = mesGet("/_nodes/stats?jvm=1&process=1&fs=1&search=1&os=1");
    is ($json->{_node}, "127.0.0.1:9200,prefix:tests", "Correct _node status");

# aliases
    $json = mesGet("/MULTIPREFIX_sessions-*/_aliases");
    is (exists $json->{"MULTIPREFIX_sessions-050330"}, 1, "Correct session alias");
    is (exists $json->{"MULTIPREFIX_sessions-140113"}, 1, "Correct session alias");

# _search

    $json = mesGet("/MULTIPREFIX_stats/stat/_search");
    cmp_ok($json->{hits}->{total}, '>=', 1, "stats/search count is at least 1");
    is ($json->{hits}->{hits}->[0]->{_index}, "MULTIPREFIX_stats", "Correct stats index name");

    $json = mesPost("/MULTIPREFIX_stats/stat/_search", "{}");
    cmp_ok($json->{hits}->{total}, '>=', 1, "stats/search count is at least 1");
    is ($json->{hits}->{hits}->[0]->{_index}, "MULTIPREFIX_stats", "Correct stats index name");

    $json = mesPost("/MULTIPREFIX_fields/field/_search", "{size:1000}");
    cmp_ok($json->{hits}->{total}, '>=', 190, "fields count is at least 190");
    cmp_ok($json->{hits}->{total}, '<',  300, "fields count is less then 300");
    is ($json->{hits}->{hits}->[0]->{_index}, "MULTIPREFIX_fields", "Correct fields index name");

    $json = mesGet("/MULTIPREFIX_sessions-141015/session/_search?preference=_primary_first&ignoreIndices=missing&ignore_unavailable=true");
    is ($json->{hits}->{hits}->[0]->{_index}, "MULTIPREFIX_sessions-141015", "Correct sessions index name");
    cmp_ok($json->{hits}->{total}, '>=', 6, "sessions count is at least 6");

    $json = mesPost("/MULTIPREFIX_sessions-141015/session/_search?preference=_primary_first&ignoreIndices=missing&ignore_unavailable=true", '{"from":"0","size":100,"query":{"filtered":{"query":{"range":{"lp":{"gte":1413336071,"lte":1413339341}}}}},"facets":{"dbHisto":{"histogram":{"key_field":"lp","value_field":"db","interval":60,"size":1440}},"paHisto":{"histogram":{"key_field":"lp","value_field":"pa","interval":60,"size":1440}},"map":{"terms":{"fields":["g1","g2"],"size":1000}}},"sort":[{"fp":{"order":"asc"}},{"fpd":{"order":"asc"}}],"_source":["pr","ro","db","fp","lp","a1","p1","a2","p2","pa","by","no","us","g1","g2","esub","esrc","edst","efn","dnsho","tls","ircch"]}');
    is ($json->{hits}->{hits}->[0]->{_index}, "MULTIPREFIX_sessions-141015", "Correct sessions index name");
    cmp_ok($json->{hits}->{total}, '>=', 6, "sessions count is at least 6");

    is (exists $json->{facets}->{map}, 1, "Have map facet");
    is (exists $json->{facets}->{paHisto}, 1, "Have paHisto facet");
    is (exists $json->{facets}->{dbHisto}, 1, "Have dbHisto facet");

    #print Dumper($json);
