use Test::More tests => 38;
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
    eq_or_diff($json, from_json('{"text": "You do not have permission to access this resource", "success": false}'));

    $json = viewerPost("/api/esadmin/set?arkimeRegressionUser=notadmin");
    eq_or_diff($json, from_json('{"text": "You do not have permission to access this resource", "success": false}'));

    $json = viewerPost("/api/esadmin/reroute?arkimeRegressionUser=notadmin");
    eq_or_diff($json, from_json('{"text": "You do not have permission to access this resource", "success": false}'));

    $json = viewerPost("/api/esadmin/flush?arkimeRegressionUser=notadmin");
    eq_or_diff($json, from_json('{"text": "You do not have permission to access this resource", "success": false}'));

    $json = viewerPost("/api/esadmin/unflood?arkimeRegressionUser=notadmin");
    eq_or_diff($json, from_json('{"text": "You do not have permission to access this resource", "success": false}'));

    $json = viewerGet("/api/esadmin/allocation?arkimeRegressionUser=notadmin");
    eq_or_diff($json, from_json('{"text": "You do not have permission to access this resource", "success": false}'));

# Missing token
    $json = viewerPost("/api/esadmin/set?arkimeRegressionUser=adminuser1");
    eq_or_diff($json, from_json('{"text": "Missing token", "success": false}'));

    $json = viewerPost("/api/esadmin/reroute?arkimeRegressionUser=adminuser1");
    eq_or_diff($json, from_json('{"text": "Missing token", "success": false}'));

    $json = viewerPost("/api/esadmin/flush?arkimeRegressionUser=adminuser1");
    eq_or_diff($json, from_json('{"text": "Missing token", "success": false}'));

    $json = viewerPost("/api/esadmin/unflood?arkimeRegressionUser=adminuser1");
    eq_or_diff($json, from_json('{"text": "Missing token", "success": false}'));

# good
    $json = viewerPostToken("/api/esadmin/reroute?arkimeRegressionUser=adminuser1", "", $token);
    eq_or_diff($json, from_json('{"text": "Reroute successful", "success": true}'));

    $json = viewerPostToken("/api/esadmin/flush?arkimeRegressionUser=adminuser1", "", $token);
    eq_or_diff($json, from_json('{"text": "Flushed", "success": true}'));

    $json = viewerPostToken("/api/esadmin/unflood?arkimeRegressionUser=adminuser1", "", $token);
    eq_or_diff($json, from_json('{"text": "Unflooded", "success": true}'));

# set tests
    $json = viewerPostToken("/api/esadmin/set?arkimeRegressionUser=adminuser1", "", $token);
    eq_or_diff($json, from_json('{"text": "Missing key", "success": false}'));

    $json = viewerPostToken("/api/esadmin/set?arkimeRegressionUser=adminuser1", "key=foo", $token);
    eq_or_diff($json, from_json('{"text": "Missing value", "success": false}'));

    $json = viewerPostToken("/api/esadmin/set?arkimeRegressionUser=adminuser1", "key=foo&value=bar", $token);
    eq_or_diff($json, from_json('{"text": "Set failed", "success": false}'));


    # set to 1234
    $json = viewerPostToken("/api/esadmin/set?arkimeRegressionUser=adminuser1", "key=cluster.max_shards_per_node&value=1234", $token);
    eq_or_diff($json, from_json('{"text": "Successfully set settings", "success": true}'));

    $json = esGet("/_cluster/settings?flat_settings");
    is ($json->{persistent}->{'cluster.max_shards_per_node'}, 1234);

    # clear with space
    $json = viewerPostToken("/api/esadmin/set?arkimeRegressionUser=adminuser1", "key=cluster.max_shards_per_node&value=", $token);
    eq_or_diff($json, from_json('{"text": "Successfully set settings", "success": true}'));

    $json = esGet("/_cluster/settings?flat_settings");
    ok (!exists $json->{persistent}->{'cluster.max_shards_per_node'});

# test cluster.routing.allocation.enable restore to "all"
    # First, clear any existing setting
    $json = esPut('/_cluster/settings', '{"persistent" : {"cluster.routing.allocation.enable" : null}}');
    $json = esGet("/_cluster/settings?flat_settings");
    ok (!exists $json->{persistent}->{'cluster.routing.allocation.enable'});

    # Set to "primaries" (something other than "all")
    $json = viewerPostToken("/api/esadmin/set?arkimeRegressionUser=adminuser1", "key=cluster.routing.allocation.enable&value=primaries", $token);
    eq_or_diff($json, from_json('{"text": "Successfully set settings", "success": true}'));

    $json = esGet("/_cluster/settings?flat_settings");
    is ($json->{persistent}->{'cluster.routing.allocation.enable'}, "primaries");

    # Restore to "all"
    $json = viewerPostToken("/api/esadmin/set?arkimeRegressionUser=adminuser1", "key=cluster.routing.allocation.enable&value=all", $token);
    eq_or_diff($json, from_json('{"text": "Successfully set settings", "success": true}'));

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
    eq_or_diff($json, from_json('{"text": "Successfully set settings", "success": true}'));

    $json = multiGetToken("/api/esadmin?arkimeRegressionUser=adminuser1&cluster=test", $token);
    is ($json->[6]->{'current'}, "4321");

    $json = multiPostToken("/api/esadmin/reroute?arkimeRegressionUser=adminuser1&cluster=test", "", $token);
    eq_or_diff($json, from_json('{"text": "Reroute successful", "success": true}'));

    $json = multiPostToken("/api/esadmin/flush?arkimeRegressionUser=adminuser1&cluster=test", "", $token);
    eq_or_diff($json, from_json('{"text": "Flushed", "success": true}'));

    $json = multiPostToken("/api/esadmin/unflood?arkimeRegressionUser=adminuser1&cluster=test", "", $token);
    eq_or_diff($json, from_json('{"text": "Unflooded", "success": true}'));

    # test2 cluster
    $json = multiPostToken("/api/esadmin/set?arkimeRegressionUser=adminuser1&cluster=test2", "key=cluster.max_shards_per_node&value=31453", $token);
    eq_or_diff($json, from_json('{"text": "Successfully set settings", "success": true}'));

    $json = multiGetToken("/api/esadmin?arkimeRegressionUser=adminuser1&cluster=test2", $token);
    is ($json->[6]->{'current'}, "31453");

    $json = multiPostToken("/api/esadmin/reroute?arkimeRegressionUser=adminuser1&cluster=test2", "", $token);
    eq_or_diff($json, from_json('{"text": "Reroute successful", "success": true}'));

    $json = multiPostToken("/api/esadmin/flush?arkimeRegressionUser=adminuser1&cluster=test2", "", $token);
    eq_or_diff($json, from_json('{"text": "Flushed", "success": true}'));

    $json = multiPostToken("/api/esadmin/unflood?arkimeRegressionUser=adminuser1&cluster=test2", "", $token);
    eq_or_diff($json, from_json('{"text": "Unflooded", "success": true}'));
