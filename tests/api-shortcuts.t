use Test::More tests => 92;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $token = getTokenCookie();
my $otherToken = getTokenCookie('user2');

viewerPostToken("/user/create", '{"userId": "user2", "userName": "user2", "enabled":true, "password":"password", "roles":["arkimeUser"]}', $token);

esPost("/tests_lookups/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }');
esPost("/tests2_lookups/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }');
multiPost("/regressionTests/flushCache");

# empty shortcuts
my $shortcuts = viewerGet("/lookups");
is(@{$shortcuts->{data}}, 0, "Empty shortcuts");
is($shortcuts->{recordsTotal}, 0, "Empty shortcuts with records total");
is($shortcuts->{recordsFiltered}, 0, "Empty shortcuts with records filtered");

# create shortcut required fields
my $json = viewerPostToken("/lookups", '{}', $token);
is($json->{text}, "Missing shortcut name", "shortcut name required");
$json = viewerPostToken("/lookups", '{"name":"test_shortcut"}', $token);
is($json->{text}, "Missing shortcut type", "shortcut type required");
$json = viewerPostToken("/api/shortcut", '{"name":"test_shortcut","type":"string"}', $token);
is($json->{text}, "Missing shortcut value", "shortcut value required");

# create shortcut requires token
$json = viewerPost("/lookups", '{"name":"test_shortcut","type":"string","value":"udp"}');
is($json->{text}, "Missing token", "create shortcut requires token");

# create shortcut
$json = viewerPostToken("/api/shortcut", '{"name":"test_shortcut~!@#$%^&*()+={}[]:;<>?,./","type":"string","value":"udp"}', $token);
ok($json->{success}, "create shortcut success");
ok(exists $json->{shortcut}->{id}, "returns shorcut with id");
my $shortcut1Id = $json->{shortcut}->{id}; # save id for cleanup later

# remove special chars from shortcut name
is($json->{shortcut}->{name}, "test_shortcut", "returns shortcut");

# shortcut names must be unique
$json = viewerPostToken("/lookups", '{"name":"test_shortcut","type":"string","value":"udp"}', $token);
ok(!$json->{success}, "unique shortcut names");

# update shortcut requires token
$json = viewerPut("/lookups/$shortcut1Id", "{}");
is($json->{text}, "Missing token", "update shortcut requires token");

# update shortcut required fields
$json = viewerPutToken("/lookups/$shortcut1Id", '{}', $token);
is($json->{text}, "Missing shortcut name", "shortcut name required");
$json = viewerPutToken("/lookups/$shortcut1Id", '{"name":"test_shortcut"}', $token);
is($json->{text}, "Missing shortcut type", "shortcut type required");
$json = viewerPutToken("/api/shortcut/$shortcut1Id", '{"name":"test_shortcut","type":"string"}', $token);
is($json->{text}, "Missing shortcut value", "shortcut value required");

# update shortcut
$json = viewerPutToken("/api/shortcut/$shortcut1Id", '{"name":"test_shortcut_updated","type":"ip","value":"10.0.0.1"}', $token);
is($json->{shortcut}->{name}, "test_shortcut_updated", "shortcut name updated");
is($json->{shortcut}->{value}, "10.0.0.1", "shortcut value updated");

# can update all fields of a shortcut
$json = viewerPutToken("/api/shortcut/$shortcut1Id", '{"name":"test_shortcut_updated","type":"string","value":"test","description":"test description"}', $token);
is($json->{shortcut}->{string}->[0], "test", "shortcut type updated");
is($json->{shortcut}->{value}, "test", "shortcut value updated");
is($json->{shortcut}->{description}, "test description", "shortcut description updated");
# turn it back to ip type for following tests to search for ip shortcuts
$json = viewerPutToken("/api/shortcut/$shortcut1Id", '{"name":"test_shortcut_updated","type":"ip","value":"10.0.0.1","description":"test description"}', $token);

# verify shortcut works
esGet("/_refresh");
countTest(1, "date=-1&expression=" . uri_escape("file=*/pcap/bt-udp.pcap&&ip.dst=\$test_shortcut_updated"));
countTest(2, "date=-1&expression=" . uri_escape("file=*/pcap/bt-udp.pcap&&ip.dst!=\$test_shortcut_updated"));
countTest(0, "molochRegressionUser=user2&date=-1&expression=" . uri_escape("file=*/pcap/bt-udp.pcap&&ip.dst=\$test_shortcut_updated"));
countTest(0, "molochRegressionUser=user2&date=-1&expression=" . uri_escape("file=*/pcap/bt-udp.pcap&&ip.dst!=\$test_shortcut_updated"));
countTest(2, "date=-1&expression=" . uri_escape("file=*/pcap/bt-udp.pcap&&ip!=\$test_shortcut_updated"));

# create another ip shortcut
$json = viewerPostToken("/api/shortcut", '{"name":"ip_shortcut","type":"ip","value":"10.0.0.3"}', $token);
my $ipShortcutId = $json->{shortcut}->{id};
# search by list of shortcuts
countTest(1, "date=-1&expression=" . uri_escape("file=*/pcap/bt-udp.pcap&&ip.dst!=[\$test_shortcut_updated,\$ip_shortcut]"));
countTest(2, "date=-1&expression=" . uri_escape("file=*/pcap/bt-udp.pcap&&ip.dst==[\$test_shortcut_updated,\$ip_shortcut]"));
countTest(2, "date=-1&expression=" . uri_escape("file=*/pcap/bt-udp.pcap&&ip==[\$test_shortcut_updated,\$ip_shortcut]"));
countTest(1, "date=-1&expression=" . uri_escape("file=*/pcap/bt-udp.pcap&&ip!=[\$test_shortcut_updated,\$ip_shortcut]"));

# same tests with multi
countTestMulti(1, "date=-1&expression=" . uri_escape("file=*/pcap/bt-udp.pcap&&ip.dst=\$test_shortcut_updated"));
countTestMulti(2, "date=-1&expression=" . uri_escape("file=*/pcap/bt-udp.pcap&&ip.dst!=\$test_shortcut_updated"));
countTestMulti(0, "molochRegressionUser=user2&date=-1&expression=" . uri_escape("file=*/pcap/bt-udp.pcap&&ip.dst=\$test_shortcut_updated"));
countTestMulti(0, "molochRegressionUser=user2&date=-1&expression=" . uri_escape("file=*/pcap/bt-udp.pcap&&ip.dst!=\$test_shortcut_updated"));
countTestMulti(2, "date=-1&expression=" . uri_escape("file=*/pcap/bt-udp.pcap&&ip!=\$test_shortcut_updated"));
countTestMulti(1, "date=-1&expression=" . uri_escape("file=*/pcap/bt-udp.pcap&&ip.dst!=[\$test_shortcut_updated,\$ip_shortcut]"));
countTestMulti(2, "date=-1&expression=" . uri_escape("file=*/pcap/bt-udp.pcap&&ip.dst==[\$test_shortcut_updated,\$ip_shortcut]"));
countTestMulti(2, "date=-1&expression=" . uri_escape("file=*/pcap/bt-udp.pcap&&ip==[\$test_shortcut_updated,\$ip_shortcut]"));
countTestMulti(1, "date=-1&expression=" . uri_escape("file=*/pcap/bt-udp.pcap&&ip!=[\$test_shortcut_updated,\$ip_shortcut]"));

$json = viewerDeleteToken("/api/shortcut/$ipShortcutId", $token); # cleanup

# create shortcut by another user
$json = viewerPostToken("/api/shortcut?molochRegressionUser=user2", '{"name":"other_test_shortcut","type":"string","value":"udp"}', $otherToken);
ok($json->{success}, "create shortcut success");
my $shortcut2Id = $json->{shortcut}->{id}; # save id for cleanup later

# get shortcuts should have 1
$shortcuts = viewerGet("/api/shortcuts?molochRegressionUser=user2");
is(@{$shortcuts->{data}}, 1, "1 shortcut for this user");

# multi get shortcuts all should have 1
$shortcuts = multiGet("/api/shortcuts?molochRegressionUser=user2");
is(@{$shortcuts->{data}}, 1, "1 shortcut for this user");

# create a shortcut by another user
$json = viewerPostToken("/lookups?molochRegressionUser=user2", '{"name":"other_test_shortcut_2","type":"string","value":"udp"}', $otherToken);
ok($json->{success}, "create shortcut success");
my $shortcut3Id = $json->{shortcut}->{id}; # save id for cleanup later

# can't update shortcut and duplicate name
$json = viewerPutToken("/api/shortcut/$shortcut3Id", '{"name":"test_shortcut_updated","type":"ip","value":"10.0.0.1"}', $token);
ok(!$json->{success}, "unique shortcut names");

# unshared shortcut can't be seen by nonadmin users
$shortcuts = viewerGet('/api/shortcuts?molochRegressionUser=user3');
is(@{$shortcuts->{data}}, 0, "user3 has no shortcuts shared with them");

# can share shortcut with users
$json = viewerPostToken("/lookups", '{"name":"user_shared_shortcut","type":"string","value":"udp","users":"user2"}', $token);
ok($json->{success}, "create shortcut with users success");
is($json->{shortcut}->{users}->[0], "user2", "create user shared shortcut");
my $shortcut4Id = $json->{shortcut}->{id}; # save id for cleanup later

# user2 can see shortcut shared with just them
$shortcuts = viewerGet("/api/shortcuts?molochRegressionUser=user2");
is(@{$shortcuts->{data}}, 3, "3 shortcut for this user");

# but a user3 cannot see that shortcut
$shortcuts = viewerGet("/api/shortcuts?molochRegressionUser=user3");
is(@{$shortcuts->{data}}, 0, "0 shortcuts for this user");

# share shortcut with roles
$json = viewerPutToken("/api/shortcut/$shortcut4Id", '{"name":"role_shared_shortcut","type":"string","value":"udp","users":"","roles":["cont3xtUser"]}', $token);
ok($json->{success}, "create shortcut with roles success");
is($json->{shortcut}->{roles}->[0], "cont3xtUser", "create role shared shortcut");

# user2 can't see shortcut because they don't have that role
$shortcuts = viewerGet("/api/shortcuts?molochRegressionUser=user2");
is(@{$shortcuts->{data}}, 2, "2 shortcut for this user");

# user2 can see the shortcut because they have the role
$json = viewerPutToken("/api/shortcut/$shortcut4Id", '{"name":"role_shared_shortcut","type":"string","value":"udp","users":"","roles":["arkimeUser"]}', $token);
$shortcuts = viewerGet("/api/shortcuts?molochRegressionUser=user2");
is(@{$shortcuts->{data}}, 3, "3 shortcut for this user");

# but they can't see users and roles fields if the shortcut is shared with them (they didn't create it)
is($shortcuts->{data}->[2]->{roles}, undef, "can't see roles field if it's a shared shortcut");
is($shortcuts->{data}->[2]->{users}, undef, "can't see users field if it's a shared shortcut");

# arkimeAdmin can view users and roles fields
$shortcuts = viewerGet("/api/shortcuts");
ok(exists $shortcuts->{data}->[0]->{roles}, 'arkimeAdmin can see roles');
ok(exists $shortcuts->{data}->[0]->{users}, 'arkimeAdmin can see users');

# get only shortcuts of a specific type
$shortcuts = viewerGet("/lookups?fieldType=string");
is(@{$shortcuts->{data}}, 2, "should be 2 shortcuts of type string");
is($shortcuts->{data}->[0]->{type}, 'string', 'shortcut should be of type string');
$shortcuts = viewerGet("/api/shortcuts?fieldType=ip");
is(@{$shortcuts->{data}}, 1, "should be 1 shortcuts of type ip");
is($shortcuts->{data}->[0]->{type}, 'ip', 'shortcut should be of type ip');

# get shortcuts map
$shortcuts = viewerGet("/lookups?map=true");
ok(exists $shortcuts->{$shortcut1Id} && exists $shortcuts->{$shortcut4Id}, "Request lookup map");

# get shortcuts formatted for typeahead
$shortcuts = viewerGet("/api/shortcuts?fieldFormat=true&map=true");
ok(exists $shortcuts->{$shortcut1Id}->{exp}, "Shortcut has exp");
ok(exists $shortcuts->{$shortcut1Id}->{help}, "Shortcut has help");
ok(exists $shortcuts->{$shortcut1Id}->{dbField}, "Shortcut has dbField");

# the local (test2) cluster should sync with the remote (test) cluster
sleep(1);
esGet("/_refresh");
viewerGet2("/api/syncshortcuts");
sleep(1);
esGet("/_refresh");
sleep(2);

my $testsCluster = esGet("/tests_lookups/_search?sort=name")->{hits}->{hits};
my $tests2Cluster = esGet("/tests2_lookups/_search?sort=name")->{hits}->{hits};

for (my $i=0; $i < scalar(@{$testsCluster}); $i++) { # indexes are different
  delete $testsCluster->[$i]->{_index};
  delete $tests2Cluster->[$i]->{_index};
}

eq_or_diff($testsCluster, $tests2Cluster, "cluster sync failed", { context => 2 });

# delete shortcut requires token
$json = viewerDelete("/lookups/$shortcut1Id");
is($json->{text}, "Missing token", "delete shortcut requires token");

# can't delete another user's shortcuts
$json = viewerDeleteToken("/lookups/$shortcut1Id?molochRegressionUser=user2", $otherToken);
ok(!$json->{success}, "can't delete another user's shortcut");
is($json->{text}, "Permission denied");

# can't delete shortcut that doesn't exist
$json = viewerDeleteToken("/api/shortcut/fakeshortcutid", $token);
ok(!$json->{success}, "can't delete a nonexisting shortcut");
is($json->{text}, "Fetching shortcut to delete failed");

# delete shortcut (plus bonus cleanup)
$json = viewerDeleteToken("/api/shortcut/$shortcut1Id", $token);
ok($json->{success}, "delete shortcut success");

# cleanup
$json = viewerDeleteToken("/lookups/$shortcut2Id", $token);
$json = viewerDeleteToken("/lookups/$shortcut3Id", $token);
$json = viewerDeleteToken("/lookups/$shortcut4Id", $token);

# make sure cleanup worked
$shortcuts = viewerGet("/lookups");
is(@{$shortcuts->{data}}, 0, "Empty shortcuts after cleanup");
$shortcuts = viewerGet("/api/shortcuts?molochRegressionUser=user2");
is(@{$shortcuts->{data}}, 0, "Empty shortcuts for user2 after cleanup");

# the local (test2) cluster should sync with the remote (test) cluster
viewerGet2("/api/syncshortcuts");
esGet("/_refresh");
sleep(2);

$testsCluster = esGet("/tests_lookups/_search?sort=name")->{hits}->{hits};
$tests2Cluster = esGet("/tests2_lookups/_search?sort=name")->{hits}->{hits};

for (my $i=0; $i < scalar(@{$testsCluster}); $i++) { # indexes are different
  delete $testsCluster->[$i]->{_index};
  delete $tests2Cluster->[$i]->{_index};
}

eq_or_diff($testsCluster, $tests2Cluster, "cluster sync failed", { context => 2 });

# remove shared user that gets added when creating shared shortcuts
viewerPostToken("/user/delete", "userId=_moloch_shared", $token);
# remove user2
viewerPostToken("/user/delete", "userId=user2", $token);
