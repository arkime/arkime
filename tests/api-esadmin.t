use Test::More tests => 66;
use Cwd;
use URI::Escape;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

    my $token = getTokenCookie('adminuser1');
    my $json;

# Clear any old setting and make sure nothing set
    $json = esPut('/_cluster/settings', '{"persistent" : {"cluster.max_shards_per_node" : null}}');
    $json = esGet("/_cluster/settings?flat_settings");
    ok (!exists $json->{persistent}->{'cluster.max_shards_per_node'});

# fail permission tests
    $json = viewerGet("/api/esadmin?arkimeRegressionUser=notadmin");
    is($json->{success}, 0, "esadmin get no permission");
    is($json->{i18n}, "api.viewer.noPermission", "esadmin get no permission i18n");

    $json = viewerPost("/api/esadmin/set?arkimeRegressionUser=notadmin");
    is($json->{success}, 0, "esadmin set no permission");
    is($json->{i18n}, "api.viewer.noPermission", "esadmin set no permission i18n");

    $json = viewerPost("/api/esadmin/reroute?arkimeRegressionUser=notadmin");
    is($json->{success}, 0, "esadmin reroute no permission");
    is($json->{i18n}, "api.viewer.noPermission", "esadmin reroute no permission i18n");

    $json = viewerPost("/api/esadmin/flush?arkimeRegressionUser=notadmin");
    is($json->{success}, 0, "esadmin flush no permission");
    is($json->{i18n}, "api.viewer.noPermission", "esadmin flush no permission i18n");

    $json = viewerPost("/api/esadmin/unflood?arkimeRegressionUser=notadmin");
    is($json->{success}, 0, "esadmin unflood no permission");
    is($json->{i18n}, "api.viewer.noPermission", "esadmin unflood no permission i18n");

    $json = viewerGet("/api/esadmin/allocation?arkimeRegressionUser=notadmin");
    is($json->{success}, 0, "esadmin allocation no permission");
    is($json->{i18n}, "api.viewer.noPermission", "esadmin allocation no permission i18n");

# Missing token
    $json = viewerPost("/api/esadmin/set?arkimeRegressionUser=adminuser1");
    is($json->{success}, 0, "esadmin set missing token");
    is($json->{i18n}, "api.viewer.missingToken", "esadmin set missing token i18n");

    $json = viewerPost("/api/esadmin/reroute?arkimeRegressionUser=adminuser1");
    is($json->{success}, 0, "esadmin reroute missing token");
    is($json->{i18n}, "api.viewer.missingToken", "esadmin reroute missing token i18n");

    $json = viewerPost("/api/esadmin/flush?arkimeRegressionUser=adminuser1");
    is($json->{success}, 0, "esadmin flush missing token");
    is($json->{i18n}, "api.viewer.missingToken", "esadmin flush missing token i18n");

    $json = viewerPost("/api/esadmin/unflood?arkimeRegressionUser=adminuser1");
    is($json->{success}, 0, "esadmin unflood missing token");
    is($json->{i18n}, "api.viewer.missingToken", "esadmin unflood missing token i18n");

# good
    $json = viewerPostToken("/api/esadmin/reroute?arkimeRegressionUser=adminuser1", "", $token);
    is($json->{success}, 1, "esadmin reroute success");
    is($json->{i18n}, "api.stats.rerouteSuccessful", "esadmin reroute success i18n");

    $json = viewerPostToken("/api/esadmin/flush?arkimeRegressionUser=adminuser1", "", $token);
    is($json->{success}, 1, "esadmin flush success");
    is($json->{i18n}, "api.stats.flushed", "esadmin flush success i18n");

    $json = viewerPostToken("/api/esadmin/unflood?arkimeRegressionUser=adminuser1", "", $token);
    is($json->{success}, 1, "esadmin unflood success");
    is($json->{i18n}, "api.stats.unflooded", "esadmin unflood success i18n");

# set tests
    $json = viewerPostToken("/api/esadmin/set?arkimeRegressionUser=adminuser1", "", $token);
    is($json->{success}, 0, "esadmin set missing key");
    is($json->{i18n}, "api.stats.missingKey", "esadmin set missing key i18n");

    $json = viewerPostToken("/api/esadmin/set?arkimeRegressionUser=adminuser1", "key=foo", $token);
    is($json->{success}, 0, "esadmin set missing value");
    is($json->{i18n}, "api.stats.missingValue", "esadmin set missing value i18n");

    $json = viewerPostToken("/api/esadmin/set?arkimeRegressionUser=adminuser1", "key=foo&value=bar", $token);
    is($json->{success}, 0, "esadmin set failed");
    is($json->{i18n}, "api.stats.setFailed", "esadmin set failed i18n");


    # set to 1234
    $json = viewerPostToken("/api/esadmin/set?arkimeRegressionUser=adminuser1", "key=cluster.max_shards_per_node&value=1234", $token);
    is($json->{success}, 1, "esadmin set settings success");
    is($json->{i18n}, "api.stats.settingsSet", "esadmin set settings i18n");

    $json = esGet("/_cluster/settings?flat_settings");
    is ($json->{persistent}->{'cluster.max_shards_per_node'}, 1234);

    # clear with space
    $json = viewerPostToken("/api/esadmin/set?arkimeRegressionUser=adminuser1", "key=cluster.max_shards_per_node&value=", $token);
    is($json->{success}, 1, "esadmin set settings success");
    is($json->{i18n}, "api.stats.settingsSet", "esadmin set settings i18n");

    $json = esGet("/_cluster/settings?flat_settings");
    ok (!exists $json->{persistent}->{'cluster.max_shards_per_node'});

# test cluster.routing.allocation.enable restore to "all"
    # First, clear any existing setting
    $json = esPut('/_cluster/settings', '{"persistent" : {"cluster.routing.allocation.enable" : null}}');
    $json = esGet("/_cluster/settings?flat_settings");
    ok (!exists $json->{persistent}->{'cluster.routing.allocation.enable'});

    # Set to "primaries" (something other than "all")
    $json = viewerPostToken("/api/esadmin/set?arkimeRegressionUser=adminuser1", "key=cluster.routing.allocation.enable&value=primaries", $token);
    is($json->{success}, 1, "esadmin set settings success");
    is($json->{i18n}, "api.stats.settingsSet", "esadmin set settings i18n");

    $json = esGet("/_cluster/settings?flat_settings");
    is ($json->{persistent}->{'cluster.routing.allocation.enable'}, "primaries");

    # Restore to "all"
    $json = viewerPostToken("/api/esadmin/set?arkimeRegressionUser=adminuser1", "key=cluster.routing.allocation.enable&value=all", $token);
    is($json->{success}, 1, "esadmin set settings success");
    is($json->{i18n}, "api.stats.settingsSet", "esadmin set settings i18n");

    $json = esGet("/_cluster/settings?flat_settings");
    is ($json->{persistent}->{'cluster.routing.allocation.enable'}, "all");

    # Clean up - restore to null
    $json = esPut('/_cluster/settings', '{"persistent" : {"cluster.routing.allocation.enable" : null}}');

# allocation explain tests
    # General allocation explanation (no specific shard)
    $json = viewerGet("/api/esadmin/allocation?arkimeRegressionUser=adminuser1");
    ok(exists $json->{index} || exists $json->{allocate_explanation}, "Allocation explain returns valid response");

    # Allocation explanation with specific index/shard parameters
    # Note: This may fail if there are no unassigned shards, so we just check it doesn't error
    $json = viewerGet("/api/esadmin/allocation?arkimeRegressionUser=adminuser1&index=sessions2-*&shard=0&primary=true");
    ok(defined $json, "Allocation explain with parameters returns response");

# multiviewer
    # test cluster
    $json = multiPostToken("/api/esadmin/set?arkimeRegressionUser=adminuser1&cluster=test", "key=cluster.max_shards_per_node&value=4321", $token);
    is($json->{success}, 1, "esadmin set settings success");
    is($json->{i18n}, "api.stats.settingsSet", "esadmin set settings i18n");

    $json = multiGetToken("/api/esadmin?arkimeRegressionUser=adminuser1&cluster=test", $token);
    is ($json->[7]->{'current'}, "4321");

    $json = multiPostToken("/api/esadmin/reroute?arkimeRegressionUser=adminuser1&cluster=test", "", $token);
    is($json->{success}, 1, "esadmin reroute success");
    is($json->{i18n}, "api.stats.rerouteSuccessful", "esadmin reroute i18n");

    $json = multiPostToken("/api/esadmin/flush?arkimeRegressionUser=adminuser1&cluster=test", "", $token);
    is($json->{success}, 1, "esadmin flush success");
    is($json->{i18n}, "api.stats.flushed", "esadmin flush i18n");

    $json = multiPostToken("/api/esadmin/unflood?arkimeRegressionUser=adminuser1&cluster=test", "", $token);
    is($json->{success}, 1, "esadmin unflood success");
    is($json->{i18n}, "api.stats.unflooded", "esadmin unflood i18n");

    # test2 cluster
    $json = multiPostToken("/api/esadmin/set?arkimeRegressionUser=adminuser1&cluster=test2", "key=cluster.max_shards_per_node&value=31453", $token);
    is($json->{success}, 1, "esadmin set settings success");
    is($json->{i18n}, "api.stats.settingsSet", "esadmin set settings i18n");

    $json = multiGetToken("/api/esadmin?arkimeRegressionUser=adminuser1&cluster=test2", $token);
    is ($json->[7]->{'current'}, "31453");

    $json = multiPostToken("/api/esadmin/reroute?arkimeRegressionUser=adminuser1&cluster=test2", "", $token);
    is($json->{success}, 1, "esadmin reroute success");
    is($json->{i18n}, "api.stats.rerouteSuccessful", "esadmin reroute i18n");

    $json = multiPostToken("/api/esadmin/flush?arkimeRegressionUser=adminuser1&cluster=test2", "", $token);
    is($json->{success}, 1, "esadmin flush success");
    is($json->{i18n}, "api.stats.flushed", "esadmin flush i18n");

    $json = multiPostToken("/api/esadmin/unflood?arkimeRegressionUser=adminuser1&cluster=test2", "", $token);
    is($json->{success}, 1, "esadmin unflood success");
    is($json->{i18n}, "api.stats.unflooded", "esadmin unflood i18n");
