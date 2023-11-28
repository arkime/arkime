use Test::More tests => 93;
use Cwd;
use URI::Escape;
use ArkimeTest;
use Data::Dumper;
use JSON;
use Test::Differences;
use strict;

my $result;

my $version = 7;

# create user without parliament role
addUser("-n testuser arkimeUserP arkimeUserP arkimeUserP --roles 'arkimeUser' ");
# create user with parliament role
addUser("-n testuser parliamentUserP parliamentUserP parliamentUserP --roles 'parliamentUser' ");
# create user with parliament admin role
addUser("-n testuser parliamentAdminP parliamentAdminP parliamentAdminP --roles 'parliamentAdmin' ");

# authenticate non parliament user
$ArkimeTest::userAgent->credentials( "$ArkimeTest::host:8008", 'Moloch', 'arkimeUserP', 'arkimeUserP' );
my $arkimeUserToken = getParliamentTokenCookie('arkimeUserP');

# non parliament user can view parliament - empty
$result = parliamentGetToken("/parliament/api/parliament", $arkimeUserToken);
eq_or_diff($result, from_json('{"groups": [], "name": "parliamenttest"}'));

# non parliament user can view issues
$result = parliamentGetToken("/parliament/api/issues", $arkimeUserToken);
ok(exists $result->{issues});

# non parliament user cannot update issues
$result = parliamentPutToken("/parliament/api/acknowledgeIssues?arkimeRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament user"}'));
$result = parliamentPutToken("/parliament/api/ignoreIssues?arkimeRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament user"}'));
$result = parliamentPutToken("/parliament/api/removeIgnoreIssues?arkimeRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament user"}'));
$result = parliamentPutToken("/parliament/api/groups/0/clusters/0/removeIssue?arkimeRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament user"}'));
$result = parliamentPutToken("/parliament/api/issues/removeAllAcknowledgedIssues?arkimeRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament user"}'));
$result = parliamentPutToken("/parliament/api/removeSelectedAcknowledgedIssues?arkimeRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament user"}'));

# non parliamet user cannot access/udpate settings/parliament
$result = parliamentGetToken("/parliament/api/notifierTypes?arkimeRegressionUser=arkimeUserP", $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/notifier/test?arkimeRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken("/parliament/api/notifier?arkimeRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentDeleteToken("/parliament/api/notifier/test?arkimeRegressionUser=arkimeUserP", $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken("/parliament/api/notifier/id/test?arkimeRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/parliament/order?arkimeRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken("/parliament/api/groups?arkimeRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentDeleteToken("/parliament/api/groups/0?arkimeRegressionUser=arkimeUserP", $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/groups/0?arkimeRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken("/parliament/api/groups/0/clusters?arkimeRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentDeleteToken("/parliament/api/groups/0/clusters/0?arkimeRegressionUser=arkimeUserP", $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/groups/0/clusters/0?arkimeRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));

# authenticate parliament user
$ArkimeTest::userAgent->credentials( "$ArkimeTest::host:8008", 'Moloch', 'parliamentUserP', 'parliamentUserP' );
my $parliamentUserToken = getParliamentTokenCookie('parliamentUserP');

# parliament user can view parliament
$result = parliamentGetToken("/parliament/api/parliament?arkimeRegressionUser=parliamentUserP", $parliamentUserToken);
eq_or_diff($result, from_json('{"groups": [], "name": "parliamenttest" }'));

# parliament user can view issues
$result = parliamentGetToken("/parliament/api/issues?arkimeRegressionUser=parliamentUserP", $parliamentUserToken);
ok(exists $result->{issues});

# parliament user can access update issues endpoints
$result = parliamentPutToken("/parliament/api/acknowledgeIssues?arkimeRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue(s) to acknowledge.", "success": false}'));
$result = parliamentPutToken("/parliament/api/ignoreIssues?arkimeRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue(s) to ignore.", "success": false}'));
$result = parliamentPutToken("/parliament/api/removeIgnoreIssues?arkimeRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue(s) to unignore.", "success": false}'));
$result = parliamentPutToken("/parliament/api/groups/0/clusters/0/removeIssue?arkimeRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue type to remove.", "success": false}'));
$result = parliamentPutToken("/parliament/api/issues/removeAllAcknowledgedIssues?arkimeRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"text": "There are no acknowledged issues to remove.", "success": false}'));
$result = parliamentPutToken("/parliament/api/removeSelectedAcknowledgedIssues?arkimeRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"text": "Must specify the acknowledged issue(s) to remove.", "success": false}'));

# parliament user cannot access/udpate settings/parliament
$result = parliamentGetToken("/parliament/api/parliament?arkimeRegressionUser=parliamentUserP", $parliamentUserToken);
ok(!exists $result->{settings});
$result = parliamentPutToken("/parliament/api/settings?arkimeRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/settings/restoreDefaults?arkimeRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentGetToken("/parliament/api/notifierTypes?arkimeRegressionUser=parliamentUserP", $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/notifier/test?arkimeRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken("/parliament/api/notifier?arkimeRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentDeleteToken("/parliament/api/notifier/test?arkimeRegressionUser=parliamentUserP", $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken("/parliament/api/notifier/id/test?arkimeRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/parliament/order?arkimeRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken("/parliament/api/groups?arkimeRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentDeleteToken("/parliament/api/groups/0?arkimeRegressionUser=parliamentUserP", $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/groups/0?arkimeRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken("/parliament/api/groups/0/clusters?arkimeRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentDeleteToken("/parliament/api/groups/0/clusters/0?arkimeRegressionUser=parliamentUserP", $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/groups/0/clusters/0?arkimeRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));

# authenticate parliament admin
$ArkimeTest::userAgent->credentials( "$ArkimeTest::host:8008", 'Moloch', 'parliamentAdminP', 'parliamentAdminP' );
my $parliamentAdminToken = getParliamentTokenCookie('parliamentAdminP');

# parliament admin can view parliament
$result = parliamentGetToken("/parliament/api/parliament?arkimeRegressionUser=parliamentAdminP", $parliamentAdminToken);
delete $result->{settings};
eq_or_diff($result, from_json('{"groups": [], "name": "parliamenttest" }'));

# parliament admin can view issues
$result = parliamentGetToken("/parliament/api/issues?arkimeRegressionUser=parliamentAdminP", $parliamentAdminToken);
ok(exists $result->{issues});

# parliament admin can access update issues endpoints
$result = parliamentPutToken("/parliament/api/acknowledgeIssues?arkimeRegressionUser=parliamentAdminP", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue(s) to acknowledge.", "success": false}'));
$result = parliamentPutToken("/parliament/api/ignoreIssues?arkimeRegressionUser=parliamentAdminP", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue(s) to ignore.", "success": false}'));
$result = parliamentPutToken("/parliament/api/removeIgnoreIssues?arkimeRegressionUser=parliamentAdminP", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue(s) to unignore.", "success": false}'));
$result = parliamentPutToken("/parliament/api/groups/0/clusters/0/removeIssue?arkimeRegressionUser=parliamentAdminP", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue type to remove.", "success": false}'));
$result = parliamentPutToken("/parliament/api/issues/removeAllAcknowledgedIssues?arkimeRegressionUser=parliamentAdminP", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "There are no acknowledged issues to remove.", "success": false}'));
$result = parliamentPutToken("/parliament/api/removeSelectedAcknowledgedIssues?arkimeRegressionUser=parliamentAdminP", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Must specify the acknowledged issue(s) to remove.", "success": false}'));

# parliament admin can access/update settings/parliament
$result = parliamentGetToken("/parliament/api/parliament?arkimeRegressionUser=parliamentAdminP", $parliamentAdminToken);
ok(exists $result->{settings}->{general});
ok(exists $result->{settings}->{general}->{outOfDate});
ok(exists $result->{settings}->{general}->{noPackets});
ok(exists $result->{settings}->{general}->{esQueryTimeout});
ok(exists $result->{settings}->{general}->{removeIssuesAfter});
ok(exists $result->{settings}->{general}->{removeAcknowledgedAfter});

# need settings object
$result = parliamentPutToken("/parliament/api/settings?arkimeRegressionUser=parliamentAdminP", '{}', $parliamentAdminToken);
ok(!$result->{success});

# need settings object with general
$result = parliamentPutToken("/parliament/api/settings?arkimeRegressionUser=parliamentAdminP", '{"settings": {} }', $parliamentAdminToken);
ok(!$result->{success});

# can update settings
$result = parliamentPutToken("/parliament/api/settings?arkimeRegressionUser=parliamentAdminP", '{"settings": { "general": { "noPacketsLength": 100 } } }', $parliamentAdminToken);
ok($result->{success});
$result = parliamentGetToken("/parliament/api/parliament?arkimeRegressionUser=parliamentAdminP", $parliamentAdminToken);
eq_or_diff($result->{settings}->{general}->{noPacketsLength}, 100);

# notifier types have been initiated
$result = parliamentGetToken("/parliament/api/notifierTypes?arkimeRegressionUser=parliamentAdminP", $parliamentAdminToken);
ok(exists $result->{slack});
ok(exists $result->{email});
ok(exists $result->{twilio});

# can create notifier
$result = parliamentPostToken("/parliament/api/notifier?arkimeRegressionUser=parliamentAdminP", '{"name":"Slack","type":"slack","fields":[{"name":"slackWebhookUrl","required":true,"type":"secret","description":"Incoming Webhooks are a simple way to post messages from external sources into Slack.","value":"https://hooks.slack.com/services/asdf"}],"alerts":{"esRed":true,"esDown":true,"esDropped":true,"outOfDate":true,"noPackets":true}}', $parliamentAdminToken);
ok($result->{success});
eq_or_diff($result->{notifier}->{name}, "Slack");
my $id = $result->{notifier}->{id};

# can update notifier
$result = parliamentPutToken("/parliament/api/notifier/$id?arkimeRegressionUser=parliamentAdminP", '{"name":"Slack","type":"slack","fields":[{"name":"slackWebhookUrl","required":true,"type":"secret","description":"Incoming Webhooks are a simple way to post messages from external sources into Slack.","value":"https://hooks.slack.com/services/asdfasdf"}],"alerts":{"esRed":true,"esDown":true,"esDropped":true,"outOfDate":true,"noPackets":true}}', $parliamentAdminToken);
ok($result->{success});
eq_or_diff($result->{notifier}->{fields}->[0]->{value}, "https://hooks.slack.com/services/asdfasdf");

# can issue notification
$result = parliamentPostToken("/parliament/api/notifier/$id/test?arkimeRegressionUser=parliamentAdminP", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Successfully issued alert using the Slack notifier.", "success": true}'));

# can delete notifier
$result = parliamentDeleteToken("/parliament/api/notifier/$id?arkimeRegressionUser=parliamentAdminP", $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Deleted notifier successfully", "success": true}'));

# Create group no title
$result = parliamentPostToken("/parliament/api/groups?arkimeRegressionUser=parliamentAdminP", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":false,"text":"A group must have a title"}'));

# Bad title
$result = parliamentPostToken("/parliament/api/groups?arkimeRegressionUser=parliamentAdminP", '{"title": 1}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":false,"text":"A group must have a title"}'));

# Bad description
$result = parliamentPostToken("/parliament/api/groups?arkimeRegressionUser=parliamentAdminP", '{"title": "title", "description": 1}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":false,"text":"A group must have a string description."}'));

# Create group
$result = parliamentPostToken("/parliament/api/groups?arkimeRegressionUser=parliamentAdminP", '{"title": "the title"}', $parliamentAdminToken);
my $firstGroupId = $result->{group}->{id};
eq_or_diff($result, from_json(qq({"success":true,"text":"Successfully added new group.", "group": {"clusters": [], "id": "$firstGroupId", "title": "the title"}})));

# Create second group
$result = parliamentPostToken("/parliament/api/groups?arkimeRegressionUser=parliamentAdminP", '{"title": "the second title", "description": "description for 2"}', $parliamentAdminToken);
my $secondGroupId = $result->{group}->{id};
eq_or_diff($result, from_json(qq({"success":true,"text":"Successfully added new group.", "group": {"clusters": [], "title": "the second title", "id" : "$secondGroupId", "description": "description for 2"}})));

# Get parliament
$result = parliamentGetToken("/parliament/api/parliament?arkimeRegressionUser=parliamentAdminP", $parliamentAdminToken);
delete $result->{settings};
eq_or_diff($result, from_json(qq({"groups": [{"clusters": [], "id": "$firstGroupId", "title": "the title"}, {"clusters": [], "description": "description for 2", "id": "$secondGroupId", "title": "the second title"}], "name": "parliamenttest"})));

# Update second group bad title
$result = parliamentPutToken("/parliament/api/groups/$secondGroupId?arkimeRegressionUser=parliamentAdminP", '{"title": 1, "description": "UP description for 2"}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":false,"text":"A group must have a title."}'));

# Update second group bad description
$result = parliamentPutToken("/parliament/api/groups/$secondGroupId?arkimeRegressionUser=parliamentAdminP", '{"title": "UP the second title", "description": 1}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":false,"text":"A group must have a string description."}'));

# Update second group
$result = parliamentPutToken("/parliament/api/groups/$secondGroupId?arkimeRegressionUser=parliamentAdminP", '{"title": "UP the second title", "description": "UP description for 2"}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":true,"text":"Successfully updated the group."}'));

# Restore defaults
$result = parliamentPutToken("/parliament/api/settings/restoreDefaults?arkimeRegressionUser=parliamentAdminP", '{}', $parliamentAdminToken);
delete $result->{settings};
eq_or_diff($result, from_json('{"success":true, "text":"Successfully restored default settings."}'));

# Get parliament
$result = parliamentGetToken("/parliament/api/parliament?arkimeRegressionUser=parliamentAdminP", $parliamentAdminToken);
delete $result->{settings};
eq_or_diff($result, from_json('{"groups": [{"clusters": [], "id": "' . $firstGroupId . '", "title": "the title"}, {"clusters": [], "description": "UP description for 2", "id":  "' . $secondGroupId . '", "title": "UP the second title"}], "name": "parliamenttest"}'));

# Delete second group
$result = parliamentDeleteToken("/parliament/api/groups/$secondGroupId?arkimeRegressionUser=parliamentAdminP", $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":true,"text":"Successfully removed group."}'));

# Get parliament after delete
$result = parliamentGetToken("/parliament/api/parliament?arkimeRegressionUser=parliamentAdminP", $parliamentAdminToken);
delete $result->{settings};
eq_or_diff($result, from_json('{"groups": [{"clusters": [], "id": "' . $firstGroupId . '", "title": "the title"}], "name": "parliamenttest"}'));

# Add cluster requires url
$result = parliamentPostToken("/parliament/api/groups/$firstGroupId/clusters?arkimeRegressionUser=parliamentAdminP", '{"title": "cluster 1"}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":false,"text":"A cluster must have a url."}'));

# Add cluster
$result = parliamentPostToken("/parliament/api/groups/$firstGroupId/clusters?arkimeRegressionUser=parliamentAdminP", '{"title": "cluster 1", "url": "super/fancy/url"}', $parliamentAdminToken);
my $firstClusterId = $result->{cluster}->{id};
ok ($result->{success});

# Update cluster
$result = parliamentPutToken("/parliament/api/groups/$firstGroupId/clusters/$firstClusterId?arkimeRegressionUser=parliamentAdminP", '{"title": "cluster 1a", "url": "http://localhost:8123"}', $parliamentAdminToken);
ok ($result->{success});

# Stats
parliamentGet("/regressionTests/updateParliament");
$result = parliamentGetToken("/parliament/api/parliament/stats", $parliamentAdminToken);
my @k = keys %{$result->{results}};
is (scalar @k, 1);
my $result = $result->{results}->{$k[0]};
is ($result->{title}, "cluster 1a");
is ($result->{id}, $k[0]);
ok (exists $result->{deltaBPS});
ok (exists $result->{dataNodes});

# Delete cluster
$result = parliamentDeleteToken("/parliament/api/groups/$firstGroupId/clusters/$firstClusterId?arkimeRegressionUser=parliamentAdminP", $parliamentAdminToken);
ok ($result->{success});

# Delete first group
$result = parliamentDeleteToken("/parliament/api/groups/$firstGroupId?arkimeRegressionUser=parliamentAdminP", $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Successfully removed group.", "success": true}'));

# delete the added users
viewerGet("/regressionTests/deleteAllUsers");

# delete the parliament?
esDelete("/tests_parliament/_doc/parliamenttest");
