use Test::More tests => 25;
use Cwd;
use URI::Escape;
use MolochTest;
use Data::Dumper;
use JSON;
use Test::Differences;
use strict;

my $result;

# Get parliament, empty
$result = from_json($MolochTest::userAgent->get("http://localhost:8008/parliament/api/parliament")->content);
eq_or_diff($result, from_json('{"groups": []}'));


# Set first password
$result = from_json($MolochTest::userAgent->put("http://localhost:8008/parliament/api/auth/update", Content => '{"newPassword": "test"}', "Content-Type" => "application/json;charset=UTF-8")->content);
ok(exists $result->{token});
delete $result->{token};
eq_or_diff($result, from_json('{"success":true,"text":"Here\'s your new token!"}'));

# Try and change without current password
$result = from_json($MolochTest::userAgent->put("http://localhost:8008/parliament/api/auth/update", Content => '{"newPassword": "test2"}', "Content-Type" => "application/json;charset=UTF-8")->content);
eq_or_diff($result, from_json('{"success":false,"text":"You must provide your current password"}'));

# Try and change wrong current password
$result = from_json($MolochTest::userAgent->put("http://localhost:8008/parliament/api/auth/update", Content => '{"newPassword": "test2", "currentPassword": "wrong"}', "Content-Type" => "application/json;charset=UTF-8")->content);
eq_or_diff($result, from_json('{"success":false,"text":"Authentication failed."}'));

# Change password right
$result = from_json($MolochTest::userAgent->put("http://localhost:8008/parliament/api/auth/update", Content => '{"newPassword": "test2", "currentPassword": "test"}', "Content-Type" => "application/json;charset=UTF-8")->content);
my $token = $result->{token};
ok(exists $result->{token});
delete $result->{token};
eq_or_diff($result, from_json('{"success":true,"text":"Here\'s your new token!"}'));



# Create group no title no token
$result = from_json($MolochTest::userAgent->post("http://localhost:8008/parliament/api/groups", Content => '{}', "Content-Type" => "application/json;charset=UTF-8")->content);
eq_or_diff($result, from_json('{"tokenError":true,"success":false,"text":"Permission Denied: No token provided."}'));

# Create group no title wrong token
$result = from_json($MolochTest::userAgent->post("http://localhost:8008/parliament/api/groups", Content => '{"token": "token"}', "Content-Type" => "application/json;charset=UTF-8")->content);
eq_or_diff($result, from_json('{"tokenError":true,"success":false,"text":"Permission Denied: Failed to authenticate token. Try logging in again."}'));

# Create group no title
$result = from_json($MolochTest::userAgent->post("http://localhost:8008/parliament/api/groups", Content => '{"token": "' . $token . '"}', "Content-Type" => "application/json;charset=UTF-8")->content);
eq_or_diff($result, from_json('{"success":false,"text":"A group must have a title"}'));

# Create group
$result = from_json($MolochTest::userAgent->post("http://localhost:8008/parliament/api/groups", Content => '{"token": "' . $token . '", "title": "the title"}', "Content-Type" => "application/json;charset=UTF-8")->content);
eq_or_diff($result, from_json('{"success":true,"text":"Successfully added new group.", "group": {"clusters": [], "id": 0, "title": "the title"}}'));

# Get parliament no token
$result = from_json($MolochTest::userAgent->get("http://localhost:8008/parliament/api/parliament")->content);
eq_or_diff($result, from_json('{"groups": [{"clusters": [], "id": 0, "title": "the title"}]}'));

# Get settings no token
$result = from_json($MolochTest::userAgent->get("http://localhost:8008/parliament/api/settings")->content);
eq_or_diff($result, from_json('{"tokenError":true,"success":false,"text":"Permission Denied: No token provided."}'));

# Get settings bad token
$result = from_json($MolochTest::userAgent->get("http://localhost:8008/parliament/api/settings", "x-access-token" => "token")->content);
eq_or_diff($result, from_json('{"tokenError":true,"success":false,"text":"Permission Denied: Failed to authenticate token. Try logging in again."}'));

# Get settings good token
$result = from_json($MolochTest::userAgent->get("http://localhost:8008/parliament/api/settings", "x-access-token" => $token)->content);
ok (exists $result->{notifiers});

# Create second group
$result = from_json($MolochTest::userAgent->post("http://localhost:8008/parliament/api/groups", Content => '{"token": "' . $token . '", "title": "the second title", "description": "description for 2"}', "Content-Type" => "application/json;charset=UTF-8")->content);
eq_or_diff($result, from_json('{"success":true,"text":"Successfully added new group.", "group": {"clusters": [], "id": 1, "title": "the second title", "description": "description for 2"}}'));

# Get parliament
$result = from_json($MolochTest::userAgent->get("http://localhost:8008/parliament/api/parliament")->content);
eq_or_diff($result, from_json('{"groups": [{"clusters": [], "id": 0, "title": "the title"}, {"clusters": [], "description": "description for 2", "id": 1, "title": "the second title"}]}'));


# Update second group no token
$result = from_json($MolochTest::userAgent->put("http://localhost:8008/parliament/api/groups/1", Content => '{"title": "UP the second title", "description": "UP description for 2"}', "Content-Type" => "application/json;charset=UTF-8")->content);
eq_or_diff($result, from_json('{"tokenError":true,"success":false,"text":"Permission Denied: No token provided."}'));

# Update second group bad token
$result = from_json($MolochTest::userAgent->put("http://localhost:8008/parliament/api/groups/1", Content => '{"token": "token", "title": "UP the second title", "description": "UP description for 2"}', "Content-Type" => "application/json;charset=UTF-8")->content);
eq_or_diff($result, from_json('{"tokenError":true,"success":false,"text":"Permission Denied: Failed to authenticate token. Try logging in again."}'));

# Update second group
$result = from_json($MolochTest::userAgent->put("http://localhost:8008/parliament/api/groups/1", Content => '{"token": "' . $token . '", "title": "UP the second title", "description": "UP description for 2"}', "Content-Type" => "application/json;charset=UTF-8")->content);
eq_or_diff($result, from_json('{"success":true,"text":"Successfully updated the requested group."}'));

# Get parliament
$result = from_json($MolochTest::userAgent->get("http://localhost:8008/parliament/api/parliament")->content);
eq_or_diff($result, from_json('{"groups": [{"clusters": [], "id": 0, "title": "the title"}, {"clusters": [], "description": "UP description for 2", "id": 1, "title": "UP the second title"}]}'));

# Delete second group no token
$result = from_json($MolochTest::userAgent->delete("http://localhost:8008/parliament/api/groups/1")->content);
eq_or_diff($result, from_json('{"tokenError":true,"success":false,"text":"Permission Denied: No token provided."}'));

# Delete second group bad token
$result = from_json($MolochTest::userAgent->delete("http://localhost:8008/parliament/api/groups/1", "x-access-token" => "token")->content);
eq_or_diff($result, from_json('{"tokenError":true,"success":false,"text":"Permission Denied: Failed to authenticate token. Try logging in again."}'));

# Delete second group
$result = from_json($MolochTest::userAgent->delete("http://localhost:8008/parliament/api/groups/1", "x-access-token" => $token)->content);
eq_or_diff($result, from_json('{"success":true,"text":"Successfully removed the requested group."}'));

# Get parliament after delete
$result = from_json($MolochTest::userAgent->get("http://localhost:8008/parliament/api/parliament")->content);
eq_or_diff($result, from_json('{"groups": [{"clusters": [], "id": 0, "title": "the title"}]}'));
