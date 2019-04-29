use Test::More tests => 37;
use Cwd;
use URI::Escape;
use MolochTest;
use Data::Dumper;
use JSON;
use Test::Differences;
use strict;

my $result;

my $version = 3;


# Get parliament, empty
$result = parliamentGet("/parliament/api/parliament");
eq_or_diff($result, from_json('{"groups": [], "version": ' . $version . '}'));


# Set first password
$result = parliamentPut("/parliament/api/auth/update", '{"newPassword": "test"}');
ok(exists $result->{token});
delete $result->{token};
eq_or_diff($result, from_json('{"success":true,"text":"Here\'s your new token!"}'));

# Try and change without current password
$result = parliamentPut("/parliament/api/auth/update", '{"newPassword": "test2"}');
eq_or_diff($result, from_json('{"success":false,"text":"You must provide your current password"}'));

# Try and change wrong current password
$result = parliamentPut("/parliament/api/auth/update", '{"newPassword": "test2", "currentPassword": "wrong"}');
eq_or_diff($result, from_json('{"success":false,"text":"Authentication failed."}'));

# Change password right
$result = parliamentPut("/parliament/api/auth/update", '{"newPassword": "test2", "currentPassword": "test"}');
my $token = $result->{token};
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

# Create group
$result = parliamentPost("/parliament/api/groups", '{"token": "' . $token . '", "title": "the title"}');
eq_or_diff($result, from_json('{"success":true,"text":"Successfully added new group.", "group": {"clusters": [], "id": 0, "title": "the title"}}'));

# Get parliament no token
$result = parliamentGet("/parliament/api/parliament");
eq_or_diff($result, from_json('{"groups": [{"clusters": [], "id": 0, "title": "the title"}], "version": ' . $version .'}'));

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
eq_or_diff($result, from_json('{"groups": [{"clusters": [], "id": 0, "title": "the title"}, {"clusters": [], "description": "description for 2", "id": 1, "title": "the second title"}], "version": ' . $version .'}'));


# Update second group no token
$result = parliamentPut("/parliament/api/groups/1", '{"title": "UP the second title", "description": "UP description for 2"}');
eq_or_diff($result, from_json('{"tokenError":true,"success":false,"text":"Permission Denied: No token provided."}'));

# Update second group bad token
$result = parliamentPut("/parliament/api/groups/1", '{"token": "token", "title": "UP the second title", "description": "UP description for 2"}');
eq_or_diff($result, from_json('{"tokenError":true,"success":false,"text":"Permission Denied: Failed to authenticate token. Try logging in again."}'));

# Update second group
$result = parliamentPut("/parliament/api/groups/1", '{"token": "' . $token . '", "title": "UP the second title", "description": "UP description for 2"}');
eq_or_diff($result, from_json('{"success":true,"text":"Successfully updated the requested group."}'));

# Get parliament
$result = parliamentGet("/parliament/api/parliament");
eq_or_diff($result, from_json('{"groups": [{"clusters": [], "id": 0, "title": "the title"}, {"clusters": [], "description": "UP description for 2", "id": 1, "title": "UP the second title"}], "version": ' . $version .'}'));

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
eq_or_diff($result, from_json('{"groups": [{"clusters": [], "id": 0, "title": "the title"}], "version": ' . $version .'}'));

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
