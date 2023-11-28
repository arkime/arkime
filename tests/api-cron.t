use Test::More tests => 42;
use Cwd;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use URI::Escape;
use strict;

my $json;
my $token = getTokenCookie();
my $suffix = int(rand()*1000000);

clearIndex("tests_queries");

$json = viewerPostToken("/api/user", '{"userId": "sac-test1", "userName": "UserName", "enabled":true, "password":"password", "roles":["arkimeUser"]}', $token);
$json = viewerPostToken("/api/user", '{"userId": "sac-test2", "userName": "UserName", "enabled":true, "password":"password", "roles":["arkimeUser", "cont3xtUser"]}', $token);

my $test1Token = getTokenCookie("sac-test1");
my $test2Token = getTokenCookie("sac-test2");

# periodic query field validation
$json = viewerPostToken("/api/cron", '{}', $token);
ok(!$json->{success}, "query must have name");
$json = viewerPostToken("/api/cron", '{"name":"sac-test1"}', $token);
ok(!$json->{success}, "query must have query expression");
$json = viewerPostToken("/api/cron", '{"name":"sac-test1","query":"protocols == tls"}', $token);
ok(!$json->{success}, "query must have query action");
$json = viewerPostToken("/api/cron", '{"name":"sac-test1","query":"protocols == tls","action":"tag"}', $token);
ok(!$json->{success}, "query must have query tag(s)");

# can create periodic query
$json = viewerPostToken("/api/cron", '{"name":"sac-test1","query":"protocols == tls","action":"tag","tags":"tls"}', $token);
ok($json->{success}, "query can be created");
my $key = $json->{query}->{key};

# can update periodic query
$json = viewerPostToken("/api/cron/$key", '{"name":"sac-test1update","query":"protocols == tls","action":"tag","tags":"tls","users":"sac-test2,test3"}', $token);
ok($json->{success}, "query can be updated");
eq_or_diff($json->{query}->{name}, "sac-test1update", "query was updated");
eq_or_diff($json->{query}->{users}, "sac-test2", "query users was updated");
eq_or_diff($json->{invalidUsers}->[0], "test3", "returns invalid users");

# can get periodic queries
$json = viewerGetToken("/api/crons", $token);
eq_or_diff($json->[0]->{name}, "sac-test1update", "can fetch queries");

# sac-test2 can see query because it is shared with their user
$json = viewerGetToken("/api/crons?arkimeRegressionUser=sac-test2", $test2Token);
eq_or_diff($json->[0]->{name}, "sac-test1update", "sac-test2 user can see query");
ok(!exists $json->[0]->{users}, "sac-test2 user cannot see users");
ok(!exists $json->[0]->{roles}, "sac-test2 user cannot see roles");

# but sac-test1 cannot
$json = viewerGetToken("/api/crons?arkimeRegressionUser=sac-test1", $test1Token);
is (@{$json}, 0, "sac-test1 cannot see queries not shared with them");

# update roles
$json = viewerPostToken("/api/cron/$key", '{"name":"sac-test1update","query":"protocols == tls","action":"tag","tags":"tls","users":"sac-test2,test3", "roles":["arkimeUser"],"editRoles":["cont3xtUser"]}', $token);
ok($json->{success}, "query roles can be updated");
eq_or_diff($json->{query}->{roles}->[0], "arkimeUser", "query roles was updated");
eq_or_diff($json->{query}->{editRoles}->[0], "cont3xtUser", "query editRoles was updated");

# can share with sac-test1 via roles
$json = viewerGetToken("/api/crons?arkimeRegressionUser=sac-test1", $test1Token);
eq_or_diff($json->[0]->{name}, "sac-test1update", "sac-test1 user can see query");
ok(!exists $json->[0]->{users}, "sac-test1 user cannot see users");
ok(!exists $json->[0]->{roles}, "sac-test1 user cannot see roles");

# admin can view all periodic queries when all param is supplied
my $files = '(file == */https-connect.pcap || file == */https-generalizedtime.pcap || file == */https2-301-get.pcap || file == */https3-301-get.pcap)';
$json = viewerPostToken("/api/cron?arkimeRegressionUser=sac-test1", qq({"name":"asdf","since":-1,"query":"protocols == tls && $files","action":"tag","tags":"test$suffix"}), $test1Token);
my $key2 = $json->{query}->{key};
$json = viewerGet("/api/crons?arkimeRegressionUser=anonymous");
is (@{$json}, 1, "returns 1 query without all flag");
$json = viewerGet("/api/crons?arkimeRegressionUser=anonymous&all=true");
is (@{$json}, 2, "returns 2 queries with all flag");


# sac-test2 can update using editRoles
$json = viewerPostToken("/api/cron/$key?arkimeRegressionUser=sac-test2", '{"name":"good name","query":"protocols == tls","action":"tag","tags":"tls","users":"sac-test2,test3", "roles":["arkimeUser"], "editRoles":["cont3xtUser"]}', $test2Token);
ok($json->{success}, "editRoles user can update query");

# shared user cannot update query
$json = viewerPostToken("/api/cron/$key?arkimeRegressionUser=sac-test1", '{"name":"bad name","query":"protocols == tls","action":"tag","tags":"tls","users":"sac-test2,test3", "roles":["arkimeUser"]}', $test1Token);
ok(!$json->{success}, "shared user cannot update query");

# shared user cannot delete query
$json = viewerDeleteToken("/api/cron/$key?arkimeRegressionUser=sac-test1", $test1Token);
ok(!$json->{success}, "shared user cannot delete query");

# sac-test1 cannot transfer ownership (not admin or creator)
$json = viewerPostToken("/api/cron/$key?arkimeRegressionUser=sac-test1", '{"creator":"sac-test1","name":"sac-test1update","query":"protocols == tls","action":"tag","tags":"tls","users":"sac-test2,test3", "roles":["arkimeUser"],"editRoles":["cont3xtUser"]}', $test1Token);
ok(!$json->{success}, "cannot transfer ownership without being admin or creator");
eq_or_diff($json->{text}, "Permission denied");

# can't transfer ownership to invalid user
$json = viewerPostToken("/api/cron/$key", '{"creator":"asdf","name":"sac-test1update","query":"protocols == tls","action":"tag","tags":"tls","users":"sac-test2,test3", "roles":["arkimeUser"],"editRoles":["cont3xtUser"]}', $token);
ok(!$json->{success}, "cannot transfer ownership to an invalid user");
eq_or_diff($json->{text}, "User not found");

# can transfer ownership
$json = viewerPostToken("/api/cron/$key", '{"creator":"sac-test1","name":"sac-test1update","query":"protocols == tls","action":"tag","tags":"tls","users":"sac-test2,test3", "roles":["arkimeUser"],"editRoles":["cont3xtUser"]}', $token);
ok($json->{success}, "can transfer ownership to valid user");
eq_or_diff($json->{query}->{creator}, "sac-test1");

# sac-test2 can delete using editRoles
$json = viewerDeleteToken("/api/cron/$key?arkimeRegressionUser=sac-test2", $test2Token);
ok($json->{success}, "query can be deleted");

# can not update primary-viewer periodic query
$json = viewerPostToken("/api/cron/primary-viewer", '{"name":"sac-test1update","query":"protocols == tls","action":"tag","tags":"tls","users":"sac-test2,test3"}', $token);
eq_or_diff($json, from_json('{"text": "Bad query key", "success": false}'));

# can not delete primary-viewer periodic queries
$json = viewerDeleteToken("/api/cron/primary-viewer", $token);
eq_or_diff($json, from_json('{"text": "Bad query key", "success": false}'));
# Run crons
viewerGet("/regressionTests/processCronQueries");
viewerGet("/regressionTests/processCronQueries");

# Check result
$json = viewerGet("/api/crons?arkimeRegressionUser=anonymous&all=true");
is ($json->[0]->{creator}, "sac-test1");
is ($json->[0]->{name}, "asdf");
is ($json->[0]->{key}, $key2);
is ($json->[0]->{count}, "4");

countTest(4, "date=-1&expression=" . uri_escape("${files} && protocols==tls"));
countTest(4, "date=-1&expression=" . uri_escape("${files} && tags=test${suffix}"));

# cleanup
$json = viewerDeleteToken("/api/cron/$key2?arkimeRegressionUser=sac-test1", $test1Token);
$json = viewerDeleteToken("/api/user/sac-test1", $token);
$json = viewerDeleteToken("/api/user/sac-test2", $token);
