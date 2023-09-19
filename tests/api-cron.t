use Test::More tests => 36;
use Cwd;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use URI::Escape;
use strict;

my $json;
my $token = getTokenCookie();
my $suffix = int(rand()*1000000);

$json = viewerPostToken("/api/user", '{"userId": "test1", "userName": "UserName", "enabled":true, "password":"password", "roles":["arkimeUser"]}', $token);
$json = viewerPostToken("/api/user", '{"userId": "test2", "userName": "UserName", "enabled":true, "password":"password", "roles":["cont3xtUser"]}', $token);

my $test1Token = getTokenCookie("test1");
my $test2Token = getTokenCookie("test2");

# periodic query field validation
$json = viewerPostToken("/api/cron", '{}', $token);
ok(!$json->{success}, "query must have name");
$json = viewerPostToken("/api/cron", '{"name":"test1"}', $token);
ok(!$json->{success}, "query must have query expression");
$json = viewerPostToken("/api/cron", '{"name":"test1","query":"protocols == tls"}', $token);
ok(!$json->{success}, "query must have query action");
$json = viewerPostToken("/api/cron", '{"name":"test1","query":"protocols == tls","action":"tag"}', $token);
ok(!$json->{success}, "query must have query tag(s)");

# can create periodic query
$json = viewerPostToken("/api/cron", '{"name":"test1","query":"protocols == tls","action":"tag","tags":"tls"}', $token);
ok($json->{success}, "query can be created");
my $key = $json->{query}->{key};

# can update periodic query
$json = viewerPostToken("/api/cron/$key", '{"name":"test1update","query":"protocols == tls","action":"tag","tags":"tls","users":"test2,test3"}', $token);
ok($json->{success}, "query can be updated");
eq_or_diff($json->{query}->{name}, "test1update", "query was updated");
eq_or_diff($json->{query}->{users}, "test2", "query users was updated");
eq_or_diff($json->{invalidUsers}->[0], "test3", "returns invalid users");

# can get periodic queries
$json = viewerGetToken("/api/crons", $token);
eq_or_diff($json->[0]->{name}, "test1update", "can fetch queries");

# test2 can see query because it is shared with their user
$json = viewerGetToken("/api/crons?molochRegressionUser=test2", $test2Token);
eq_or_diff($json->[0]->{name}, "test1update", "test2 user can see query");
ok(!exists $json->[0]->{users}, "test2 user cannot see users");
ok(!exists $json->[0]->{roles}, "test2 user cannot see roles");

# but test1 cannot
$json = viewerGetToken("/api/crons?molochRegressionUser=test1", $test1Token);
is (@{$json}, 0, "test1 cannot see queries not shared with them");

# update roles
$json = viewerPostToken("/api/cron/$key", '{"name":"test1update","query":"protocols == tls","action":"tag","tags":"tls","users":"test2,test3", "roles":["arkimeUser"],"editRoles":["cont3xtUser"]}', $token);
ok($json->{success}, "query roles can be updated");
eq_or_diff($json->{query}->{roles}->[0], "arkimeUser", "query roles was updated");
eq_or_diff($json->{query}->{editRoles}->[0], "cont3xtUser", "query editRoles was updated");

# can share with test1 via roles
$json = viewerGetToken("/api/crons?molochRegressionUser=test1", $test1Token);
eq_or_diff($json->[0]->{name}, "test1update", "test1 user can see query");
ok(!exists $json->[0]->{users}, "test1 user cannot see users");
ok(!exists $json->[0]->{roles}, "test1 user cannot see roles");

# admin can view all periodic queries when all param is supplied
my $files = '(file == */https-connect.pcap || file == */https-generalizedtime.pcap || file == */https2-301-get.pcap || file == */https3-301-get.pcap)';
$json = viewerPostToken("/api/cron?molochRegressionUser=test1", qq({"name":"asdf","since":-1,"query":"protocols == tls && $files","action":"tag","tags":"test$suffix"}), $test1Token);
my $key2 = $json->{query}->{key};
$json = viewerGet("/api/crons?molochRegressionUser=anonymous");
is (@{$json}, 1, "returns 1 query without all flag");
$json = viewerGet("/api/crons?molochRegressionUser=anonymous&all=true");
is (@{$json}, 2, "returns 2 queries with all flag");

# test2 can update using editRoles
$json = viewerPostToken("/api/cron/$key?molochRegressionUser=test2", '{"name":"good name","query":"protocols == tls","action":"tag","tags":"tls","users":"test2,test3", "roles":["arkimeUser"], "editRoles":["cont3xtUser"]}', $test2Token);
ok($json->{success}, "editRoles user can update query");

# shared user cannot update query
$json = viewerPostToken("/api/cron/$key?molochRegressionUser=test1", '{"name":"bad name","query":"protocols == tls","action":"tag","tags":"tls","users":"test2,test3", "roles":["arkimeUser"]}', $test1Token);
ok(!$json->{success}, "shared user cannot update query");

# shared user cannot delete query
$json = viewerDeleteToken("/api/cron/$key?molochRegressionUser=test1", $test1Token);
ok(!$json->{success}, "shared user cannot delete query");

# test2 can delete using editRoles
$json = viewerDeleteToken("/api/cron/$key?molochRegressionUser=test2", $test2Token);
ok($json->{success}, "query can be deleted");

# can not update primary-viewer periodic query
$json = viewerPostToken("/api/cron/primary-viewer", '{"name":"test1update","query":"protocols == tls","action":"tag","tags":"tls","users":"test2,test3"}', $token);
eq_or_diff($json, from_json('{"text": "Bad query key", "success": false}'));

# can not delete primary-viewer periodic queries
$json = viewerDeleteToken("/api/cron/primary-viewer", $token);
eq_or_diff($json, from_json('{"text": "Bad query key", "success": false}'));

# Run crons
viewerGet("/regressionTests/processCronQueries");
viewerGet("/regressionTests/processCronQueries");

# Check result
$json = viewerGet("/api/crons?molochRegressionUser=anonymous&all=true");
is ($json->[0]->{creator}, "test1");
is ($json->[0]->{name}, "asdf");
is ($json->[0]->{key}, $key2);
is ($json->[0]->{count}, "4");

countTest(4, "date=-1&expression=" . uri_escape("${files} && protocols==tls"));
countTest(4, "date=-1&expression=" . uri_escape("${files} && tags=test${suffix}"));

# cleanup
$json = viewerDeleteToken("/api/cron/$key2?molochRegressionUser=test1", $test1Token);
$json = viewerDeleteToken("/api/user/test1", $token);
$json = viewerDeleteToken("/api/user/test2", $token);
