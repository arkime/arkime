use Test::More tests => 115;
use Cwd;
use URI::Escape;
use MolochTest;
use Data::Dumper;
use JSON;
use Test::Differences;
use strict;

my $result;

my $version = 3;

# OLD JWT AUTH ################################################################
# Get parliament, empty
$result = parliamentGet("/parliament/api/parliament");
eq_or_diff($result, from_json('{"authMode": false, "groups": [], "version": ' . $version . '}'));

# requires auth setup code
$result = parliamentPut("/parliament/api/auth/update", '{"newPassword": "test"}');
eq_or_diff($result, from_json('{"success":false,"text":"Not authorized, check log file"}'));

# Set first password
$result = parliamentPut("/parliament/api/auth/update", '{"newPassword": "test", "authSetupCode": "0000000000"}');
my $token = $result->{token};
ok(exists $result->{token});
delete $result->{token};
eq_or_diff($result, from_json('{"success":true,"text":"Here\'s your new token!"}'));

# Try and change without current password
$result = parliamentPut("/parliament/api/auth/update", '{"newPassword": "test2", "token": "' . $token . '"}');
eq_or_diff($result, from_json('{"success":false,"text":"You must provide your current password"}'));

# Try and change wrong current password
$result = parliamentPut("/parliament/api/auth/update", '{"newPassword": "test2", "currentPassword": "wrong", "token": "' . $token . '"}');
eq_or_diff($result, from_json('{"success":false,"text":"Authentication failed."}'));

# Change password right
$result = parliamentPut("/parliament/api/auth/update", '{"newPassword": "test2", "currentPassword": "test", "token": "' . $token . '"}');
$token = $result->{token};
ok(exists $result->{token});
delete $result->{token};
eq_or_diff($result, from_json('{"success":true,"text":"Here\'s your new token!"}'));

# Create group no title no token
$result = parliamentPost("/parliament/api/groups", '{}');
eq_or_diff($result, from_json('{"tokenError":true,"success":false,"text":"Permission Denied: No token provided."}'));

# Create group no title wrong token
$result = parliamentPost("/parliament/api/groups", '{"token": "token"}');
eq_or_diff($result, from_json('{"tokenError":true,"success":false,"text":"Permission Denied: Failed to authenticate token. Try logging in again."}'));

# Create group no title
$result = parliamentPost("/parliament/api/groups", '{"token": "' . $token . '"}');
eq_or_diff($result, from_json('{"success":false,"text":"A group must have a title"}'));

# Bad title
$result = parliamentPost("/parliament/api/groups", '{"token": "' . $token . '", "title": 1}');
eq_or_diff($result, from_json('{"success":false,"text":"A group must have a title"}'));

# Bad description
$result = parliamentPost("/parliament/api/groups", '{"token": "' . $token . '", "title": "title", "description": 1}');
eq_or_diff($result, from_json('{"success":false,"text":"A group must have a string description."}'));

# Create group
$result = parliamentPost("/parliament/api/groups", '{"token": "' . $token . '", "title": "the title"}');
eq_or_diff($result, from_json('{"success":true,"text":"Successfully added new group.", "group": {"clusters": [], "id": 0, "title": "the title"}}'));

# Get parliament no token
$result = parliamentGet("/parliament/api/parliament");
eq_or_diff($result, from_json('{"authMode": false, "groups": [{"clusters": [], "id": 0, "title": "the title"}], "version": ' . $version .'}'));

# Get settings no token
$result = parliamentGet("/parliament/api/settings");
eq_or_diff($result, from_json('{"tokenError":true,"success":false,"text":"Permission Denied: No token provided."}'));

# Get settings bad token
$result = parliamentGetToken("/parliament/api/settings", "token");
eq_or_diff($result, from_json('{"tokenError":true,"success":false,"text":"Permission Denied: Failed to authenticate token. Try logging in again."}'));

# Get settings good token
$result = parliamentGetToken("/parliament/api/settings", $token);
ok (exists $result->{notifiers});
ok (exists $result->{general});
ok (exists $result->{general}->{hostname});
ok (exists $result->{general}->{outOfDate});
ok (exists $result->{general}->{noPackets});
ok (exists $result->{general}->{esQueryTimeout});
ok (exists $result->{general}->{removeIssuesAfter});
ok (exists $result->{general}->{removeAcknowledgedAfter});

# Create second group
$result = parliamentPost("/parliament/api/groups", '{"token": "' . $token . '", "title": "the second title", "description": "description for 2"}');
eq_or_diff($result, from_json('{"success":true,"text":"Successfully added new group.", "group": {"clusters": [], "id": 1, "title": "the second title", "description": "description for 2"}}'));

# Get parliament
$result = parliamentGet("/parliament/api/parliament");
eq_or_diff($result, from_json('{"authMode": false, "groups": [{"clusters": [], "id": 0, "title": "the title"}, {"clusters": [], "description": "description for 2", "id": 1, "title": "the second title"}], "version": ' . $version .'}'));

# Update second group no token
$result = parliamentPut("/parliament/api/groups/1", '{"title": "UP the second title", "description": "UP description for 2"}');
eq_or_diff($result, from_json('{"tokenError":true,"success":false,"text":"Permission Denied: No token provided."}'));

# Update second group bad token
$result = parliamentPut("/parliament/api/groups/1", '{"token": "token", "title": "UP the second title", "description": "UP description for 2"}');
eq_or_diff($result, from_json('{"tokenError":true,"success":false,"text":"Permission Denied: Failed to authenticate token. Try logging in again."}'));

# Update second group bad title
$result = parliamentPut("/parliament/api/groups/1", '{"token": "' . $token . '", "title": 1, "description": "UP description for 2"}');
eq_or_diff($result, from_json('{"success":false,"text":"A group must have a title."}'));

# Update second group bad description
$result = parliamentPut("/parliament/api/groups/1", '{"token": "' . $token . '", "title": "UP the second title", "description": 1}');
eq_or_diff($result, from_json('{"success":false,"text":"A group must have a string description."}'));

# Update second group
$result = parliamentPut("/parliament/api/groups/1", '{"token": "' . $token . '", "title": "UP the second title", "description": "UP description for 2"}');
eq_or_diff($result, from_json('{"success":true,"text":"Successfully updated the requested group."}'));

# Restore defaults error
$result = parliamentPut("/parliament/api/settings/restoreDefaults", '{"token": "' . $token . '", "type": "foo"}');
eq_or_diff($result, from_json('{"success":false,"text":"type must be general or all"}'));

# Get parliament
$result = parliamentGet("/parliament/api/parliament");
eq_or_diff($result, from_json('{"authMode": false, "groups": [{"clusters": [], "id": 0, "title": "the title"}, {"clusters": [], "description": "UP description for 2", "id": 1, "title": "UP the second title"}], "version": ' . $version .'}'));

# Delete second group no token
$result = parliamentDelete("/parliament/api/groups/1");
eq_or_diff($result, from_json('{"tokenError":true,"success":false,"text":"Permission Denied: No token provided."}'));

# Delete second group bad token
$result = parliamentDeleteToken("/parliament/api/groups/1", "token");
eq_or_diff($result, from_json('{"tokenError":true,"success":false,"text":"Permission Denied: Failed to authenticate token. Try logging in again."}'));

# Delete second group
$result = parliamentDeleteToken("/parliament/api/groups/1", $token);
eq_or_diff($result, from_json('{"success":true,"text":"Successfully removed the requested group."}'));

# Get parliament after delete
$result = parliamentGet("/parliament/api/parliament");
eq_or_diff($result, from_json('{"authMode": false, "groups": [{"clusters": [], "id": 0, "title": "the title"}], "version": ' . $version .'}'));

# Add cluster requires url
$result = parliamentPost("/parliament/api/groups/0/clusters", '{"token": "' . $token . '", "title": "cluster 1"}');
eq_or_diff($result, from_json('{"success":false,"text":"A cluster must have a url."}'));

# Add cluster
$result = parliamentPost("/parliament/api/groups/0/clusters", '{"token": "' . $token . '", "title": "cluster 1", "url": "super/fancy/url"}');
ok ($result->{success});

# Update cluster
$result = parliamentPut("/parliament/api/groups/0/clusters/0", '{"token": "' . $token . '", "title": "cluster 1a", "url": "super/fancy/urla"}');
ok ($result->{success});

# Delete cluster no token
$result = parliamentDelete("/parliament/api/groups/0/clusters/0");
eq_or_diff($result, from_json('{"tokenError":true,"success":false,"text":"Permission Denied: No token provided."}'));

# Delete cluster
$result = parliamentDeleteToken("/parliament/api/groups/0/clusters/0", $token);
ok ($result->{success});


# DIGEST AUTH ################################################################
my $es = "-o 'elasticsearch=$MolochTest::elasticsearch' -o 'usersElasticsearch=$MolochTest::elasticsearch' $ENV{INSECURE}";
# create user without parliament role
system("cd ../viewer ; node addUser.js $es -c ../tests/config.test.ini -n testuser arkimeUserP arkimeUserP arkimeUserP --roles 'arkimeUser' ");
# create user with parliament role
system("cd ../viewer ; node addUser.js $es -c ../tests/config.test.ini -n testuser parliamentUserP parliamentUserP parliamentUserP --roles 'parliamentUser' ");
# create user with parliament admin role
system("cd ../viewer ; node addUser.js $es -c ../tests/config.test.ini -n testuser parliamentAdminP parliamentAdminP parliamentAdminP --roles 'parliamentAdmin' ");

# authenticate non parliament user
$MolochTest::userAgent->credentials( "$MolochTest::host:8009", 'Moloch', 'arkimeUserP', 'arkimeUserP' );
my $arkimeUserToken = getParliamentTokenCookie('arkimeUserP');

# non parliament user can view parliament
$result = parliamentGetToken2("/parliament/api/parliament", $arkimeUserToken);
eq_or_diff($result, from_json('{"authMode": "digest", "groups": [], "version": ' . $version . '}'));

# non parliament user can view issues
$result = parliamentGetToken2("/parliament/api/issues", $arkimeUserToken);
ok(exists $result->{issues});

# non parliament user cannot update issues
$result = parliamentPutToken2("/parliament/api/acknowledgeIssues", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament user"}'));
$result = parliamentPutToken2("/parliament/api/ignoreIssues", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament user"}'));
$result = parliamentPutToken2("/parliament/api/removeIgnoreIssues", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament user"}'));
$result = parliamentPutToken2("/parliament/api/groups/0/clusters/0/removeIssue", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament user"}'));
$result = parliamentPutToken2("/parliament/api/issues/removeAllAcknowledgedIssues", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament user"}'));
$result = parliamentPutToken2("/parliament/api/removeSelectedAcknowledgedIssues", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament user"}'));

# non parliamet user cannot access/udpate settings/parliament
$result = parliamentGetToken2("/parliament/api/settings", $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken2("/parliament/api/settings", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken2("/parliament/api/settings/restoreDefaults", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentGetToken2("/parliament/api/notifierTypes", $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken2("/parliament/api/notifiers/test", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken2("/parliament/api/notifiers", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentDeleteToken2("/parliament/api/notifiers/test", $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken2("/parliament/api/testAlert", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken2("/parliament/api/parliament", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken2("/parliament/api/groups", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentDeleteToken2("/parliament/api/groups/0", $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken2("/parliament/api/groups/0", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken2("/parliament/api/groups/0/clusters", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentDeleteToken2("/parliament/api/groups/0/clusters/0", $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken2("/parliament/api/groups/0/clusters/0", '{}', $arkimeUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));

# authenticate parliament user
$MolochTest::userAgent->credentials( "$MolochTest::host:8009", 'Moloch', 'parliamentUserP', 'parliamentUserP' );
my $parliamentUserToken = getParliamentTokenCookie('parliamentUserP');

# parliament user can view parliament
$result = parliamentGetToken2("/parliament/api/parliament", $parliamentUserToken);
eq_or_diff($result, from_json('{"authMode": "digest", "groups": [], "version": ' . $version . '}'));

# parliament user can view issues
$result = parliamentGetToken2("/parliament/api/issues", $parliamentUserToken);
ok(exists $result->{issues});

# parliament user can access update issues endpoints
$result = parliamentPutToken2("/parliament/api/acknowledgeIssues", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue(s) to acknowledge.", "success": false}'));
$result = parliamentPutToken2("/parliament/api/ignoreIssues", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue(s) to ignore.", "success": false}'));
$result = parliamentPutToken2("/parliament/api/removeIgnoreIssues", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue(s) to unignore.", "success": false}'));
$result = parliamentPutToken2("/parliament/api/groups/0/clusters/0/removeIssue", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue type to remove.", "success": false}'));
$result = parliamentPutToken2("/parliament/api/issues/removeAllAcknowledgedIssues", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"text": "There are no acknowledged issues to remove.", "success": false}'));
$result = parliamentPutToken2("/parliament/api/removeSelectedAcknowledgedIssues", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"text": "Must specify the acknowledged issue(s) to remove.", "success": false}'));

# parliament user cannot access/udpate settings/parliament
$result = parliamentGetToken2("/parliament/api/settings", $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken2("/parliament/api/settings", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken2("/parliament/api/settings/restoreDefaults", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentGetToken2("/parliament/api/notifierTypes", $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken2("/parliament/api/notifiers/test", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken2("/parliament/api/notifiers", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentDeleteToken2("/parliament/api/notifiers/test", $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken2("/parliament/api/testAlert", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken2("/parliament/api/parliament", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken2("/parliament/api/groups", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentDeleteToken2("/parliament/api/groups/0", $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken2("/parliament/api/groups/0", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPostToken2("/parliament/api/groups/0/clusters", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentDeleteToken2("/parliament/api/groups/0/clusters/0", $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));
$result = parliamentPutToken2("/parliament/api/groups/0/clusters/0", '{}', $parliamentUserToken);
eq_or_diff($result, from_json('{"success": false, "tokenError": true, "text": "Permission Denied: Not a Parliament admin"}'));

# authenticate parliament admin
$MolochTest::userAgent->credentials( "$MolochTest::host:8009", 'Moloch', 'parliamentAdminP', 'parliamentAdminP' );
my $parliamentAdminToken = getParliamentTokenCookie('parliamentAdminP');

# parliament admin can view parliament
$result = parliamentGetToken2("/parliament/api/parliament", $parliamentAdminToken);
eq_or_diff($result, from_json('{"authMode": "digest", "groups": [], "version": ' . $version . '}'));

# parliament admin can view issues
$result = parliamentGetToken2("/parliament/api/issues", $parliamentAdminToken);
ok(exists $result->{issues});

# parliament admin can access update issues endpoints
$result = parliamentPutToken2("/parliament/api/acknowledgeIssues", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue(s) to acknowledge.", "success": false}'));
$result = parliamentPutToken2("/parliament/api/ignoreIssues", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue(s) to ignore.", "success": false}'));
$result = parliamentPutToken2("/parliament/api/removeIgnoreIssues", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue(s) to unignore.", "success": false}'));
$result = parliamentPutToken2("/parliament/api/groups/0/clusters/0/removeIssue", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Must specify the issue type to remove.", "success": false}'));
$result = parliamentPutToken2("/parliament/api/issues/removeAllAcknowledgedIssues", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "There are no acknowledged issues to remove.", "success": false}'));
$result = parliamentPutToken2("/parliament/api/removeSelectedAcknowledgedIssues", '{}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "Must specify the acknowledged issue(s) to remove.", "success": false}'));

# parliament admin can access/update settings/parliament
$result = parliamentGetToken2("/parliament/api/settings", $parliamentAdminToken);
ok(exists $result->{commonAuth});
ok(exists $result->{notifiers});
ok(exists $result->{general});

$result = parliamentPutToken2("/parliament/api/settings", '{"settings": { "general": { "noPacketsLength": 100 } } }', $parliamentAdminToken);
ok($result->{success});

$result = parliamentGetToken2("/parliament/api/settings", $parliamentAdminToken);
eq_or_diff($result->{general}->{noPacketsLength}, 100);

$result = parliamentGetToken2("/parliament/api/notifierTypes", $parliamentAdminToken);
ok(exists $result->{slack});
ok(exists $result->{email});
ok(exists $result->{twilio});

$result = parliamentPostToken2("/parliament/api/notifiers", '{"notifier":{"name":"Slack","type":"slack","fields":{"slackWebhookUrl":{"name":"slackWebhookUrl","required":true,"type":"secret","description":"Incoming Webhooks are a simple way to post messages from external sources into Slack.","value":"https://hooks.slack.com/services/asdf"}},"alerts":{"esRed":true,"esDown":true,"esDropped":true,"outOfDate":true,"noPackets":true}}}', $parliamentAdminToken);
ok($result->{success});
$result = parliamentPutToken2("/parliament/api/notifiers/Slack", '{"key":"Slack","notifier":{"name":"Slack","type":"slack","fields":{"slackWebhookUrl":{"name":"slackWebhookUrl","required":true,"type":"secret","description":"Incoming Webhooks are a simple way to post messages from external sources into Slack.","value":"https://hooks.slack.com/services/asdfasdf"}},"alerts":{"esRed":true,"esDown":true,"esDropped":true,"outOfDate":true,"noPackets":true}}}', $parliamentAdminToken);
ok($result->{success});
$result = parliamentPostToken2("/parliament/api/testAlert", '{"notifier":"Slack"}', $parliamentAdminToken);
ok($result->{success});
$result = parliamentDeleteToken2("/parliament/api/notifiers/Slack", $parliamentAdminToken);
ok($result->{success});

$result = parliamentPostToken2("/parliament/api/groups", '{"title":"Group 1","description":""}', $parliamentAdminToken);
ok($result->{success});
$result = parliamentPostToken2("/parliament/api/groups/0/clusters", '{"title":"Cluster 1","url":"localhost:8123"}', $parliamentAdminToken);
ok($result->{success});
$result = parliamentPutToken2("/parliament/api/groups/0", '{"title":"Group 1a"}', $parliamentAdminToken);
ok($result->{success});
$result = parliamentPutToken2("/parliament/api/groups/0/clusters/0", '{"url":"localhost:8123","title":"Cluster 1a"}', $parliamentAdminToken);
ok($result->{success});
$result = parliamentDeleteToken2("/parliament/api/groups/0/clusters/0", $parliamentAdminToken);
ok($result->{success});
$result = parliamentDeleteToken2("/parliament/api/groups/0", $parliamentAdminToken);
ok($result->{success});

viewerGet("/regressionTests/deleteAllUsers");
