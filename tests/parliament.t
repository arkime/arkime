use Test::More tests => 90;
use Cwd;
use URI::Escape;
use MolochTest;
use Data::Dumper;
use JSON;
use Test::Differences;
use strict;

my $result;

my $version = 5;

my $es = "-o 'elasticsearch=$MolochTest::elasticsearch' -o 'usersElasticsearch=$MolochTest::elasticsearch' $ENV{INSECURE}";
# create user without parliament role
system("cd ../viewer ; node addUser.js $es -c ../tests/config.test.ini -n testuser arkimeUserP arkimeUserP arkimeUserP --roles 'arkimeUser' ");
# create user with parliament role
system("cd ../viewer ; node addUser.js $es -c ../tests/config.test.ini -n testuser parliamentUserP parliamentUserP parliamentUserP --roles 'parliamentUser' ");
# create user with parliament admin role
system("cd ../viewer ; node addUser.js $es -c ../tests/config.test.ini -n testuser parliamentAdminP parliamentAdminP parliamentAdminP --roles 'parliamentAdmin' ");

# authenticate non parliament user
$MolochTest::userAgent->credentials( "$MolochTest::host:8008", 'Moloch', 'arkimeUserP', 'arkimeUserP' );
my $arkimeUserToken = getParliamentTokenCookie('arkimeUserP');

# non parliament user can view parliament - empty
$result = parliamentGetToken("/parliament/api/parliament", $arkimeUserToken);
eq_or_diff($result, from_json('{"authMode": "digest", "groups": [], "version": ' . $version . '}'));

# non parliament user can view issues
$result = parliamentGetToken("/parliament/api/issues", $arkimeUserToken);
ok(exists $result->{issues});

# non parliament user cannot update issues
$result = parliamentPutToken("/parliament/api/acknowledgeIssues?molochRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament user"}'));
$result = parliamentPutToken("/parliament/api/ignoreIssues?molochRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament user"}'));
$result = parliamentPutToken("/parliament/api/removeIgnoreIssues?molochRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament user"}'));
$result = parliamentPutToken("/parliament/api/groups/0/clusters/0/removeIssue?molochRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament user"}'));
$result = parliamentPutToken("/parliament/api/issues/removeAllAcknowledgedIssues?molochRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament user"}'));
$result = parliamentPutToken("/parliament/api/removeSelectedAcknowledgedIssues?molochRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament user"}'));

# non parliamet user cannot access/udpate settings/parliament
$result = parliamentGetToken("/parliament/api/settings?molochRegressionUser=arkimeUserP", $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/settings?molochRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/settings/restoreDefaults?molochRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentGetToken("/parliament/api/notifierTypes?molochRegressionUser=arkimeUserP", $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/notifiers/test?molochRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken("/parliament/api/notifiers?molochRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentDeleteToken("/parliament/api/notifiers/test?molochRegressionUser=arkimeUserP", $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken("/parliament/api/testAlert?molochRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/parliament?molochRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken("/parliament/api/groups?molochRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentDeleteToken("/parliament/api/groups/0?molochRegressionUser=arkimeUserP", $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/groups/0?molochRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken("/parliament/api/groups/0/clusters?molochRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentDeleteToken("/parliament/api/groups/0/clusters/0?molochRegressionUser=arkimeUserP", $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/groups/0/clusters/0?molochRegressionUser=arkimeUserP", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));

# authenticate parliament user
$MolochTest::userAgent->credentials( "$MolochTest::host:8008", 'Moloch', 'parliamentUserP', 'parliamentUserP' );
my $parliamentUserToken = getParliamentTokenCookie('parliamentUserP');

# parliament user can view parliament
$result = parliamentGetToken("/parliament/api/parliament?molochRegressionUser=parliamentUserP", $parliamentUserToken);
eq_or_diff($result, from_json('{"authMode": "digest", "groups": [], "version": ' . $version . '}'));

# parliament user can view issues
$result = parliamentGetToken("/parliament/api/issues?molochRegressionUser=parliamentUserP", $parliamentUserToken);
ok(exists $result->{issues});

# parliament user can access update issues endpoints
$result = parliamentPutToken("/parliament/api/acknowledgeIssues?molochRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue(s) to acknowledge.", "success": false}'));
$result = parliamentPutToken("/parliament/api/ignoreIssues?molochRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue(s) to ignore.", "success": false}'));
$result = parliamentPutToken("/parliament/api/removeIgnoreIssues?molochRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue(s) to unignore.", "success": false}'));
$result = parliamentPutToken("/parliament/api/groups/0/clusters/0/removeIssue?molochRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue type to remove.", "success": false}'));
$result = parliamentPutToken("/parliament/api/issues/removeAllAcknowledgedIssues?molochRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"text": "There are no acknowledged issues to remove.", "success": false}'));
$result = parliamentPutToken("/parliament/api/removeSelectedAcknowledgedIssues?molochRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"text": "Must specify the acknowledged issue(s) to remove.", "success": false}'));

# # parliament user cannot access/udpate settings/parliament
$result = parliamentGetToken("/parliament/api/settings?molochRegressionUser=parliamentUserP", $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/settings?molochRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/settings/restoreDefaults?molochRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentGetToken("/parliament/api/notifierTypes?molochRegressionUser=parliamentUserP", $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/notifiers/test?molochRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken("/parliament/api/notifiers?molochRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentDeleteToken("/parliament/api/notifiers/test?molochRegressionUser=parliamentUserP", $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken("/parliament/api/testAlert?molochRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/parliament?molochRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken("/parliament/api/groups?molochRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentDeleteToken("/parliament/api/groups/0?molochRegressionUser=parliamentUserP", $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/groups/0?molochRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken("/parliament/api/groups/0/clusters?molochRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentDeleteToken("/parliament/api/groups/0/clusters/0?molochRegressionUser=parliamentUserP", $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken("/parliament/api/groups/0/clusters/0?molochRegressionUser=parliamentUserP", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "text": "Permission Denied: Not a Parliament admin"}'));

# authenticate parliament admin
$MolochTest::userAgent->credentials( "$MolochTest::host:8008", 'Moloch', 'parliamentAdminP', 'parliamentAdminP' );
my $parliamentAdminToken = getParliamentTokenCookie('parliamentAdminP');

# parliament admin can view parliament
$result = parliamentGetToken("/parliament/api/parliament?molochRegressionUser=parliamentAdminP", $parliamentAdminToken);
eq_or_diff($result, from_json('{"authMode": "digest", "groups": [], "version": ' . $version . '}'));

# parliament admin can view issues
$result = parliamentGetToken("/parliament/api/issues?molochRegressionUser=parliamentAdminP", $parliamentAdminToken);
ok(exists $result->{issues});

# parliament admin can access update issues endpoints
$result = parliamentPutToken("/parliament/api/acknowledgeIssues?molochRegressionUser=parliamentAdminP", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue(s) to acknowledge.", "success": false}'));
$result = parliamentPutToken("/parliament/api/ignoreIssues?molochRegressionUser=parliamentAdminP", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue(s) to ignore.", "success": false}'));
$result = parliamentPutToken("/parliament/api/removeIgnoreIssues?molochRegressionUser=parliamentAdminP", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue(s) to unignore.", "success": false}'));
$result = parliamentPutToken("/parliament/api/groups/0/clusters/0/removeIssue?molochRegressionUser=parliamentAdminP", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue type to remove.", "success": false}'));
$result = parliamentPutToken("/parliament/api/issues/removeAllAcknowledgedIssues?molochRegressionUser=parliamentAdminP", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "There are no acknowledged issues to remove.", "success": false}'));
$result = parliamentPutToken("/parliament/api/removeSelectedAcknowledgedIssues?molochRegressionUser=parliamentAdminP", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Must specify the acknowledged issue(s) to remove.", "success": false}'));

# parliament admin can access/update settings/parliament
$result = parliamentGetToken("/parliament/api/settings?molochRegressionUser=parliamentAdminP", $parliamentAdminToken);
ok(exists $result->{commonAuth});
ok(exists $result->{notifiers});
ok(exists $result->{general});
ok(exists $result->{general}->{hostname});
ok(exists $result->{general}->{outOfDate});
ok(exists $result->{general}->{noPackets});
ok(exists $result->{general}->{esQueryTimeout});
ok(exists $result->{general}->{removeIssuesAfter});
ok(exists $result->{general}->{removeAcknowledgedAfter});

$result = parliamentPutToken("/parliament/api/settings?molochRegressionUser=parliamentAdminP", '{"settings": { "general": { "noPacketsLength": 100 } } }', $parliamentAdminToken);
ok($result->{success});

$result = parliamentGetToken("/parliament/api/settings?molochRegressionUser=parliamentAdminP", $parliamentAdminToken);
eq_or_diff($result->{general}->{noPacketsLength}, 100);

$result = parliamentGetToken("/parliament/api/notifierTypes?molochRegressionUser=parliamentAdminP", $parliamentAdminToken);
ok(exists $result->{slack});
ok(exists $result->{email});
ok(exists $result->{twilio});

$result = parliamentPostToken("/parliament/api/notifiers?molochRegressionUser=parliamentAdminP", '{"notifier":{"name":"Slack","type":"slack","fields":{"slackWebhookUrl":{"name":"slackWebhookUrl","required":true,"type":"secret","description":"Incoming Webhooks are a simple way to post messages from external sources into Slack.","value":"https://hooks.slack.com/services/asdf"}},"alerts":{"esRed":true,"esDown":true,"esDropped":true,"outOfDate":true,"noPackets":true}}}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Successfully added Slack notifier.", "success": true, "name": "Slack"}'));
$result = parliamentPutToken("/parliament/api/notifiers/Slack?molochRegressionUser=parliamentAdminP", '{"key":"Slack","notifier":{"name":"Slack","type":"slack","fields":{"slackWebhookUrl":{"name":"slackWebhookUrl","required":true,"type":"secret","description":"Incoming Webhooks are a simple way to post messages from external sources into Slack.","value":"https://hooks.slack.com/services/asdfasdf"}},"alerts":{"esRed":true,"esDown":true,"esDropped":true,"outOfDate":true,"noPackets":true}}}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Successfully updated Slack notifier.", "success": true, "newKey": "Slack"}'));
$result = parliamentPostToken("/parliament/api/testAlert?molochRegressionUser=parliamentAdminP", '{"notifier":"Slack"}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Successfully issued alert using the Slack notifier.", "success": true}'));
$result = parliamentDeleteToken("/parliament/api/notifiers/Slack?molochRegressionUser=parliamentAdminP", $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Successfully removed Slack notifier.", "success": true}'));

# $result = parliamentPostToken("/parliament/api/groups?molochRegressionUser=parliamentAdminP", '{"title":"Group 1","description":""}', $parliamentAdminToken);
# eq_or_diff($result, from_json('{"text": "Successfully added new group.", "success": true, "group": {"clusters":[],"id":0,"title":"Group 1"}}'));
# $result = parliamentPostToken("/parliament/api/groups/0/clusters?molochRegressionUser=parliamentAdminP", '{"title":"Cluster 1","url":"localhost:8123"}', $parliamentAdminToken);
# delete $result->{cluster}->{healthError};
# delete $result->{cluster}->{statsError};
# eq_or_diff($result, from_json('{"text": "Successfully added the requested cluster.", "success": true, "cluster": { "title":"Cluster 1", "url":"localhost:8123", "id":0}}'));
# $result = parliamentPutToken("/parliament/api/groups/0?molochRegressionUser=parliamentAdminP", '{"title":"Group 1a"}', $parliamentAdminToken);
# eq_or_diff($result, from_json('{"text": "Successfully updated the requested group.", "success": true}'));
# $result = parliamentPutToken("/parliament/api/groups/0/clusters/0?molochRegressionUser=parliamentAdminP", '{"url":"localhost:8123","title":"Cluster 1a"}', $parliamentAdminToken);
# eq_or_diff($result, from_json('{"text": "Successfully updated the requested cluster.", "success": true}'));
# $result = parliamentDeleteToken("/parliament/api/groups/0/clusters/0?molochRegressionUser=parliamentAdminP", $parliamentAdminToken);
# eq_or_diff($result, from_json('{"text": "Successfully removed the requested cluster.", "success": true}'));
# $result = parliamentDeleteToken("/parliament/api/groups/0?molochRegressionUser=parliamentAdminP", $parliamentAdminToken);
# eq_or_diff($result, from_json('{"text": "Successfully removed the requested group.", "success": true}'));

# Create group no title
$result = parliamentPostToken("/parliament/api/groups?molochRegressionUser=parliamentAdminP", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":false,"text":"A group must have a title"}'));

# Bad title
$result = parliamentPostToken("/parliament/api/groups?molochRegressionUser=parliamentAdminP", '{"title": 1}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":false,"text":"A group must have a title"}'));

# Bad description
$result = parliamentPostToken("/parliament/api/groups?molochRegressionUser=parliamentAdminP", '{"title": "title", "description": 1}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":false,"text":"A group must have a string description."}'));

# Create group
$result = parliamentPostToken("/parliament/api/groups?molochRegressionUser=parliamentAdminP", '{"title": "the title"}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":true,"text":"Successfully added new group.", "group": {"clusters": [], "id": 0, "title": "the title"}}'));

# Create second group
$result = parliamentPostToken("/parliament/api/groups?molochRegressionUser=parliamentAdminP", '{"title": "the second title", "description": "description for 2"}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":true,"text":"Successfully added new group.", "group": {"clusters": [], "id": 1, "title": "the second title", "description": "description for 2"}}'));

# Get parliament
$result = parliamentGetToken("/parliament/api/parliament?molochRegressionUser=parliamentAdminP", $parliamentAdminToken);
eq_or_diff($result, from_json('{"authMode": "digest", "groups": [{"clusters": [], "id": 0, "title": "the title"}, {"clusters": [], "description": "description for 2", "id": 1, "title": "the second title"}], "version": ' . $version .'}'));

# Update second group bad title
$result = parliamentPutToken("/parliament/api/groups/1?molochRegressionUser=parliamentAdminP", '{"title": 1, "description": "UP description for 2"}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":false,"text":"A group must have a title."}'));

# Update second group bad description
$result = parliamentPutToken("/parliament/api/groups/1?molochRegressionUser=parliamentAdminP", '{"title": "UP the second title", "description": 1}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":false,"text":"A group must have a string description."}'));

# Update second group
$result = parliamentPutToken("/parliament/api/groups/1?molochRegressionUser=parliamentAdminP", '{"title": "UP the second title", "description": "UP description for 2"}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":true,"text":"Successfully updated the requested group."}'));

# Restore defaults error
$result = parliamentPutToken("/parliament/api/settings/restoreDefaults?molochRegressionUser=parliamentAdminP", '{"type": "foo"}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":false,"text":"type must be general or all"}'));

# Get parliament
$result = parliamentGetToken("/parliament/api/parliament?molochRegressionUser=parliamentAdminP", $parliamentAdminToken);
eq_or_diff($result, from_json('{"authMode": "digest", "groups": [{"clusters": [], "id": 0, "title": "the title"}, {"clusters": [], "description": "UP description for 2", "id": 1, "title": "UP the second title"}], "version": ' . $version .'}'));

# Delete second group
$result = parliamentDeleteToken("/parliament/api/groups/1?molochRegressionUser=parliamentAdminP", $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":true,"text":"Successfully removed the requested group."}'));

# Get parliament after delete
$result = parliamentGetToken("/parliament/api/parliament?molochRegressionUser=parliamentAdminP", $parliamentAdminToken);
eq_or_diff($result, from_json('{"authMode": "digest", "groups": [{"clusters": [], "id": 0, "title": "the title"}], "version": ' . $version .'}'));

# Add cluster requires url
$result = parliamentPostToken("/parliament/api/groups/0/clusters?molochRegressionUser=parliamentAdminP", '{"title": "cluster 1"}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":false,"text":"A cluster must have a url."}'));

# Add cluster
$result = parliamentPostToken("/parliament/api/groups/0/clusters?molochRegressionUser=parliamentAdminP", '{"title": "cluster 1", "url": "super/fancy/url"}', $parliamentAdminToken);
ok ($result->{success});

# Update cluster
$result = parliamentPutToken("/parliament/api/groups/0/clusters/0?molochRegressionUser=parliamentAdminP", '{"title": "cluster 1a", "url": "super/fancy/urla"}', $parliamentAdminToken);
ok ($result->{success});

# Delete cluster
$result = parliamentDeleteToken("/parliament/api/groups/0/clusters/0?molochRegressionUser=parliamentAdminP", $parliamentAdminToken);
ok ($result->{success});

# Delete first group
$result = parliamentDeleteToken("/parliament/api/groups/0?molochRegressionUser=parliamentAdminP", $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Successfully removed the requested group.", "success": true}'));

viewerGet("/regressionTests/deleteAllUsers");
