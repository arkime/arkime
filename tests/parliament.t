use Test::More tests => 9;
use Cwd;
use URI::Escape;
use MolochTest;
use Data::Dumper;
use JSON;
use Test::Differences;
use strict;

my $result;

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


#$result = from_json($MolochTest::userAgent->put("http://localhost:8008/parliament/api/auth/update", Content => (newPassword => "test"))->content);
#eq_or_diff($result, from_json('{"success":false,"text":"You must provide a new password"}'), "view: 1 item");
