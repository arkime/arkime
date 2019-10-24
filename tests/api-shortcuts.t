use Test::More tests => 37;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $token = getTokenCookie();
my $otherToken = getTokenCookie('user2');

# empty shortcuts
my $shortcuts = viewerGet("/lookups");
is(@{$shortcuts->{data}}, 0, "Empty shortcuts");
is($shortcuts->{recordsTotal}, 0, "Empty shortcuts with records total");
is($shortcuts->{recordsFiltered}, 0, "Empty shortcuts with records filtered");

# create shortcut required fields
my $json = viewerPostToken("/lookups", '{}', $token);
is($json->{text}, "Missing shortcut", "shortcut object required");
$json = viewerPostToken("/lookups", '{"var":{}}', $token);
is($json->{text}, "Missing shortcut name", "shortcut name required");
$json = viewerPostToken("/lookups", '{"var":{"name":"test_shortcut"}}', $token);
is($json->{text}, "Missing shortcut type", "shortcut type required");
$json = viewerPostToken("/lookups", '{"var":{ "name":"test_shortcut","type":"string"}}', $token);
is($json->{text}, "Missing shortcut value", "shortcut value required");

# create shortcut requires token
$json = viewerPost("/lookups", '{"var":{"name":"test_shortcut","type":"string","value":"udp"}}');
is($json->{text}, "Missing token", "create shortcut requires token");

# create shortcut
$json = viewerPostToken("/lookups", '{"var":{"name":"test_shortcut~!@#$%^&*()+={}[]:;<>?,./","type":"string","value":"udp"}}', $token);
ok($json->{success}, "create shortcut success");
ok(exists $json->{var}->{id}, "returns shorcut with id");
my $shortcut1Id = $json->{var}->{id}; # save id for cleanup later

# remove special chars from shortcut name
is($json->{var}->{name}, "test_shortcut", "returns shortcut");

# shortcut names must be unique
$json = viewerPostToken("/lookups", '{"var":{"name":"test_shortcut","type":"string","value":"udp"}}', $token);
ok(!$json->{success}, "unique shortcut names");

# update shortcut requires token
$json = viewerPutToken("/lookups/$shortcut1Id", "notatoken");
is($json->{text}, "Missing token", "update shortcut requires token");

# update shortcut required fields
$json = viewerPutToken("/lookups/$shortcut1Id", '{}', $token);
is($json->{text}, "Missing shortcut", "shortcut object required");
$json = viewerPutToken("/lookups/$shortcut1Id", '{"var":{}}', $token);
is($json->{text}, "Missing shortcut name", "shortcut name required");
$json = viewerPutToken("/lookups/$shortcut1Id", '{"var":{"name":"test_shortcut"}}', $token);
is($json->{text}, "Missing shortcut type", "shortcut type required");
$json = viewerPutToken("/lookups/$shortcut1Id", '{"var":{ "name":"test_shortcut","type":"string"}}', $token);
is($json->{text}, "Missing shortcut value", "shortcut value required");

# update shortcut
$json = viewerPutToken("/lookups/$shortcut1Id", '{"var":{ "name":"test_shortcut_updated","type":"ip","value":"10.0.0.1"}}', $token);
is($json->{var}->{name}, "test_shortcut_updated", "shortcut name updated");
is($json->{var}->{value}, "10.0.0.1", "shortcut value updated");

# create shortcut by another user
$json = viewerPostToken("/lookups?molochRegressionUser=user2", '{"var":{"name":"other_test_shortcut","type":"string","value":"udp"}}', $otherToken);
ok($json->{success}, "create shortcut success");
my $shortcut2Id = $json->{var}->{id}; # save id for cleanup later

# get shortcuts should have 1
$shortcuts = viewerGet("/lookups?molochRegressionUser=user2");
is(@{$shortcuts->{data}}, 1, "1 shortcut for this user");

# create shared shortcut by another user
$json = viewerPostToken("/lookups?molochRegressionUser=user2", '{"var":{"shared":true,"name":"other_test_shortcut_2","type":"string","value":"udp"}}', $otherToken);
ok($json->{success}, "create shortcut success");
ok($json->{var}->{shared}, "create shared shortcut");
my $shortcut3Id = $json->{var}->{id}; # save id for cleanup later

# shared shortcut exists for user
$shortcuts = viewerGet("/lookups?molochRegressionUser=user2");
is(@{$shortcuts->{data}}, 2, "2 shortcuts for this user");

# unshared shortcut can't be seen by nonadmin users
$shortcuts = viewerGet('/lookups?molochRegressionUser=user3');
is(@{$shortcuts->{data}}, 1, "nonadmin user can only see shared shortcuts");

# get only shortcuts of a specific type
$shortcuts = viewerGet("/lookups?fieldType=string");
is(@{$shortcuts->{data}}, 2, "should be 2 shortcuts of type string");
is($shortcuts->{data}->[0]->{type}, 'string', 'shortcut should be of type string');
$shortcuts = viewerGet("/lookups?fieldType=ip");
is(@{$shortcuts->{data}}, 1, "should be 2 shortcuts of type ip");
is($shortcuts->{data}->[0]->{type}, 'ip', 'shortcut should be of type ip');

# get shortcuts map
$shortcuts = viewerGet("/lookups?map=true");
ok(exists $shortcuts->{$shortcut1Id} && exists $shortcuts->{$shortcut3Id}, "Request lookup map");

# get shortcuts formatted for typeahead
$shortcuts = viewerGet("/lookups?fieldFormat=true&map=true");
ok(exists $shortcuts->{$shortcut1Id}->{exp}, "Shortcut has exp");
ok(exists $shortcuts->{$shortcut1Id}->{help}, "Shortcut has help");
ok(exists $shortcuts->{$shortcut1Id}->{dbField}, "Shortcut has dbField");

# delete shortcute requires token
$json = viewerDelete("/lookups/$shortcut1Id");
is($json->{text}, "Missing token", "delete shortcut requires token");

# can't delete another user's shortcuts
$json = viewerDeleteToken("/lookups/$shortcut1Id?molochRegressionUser=user2", $otherToken);
ok(!$json->{success}, "can't delete another user's shortcut");

# can't delete shortcut that doesn't exist
$json = viewerDeleteToken("/lookups/fakeshortcutid", $token);
ok(!$json->{success}, "can't delete a nonexisting shortcut");

# delete shortcut (plus bonus cleanup)
$json = viewerDeleteToken("/lookups/$shortcut1Id", $token);
ok($json->{success}, "delete shortcut success");

# cleanup
$json = viewerDeleteToken("/lookups/$shortcut2Id?molochRegressionUser=user2", $otherToken);
$json = viewerDeleteToken("/lookups/$shortcut3Id?molochRegressionUser=user2", $otherToken);
