use Test::More tests => 133;
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

# Check appversion
$result = parliamentGet("/api/appversion");
is($result->{app}, "parliament", "parliament appversion app field");

# non parliament user can view parliament - empty
$result = parliamentGetToken("/parliament/api/parliament", $arkimeUserToken);
eq_or_diff($result, from_json('{"groups": [], "name": "parliamenttest", "settings": { "general": { "esQueryTimeout": 5, "lowDiskSpace": 4, "lowDiskSpaceES": 15, "lowDiskSpaceESType": "percentage", "lowDiskSpaceType": "percentage", "noPackets": 0, "noPacketsLength": 10, "outOfDate": 30, "removeAcknowledgedAfter": 15, "removeIssuesAfter": 60 } } }'));

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

# non parliament user cannot access/update settings/parliament
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
eq_or_diff($result, from_json('{"groups": [], "name": "parliamenttest", "settings": { "general": { "esQueryTimeout": 5, "lowDiskSpace": 4, "lowDiskSpaceES": 15, "lowDiskSpaceESType": "percentage", "lowDiskSpaceType": "percentage", "noPackets": 0, "noPacketsLength": 10, "outOfDate": 30, "removeAcknowledgedAfter": 15, "removeIssuesAfter": 60 } } }'));

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

# parliament user cannot update settings/parliament
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

# ignoreIssues requires a finite numeric ms (rejects non-numeric ms before it corrupts ignoreUntil)
$result = parliamentPutToken("/parliament/api/ignoreIssues?arkimeRegressionUser=parliamentAdminP", '{"issues": [{"clusterId": "abc", "type": "esRed"}], "ms": "not-a-number"}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"text": "ms must be a finite number.", "success": false}'));

# parliament admin can access/update settings/parliament
$result = parliamentGetToken("/parliament/api/parliament?arkimeRegressionUser=parliamentAdminP", $parliamentAdminToken);
ok(exists $result->{settings}->{general});
ok(exists $result->{settings}->{general}->{outOfDate});
ok(exists $result->{settings}->{general}->{noPackets});
ok(exists $result->{settings}->{general}->{esQueryTimeout});
ok(exists $result->{settings}->{general}->{removeIssuesAfter});
ok(exists $result->{settings}->{general}->{removeAcknowledgedAfter});
ok(exists $result->{settings}->{general}->{lowDiskSpace});
ok(exists $result->{settings}->{general}->{lowDiskSpaceType});
ok(exists $result->{settings}->{general}->{lowDiskSpaceES});
ok(exists $result->{settings}->{general}->{lowDiskSpaceESType});

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

# unknown settings keys are ignored, not written into settings.general (mass-assignment guard)
$result = parliamentPutToken("/parliament/api/settings?arkimeRegressionUser=parliamentAdminP", '{"settings": { "general": { "noPacketsLength": 100, "evilKey": 99 } } }', $parliamentAdminToken);
ok($result->{success});
$result = parliamentGetToken("/parliament/api/parliament?arkimeRegressionUser=parliamentAdminP", $parliamentAdminToken);
eq_or_diff($result->{settings}->{general}->{noPacketsLength}, 100);
ok(!exists $result->{settings}->{general}->{evilKey});

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
eq_or_diff($result, from_json('{"success":false,"text":"A cluster must have a url that starts with http or / (and not //)."}'));

# Add cluster url must start with http or /
$result = parliamentPostToken("/parliament/api/groups/$firstGroupId/clusters?arkimeRegressionUser=parliamentAdminP", '{"title": "cluster 1", "url": "super/fancy/url"}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":false,"text":"A cluster must have a url that starts with http or / (and not //)."}'));

# Add cluster url must start with http or / - ftp
$result = parliamentPostToken("/parliament/api/groups/$firstGroupId/clusters?arkimeRegressionUser=parliamentAdminP", '{"title": "cluster 1", "url": "ftp://example.com"}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":false,"text":"A cluster must have a url that starts with http or / (and not //)."}'));

# Add cluster url must not be protocol-relative (// would render as an off-site link)
$result = parliamentPostToken("/parliament/api/groups/$firstGroupId/clusters?arkimeRegressionUser=parliamentAdminP", '{"title": "cluster 1", "url": "//evil.com"}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":false,"text":"A cluster must have a url that starts with http or / (and not //)."}'));

# Add cluster with valid http url
$result = parliamentPostToken("/parliament/api/groups/$firstGroupId/clusters?arkimeRegressionUser=parliamentAdminP", '{"title": "cluster 1", "url": "http://super/fancy/url"}', $parliamentAdminToken);
my $firstClusterId = $result->{cluster}->{id};
ok ($result->{success});

# Update cluster url must start with http or /
$result = parliamentPutToken("/parliament/api/groups/$firstGroupId/clusters/$firstClusterId?arkimeRegressionUser=parliamentAdminP", '{"title": "cluster 1a", "url": "gopher://evil"}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":false,"text":"A cluster must have a url that starts with http or / (and not //)."}'));

# Update cluster localUrl must start with http or /
$result = parliamentPutToken("/parliament/api/groups/$firstGroupId/clusters/$firstClusterId?arkimeRegressionUser=parliamentAdminP", '{"title": "cluster 1a", "url": "http://localhost:8123", "localUrl": "file:///etc/passwd"}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":false,"text":"A cluster localUrl must start with http or / (and not //)."}'));

# Update cluster localUrl must not be protocol-relative
$result = parliamentPutToken("/parliament/api/groups/$firstGroupId/clusters/$firstClusterId?arkimeRegressionUser=parliamentAdminP", '{"title": "cluster 1a", "url": "http://localhost:8123", "localUrl": "//evil.com"}', $parliamentAdminToken);
eq_or_diff($result, from_json('{"success":false,"text":"A cluster localUrl must start with http or / (and not //)."}'));

# Update cluster with valid url
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

# ---- /api/issues query param type confusion guards ----
# seed persistent issues with two unreachable clusters so the filter/sort
# code paths actually run over real issues
$result = parliamentPostToken("/parliament/api/groups?arkimeRegressionUser=parliamentAdminP", '{"title": "issuetest"}', $parliamentAdminToken);
my $issueGroupId = $result->{group}->{id};
ok($result->{success});

$result = parliamentPostToken("/parliament/api/groups/$issueGroupId/clusters?arkimeRegressionUser=parliamentAdminP", '{"title": "down1", "url": "http://127.0.0.1:1"}', $parliamentAdminToken);
ok($result->{success});
my $down1Id = $result->{cluster}->{id};
$result = parliamentPostToken("/parliament/api/groups/$issueGroupId/clusters?arkimeRegressionUser=parliamentAdminP", '{"title": "down2", "url": "http://127.0.0.1:1"}', $parliamentAdminToken);
ok($result->{success});

# two update cycles so the issues are no longer provisional and show up in /api/issues
parliamentGet("/regressionTests/updateParliament");
parliamentGet("/regressionTests/updateParliament");

# sanity: there are issues to filter/sort over
$result = parliamentGetToken("/parliament/api/issues?arkimeRegressionUser=parliamentAdminP", $parliamentAdminToken);
ok(scalar @{$result->{issues}} >= 2);

# the poll saves its issues to the issues file once per cycle (written async, so allow a moment)
my $issuesOnDisk = [];
for (my $i = 0; $i < 10; $i++) {
    if (open(my $fh, '<', 'parliament.dev.issues.json')) {
        local $/;
        my $data = eval { from_json(<$fh>) };
        close($fh);
        $issuesOnDisk = $data if ref $data eq 'ARRAY';
    }
    last if grep { ($_->{clusterId} // '') eq $down1Id } @$issuesOnDisk;
    sleep(1);
}
ok((grep { ($_->{clusterId} // '') eq $down1Id && $_->{type} eq 'esDown' } @$issuesOnDisk), "poll issues are written to the issues file");

# filter passed as an array is ignored instead of throwing (500) on .toLowerCase()
$result = parliamentGetToken("/parliament/api/issues?filter=a&filter=b&arkimeRegressionUser=parliamentAdminP", $parliamentAdminToken);
ok(exists $result->{issues});

# sort with an inherited prototype key is ignored instead of throwing (500) in the comparator
# (toString instead of constructor, which auth's ppChecker middleware already rejects with a 403)
$result = parliamentGetToken("/parliament/api/issues?sort=toString&order=asc&arkimeRegressionUser=parliamentAdminP", $parliamentAdminToken);
ok(exists $result->{issues});

# clean up the issue test group
$result = parliamentDeleteToken("/parliament/api/groups/$issueGroupId?arkimeRegressionUser=parliamentAdminP", $parliamentAdminToken);
ok($result->{success});

# delete the added users
viewerGet("/regressionTests/deleteAllUsers");

# delete the parliament?
esDelete("/tests_parliament/_doc/parliamenttest");

# commaString used to be a lookahead regex that was quadratic on long digit
# strings, so a polled cluster could hang parliament with a huge stat value.
# Check every copy formats like the old regex and stays fast.

my $runner = "/tmp/arkime-comma-string-$$.mjs";
END { unlink $runner if $runner; }

open(my $rf, '>', $runner) or die "can't write $runner";
print $rf <<'JS';
import path from 'path';
import { createRequire } from 'module';
import { pathToFileURL } from 'url';

const root = path.resolve(process.argv[2]);
const require = createRequire(import.meta.url);
const impls = {
  arkimeUtil: require(path.join(root, 'common/arkimeUtil')).commaString,
  vueFilters: (await import(pathToFileURL(path.join(root, 'common/vueapp/vueFilters.js')))).commaString,
  cont3xtVueFilters: (await import(pathToFileURL(path.join(root, 'cont3xt/common/vueapp/vueFilters.js')))).commaString
};

function oldCommaString (input) {
  if (isNaN(input)) { return 0; }
  const parts = input.toString().split('.');
  parts[0] = parts[0].replace(/\B(?=(\d{3})+(?!\d))/g, ',');
  return parts.join('.');
}

const cases = [0, 1, 12, 123, 1234, 12345, 123456, 1234567, -1234567, 1234.5, '1234567.891', '0.00', 'abc', 1e21];
for (let i = 0; i < 1000; i++) {
  cases.push(Math.floor(Math.random() * 10 ** (1 + i % 15)) * (i % 2 ? -1 : 1));
}
const expected = cases.map(oldCommaString);

const big = '9'.repeat(1000000);
const out = {};
for (const [name, fn] of Object.entries(impls)) {
  const got = cases.map(fn);
  const mismatch = got.findIndex((v, i) => v !== expected[i]);
  const start = Date.now();
  const res = fn(big);
  out[name] = {
    mismatch: mismatch === -1 ? null : { input: cases[mismatch], got: got[mismatch], expected: expected[mismatch] },
    bigMs: Date.now() - start,
    bigOk: res.length === 1333333 && /^9(,999)+$/.test(res),
    nullOk: fn(null) === 0
  };
}
console.log('RESULT ' + JSON.stringify(out));
JS
close($rf);

my $output = `node $runner .. 2>&1`;
is($? >> 8, 0, "runner exits cleanly") or diag($output);
my ($json) = $output =~ /^RESULT (.*)$/m;
my $commaResult = from_json($json // '{}');

foreach my $name (qw(arkimeUtil vueFilters cont3xtVueFilters)) {
    my $r = $commaResult->{$name};
    ok(defined $r, "$name loaded");
    is($r->{mismatch}, undef, "$name matches old regex output") or diag(to_json($r->{mismatch}));
    ok($r->{bigOk}, "$name formats a 1M digit string");
    cmp_ok($r->{bigMs}, '<', 1000, "$name handles a 1M digit string in linear time");
    ok($r->{nullOk}, "$name returns 0 for null instead of throwing");
}

ok($output !~ /Error/, "no errors") or diag($output);
is(scalar(keys %$commaResult), 3, "all implementations tested");
