use Test::More tests => 57;
use Cwd;
use URI::Escape;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $token = getTokenCookie();
my $notAdminToken = getTokenCookie('sac-notadmin');

clearIndex("tests_notifiers");

# add users for sharing tests
  viewerPostToken("/api/user", '{"userId": "sac-notadmin", "userName": "notadmin", "enabled":true, "password":"password", "roles":["arkimeUser"]}', $token);
  viewerPostToken("/api/user", '{"userId": "sac-user2", "userName": "user2", "enabled":true, "password":"password", "roles":["arkimeUser"]}', $token);

# notifier types
  my $notifierTypes = viewerGetToken("/api/notifiertypes", $token);
  ok(exists $notifierTypes->{email}, "email notifier exists");
  ok(exists $notifierTypes->{slack}, "slack notifier exists");
  ok(exists $notifierTypes->{twilio}, "twilio notifier exists");

# empty notifiers
  my $notifiers = viewerGetToken("/api/notifiers", $token);
  is (@{$notifiers}, 0, "Empty notifiers");

# create notifier required items
  my $json = viewerPostToken("/api/notifier", '{}', $token);
  is($json->{text}, "Missing a notifier name", "notifier name required");
  $json = viewerPostToken("/api/notifier", '{"name":"test1"}', $token);
  is($json->{text}, "Missing notifier type", "notifier type required");
  $json = viewerPostToken("/api/notifier", '{"name":"test1","type":"slack"}', $token);
  is($json->{text}, "Missing notifier fields", "notifier fields required");
  $json = viewerPostToken("/api/notifier", '{"name":"test1","type":"slack","fields":"badfields"}', $token);
  is($json->{text}, "Notifier fields must be an array", "notifier fields must be an array");
  $json = viewerPostToken("/api/notifier", '{"name":"<>","type":"slack","fields":[]}', $token);
  is($json->{text}, "Notifier name empty");
  $json = viewerPostToken("/api/notifier", '{"name":"test1","type":"foo", "fields": ["foo"]}', $token);
  is($json->{text}, "Unknown notifier type");

# create notifier requires token and admin access
  $json = viewerPost("/api/notifier", '{}');
  is($json->{text}, "Missing token", "create notifier requires token");
  $json = viewerPostToken("/api/notifier?arkimeRegressionUser=sac-notadmin", '{}', $notAdminToken);
  is($json->{text}, "You do not have permission to access this resource", "create notifier requires admin");

# create notifier needs valid notifier type
  $json = viewerPostToken("/api/notifier", '{"name":"test1","type":"unknown","fields":[]}', $token);
  is($json->{text}, "Unknown notifier type", "invalid notifier type");

# create notifier needs valid alerts
  $json = viewerPostToken("/api/notifier", '{"name":"test1","type":"slack","alerts":"badstring","fields":[{"slackWebhookUrl":{"value":"test1url"}}]}', $token);
  is($json->{text}, "Alerts must be an object", "Bad alerts type");
  $json = viewerPostToken("/api/notifier", '{"name":"test1","type":"slack","alerts":{"esDown":"badstring"},"fields":[{"slackWebhookUrl":{"value":"test1url"}}]}', $token);
  is($json->{text}, "Alert must be true or false", "Bad alerts type");

# create notifier needs valid on state
  $json = viewerPostToken("/api/notifier", '{"name":"test1","type":"slack","on":"badstring","fields":[{"slackWebhookUrl":{"value":"test1url"}}]}', $token);
  is($json->{text}, "Notifier on state must be true or false", "Bad on type");

# create notifier
  $json = viewerPostToken("/api/notifier", '{"name":"test1","type":"slack","fields":[{"slackWebhookUrl":{"value":"test1url"}}]}', $token);
  ok($json->{success}, "notifier create success");
  my $id1 = $json->{notifier}->{id};

# create notifier sanitizes notifier name
  $json = viewerPostToken("/api/notifier", '{"name":"test2`~!@#$%^&*+[]{}(),.<>?","type":"slack","fields":[{"slackWebhookUrl":{"value":"test1url"}}]}', $token);
  is($json->{notifier}->{name}, "test2", "notifier name sanitization");
  my $id2 = $json->{notifier}->{id};

# created/user fields should be set when creating a notifier
  is($json->{notifier}->{user}, "anonymous", "user field set");
  ok(exists $json->{notifier}->{created}, "created field was set");

# update notifier requires admin access
  $json = viewerPutToken("/api/notifier/$id1?arkimeRegressionUser=sac-notadmin", '{}', $notAdminToken);
  is($json->{text}, "You do not have permission to access this resource", "update notifier requires admin");

# update notifier needs valid id
  $json = viewerPutToken("/api/notifier/badid", '{"name":"hi","fields":[],"type":"slack"}', $token);
  is($json->{text}, "Fetching notifier to update failed", "update notifier needs valid id");

# update notifier required fields
  $json = viewerPutToken("/api/notifier/$id1", '{}', $token);
  is($json->{text}, "Missing a notifier name", "notifier name required");
  $json = viewerPutToken("/api/notifier/$id1", '{"name":"test1a"}', $token);
  is($json->{text}, "Missing notifier type", "notifier type required");
  $json = viewerPutToken("/api/notifier/$id1", '{"name":"test1a","type":"slack"}', $token);
  is($json->{text}, "Missing notifier fields", "notifier fields required");
  $json = viewerPutToken("/api/notifier/$id1", '{"name":"test1a","type":"slack","fields":"badfields"}', $token);
  is($json->{text}, "Notifier fields must be an array", "notifier fields must be an array");
  $json = viewerPutToken("/api/notifier/$id1", '{"name":"<>","type":"slack","fields":[]}', $token);
  is($json->{text}, "Notifier name empty");
  $json = viewerPutToken("/api/notifier/$id1", '{"name":"test1","type":"foo", "fields": ["foo"]}', $token);
  is($json->{text}, "Unknown notifier type");

# update notifier needs valid notifier type
  $json = viewerPutToken("/api/notifier/$id1", '{"name":"test1a","type":"unknown","fields":[]}', $token);
  is($json->{text}, "Unknown notifier type", "invalid notifier type");

# update notifier
  $json = viewerPutToken("/api/notifier/$id1", '{"name":"test1a","type":"slack","fields":[{"slackWebhookUrl":{"value":"test1aurl"}}]}', $token);
  ok($json->{success}, "notifier update success");
  is($json->{notifier}->{name}, "test1a", "notifier name update");
  is($json->{notifier}->{fields}[0]->{slackWebhookUrl}->{value}, "test1aurl", "notifier field value update");

# updated field should be set when updating a notifier
  ok(exists $json->{notifier}->{updated}, "updated field was set");

# can share with a users and returns invalid users
  $json = viewerPostToken("/api/notifier", '{"name":"test3","users":"sac-notadmin,asdf","type":"slack","fields":[{"slackWebhookUrl":{"value":"testurl"}}]}', $token);
  ok($json->{success}, "notifier create success");
  is($json->{notifier}->{users}, "sac-notadmin", "users set");
  is($json->{invalidUsers}->[0], "asdf", "correct invalid users");
  my $id3 = $json->{notifier}->{id};

# user can see shared notifier only but non admin no fields
  $notifiers = viewerGetToken("/api/notifiers?arkimeRegressionUser=sac-notadmin", $notAdminToken);
  is (@{$notifiers}, 1, "Single notifier shared with sac-notadmin user");
  ok(exists $notifiers->[0], "notifier update");
  ok(!exists $notifiers->[0]->{fields}, "fields shouldn't exist for non admin");

# can share with a role
  $json = viewerPostToken("/api/notifier", '{"name":"test4","roles":["parliamentUser"],"type":"slack","fields":[{"slackWebhookUrl":{"value":"testurl"}}]}', $token);
  ok($json->{success}, "notifier create success");
  is($json->{notifier}->{roles}->[0], "parliamentUser", "roles set");
  my $id4 = $json->{notifier}->{id};

# sac-notadmin user cannot see id4 notifier
  $notifiers = viewerGetToken("/api/notifiers?arkimeRegressionUser=sac-notadmin", $notAdminToken);
  is (@{$notifiers}, 1, "Still single notifier shared with sac-notadmin user");

# can update shared users and returns invalid users
  $json = viewerPutToken("/api/notifier/$id3", '{"name":"test3","users":"sac-notadmin,sac-user2,fdsa","type":"slack","fields":[{"slackWebhookUrl":{"value":"testurl"}}]}', $token);
  ok($json->{success}, "notifier update success");
  is($json->{notifier}->{users}, "sac-notadmin,sac-user2", "notifier users update");
  is($json->{invalidUsers}->[0], "fdsa", "correct invalid users on update");

# can update roles
  $json = viewerPutToken("/api/notifier/$id4", '{"name":"test4","roles":["arkimeUser","parliamentUser"],"type":"slack","fields":[{"slackWebhookUrl":{"value":"testurl"}}]}', $token);
  ok($json->{success}, "notifier update success");
  is($json->{notifier}->{roles}->[0], "arkimeUser", "roles updated");
  is($json->{notifier}->{roles}->[1], "parliamentUser", "roles updated");

# sac-notadmin user can now see see id4 notifier
  $notifiers = viewerGetToken("/api/notifiers?arkimeRegressionUser=sac-notadmin", $notAdminToken);
  is (@{$notifiers}, 2, "2 notifiers shared with sac-notadmin user (one by user sharing and one by role sharing)");
  is (${notifiers}->[0]->{name}, "test3", 'can see notifier shared by users');
  is (${notifiers}->[1]->{name}, "test4", 'can see notifier shared by roles');

# cleanup
  $json = viewerDeleteToken("/api/notifier/badid", $token);
  is($json->{text}, "Fetching notifier to delete failed", "notifier delete needs valid id");
  $json = viewerDeleteToken("/api/notifier/$id1", $token);
  ok($json->{success}, "notifier delete success");
  $json = viewerDeleteToken("/api/notifier/$id2", $token);
  ok($json->{success}, "notifier delete success");
  $json = viewerDeleteToken("/api/notifier/$id3", $token);
  ok($json->{success}, "notifier delete success");
  $json = viewerDeleteToken("/api/notifier/$id4", $token);
  ok($json->{success}, "notifier delete success");
  esGet("/_refresh");
  $notifiers = viewerGetToken("/api/notifiers", $token);
  is (@{$notifiers}, 0, "Removed notifiers");
  # remove added users
  viewerDeleteToken("/api/user/sac-notadmin", $token);
  viewerDeleteToken("/api/user/sac-user2", $token);
