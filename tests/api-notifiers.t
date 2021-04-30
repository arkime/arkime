use Test::More tests => 32;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $token = getTokenCookie();
my $notAdminToken = getTokenCookie('notadmin');

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
  is($json->{text}, "Missing a unique notifier name", "notifier name required");
  $json = viewerPostToken("/notifiers", '{"name":"test1"}', $token);
  is($json->{text}, "Missing notifier type", "notifier type required");
  $json = viewerPostToken("/api/notifier", '{"name":"test1","type":"slack"}', $token);
  is($json->{text}, "Missing notifier fields", "notifier fields required");
  $json = viewerPostToken("/notifiers", '{"name":"test1","type":"slack","fields":"badfields"}', $token);
  is($json->{text}, "Notifier fields must be an array", "notifier fields must be an array");

# create notifier requires token and admin access
  $json = viewerPost("/notifiers", '{}');
  is($json->{text}, "Missing token", "create notifier requires token");
  $json = viewerPostToken("/notifiers?molochRegressionUser=notadmin", '{}', $notAdminToken);
  is($json->{text}, "You do not have permission to access this resource", "create notifier requires admin");

# create notifier needs valid notifier type
  $json = viewerPostToken("/api/notifier", '{"name":"test1","type":"unknown","fields":[]}', $token);
  is($json->{text}, "Unknown notifier type", "invalid notifier type");

# create notifier
  $json = viewerPostToken("/notifiers", '{"name":"test1","type":"slack","fields":[{"slackWebhookUrl":{"value":"test1url"}}]}', $token);
  ok($json->{success}, "notifier create success");

# create notifier requires unique notifier name
  $json = viewerPostToken("/notifiers", '{"name":"test1","type":"slack","fields":[{"slackWebhookUrl":{"value":"test1url"}}]}', $token);
  is($json->{text}, "Notifier already exists", "notifier must have a unique name");

# create notifier sanitizes notifier name
  $json = viewerPostToken("/api/notifier", '{"name":"test2`~!@#$%^&*+[]{}(),.<>?","type":"slack","fields":[{"slackWebhookUrl":{"value":"test1url"}}]}', $token);
  is($json->{notifier}->{name}, "test2", "notifier name sanitization");

# created/user fields should be set when creating a notifier
  is($json->{notifier}->{user}, "anonymous", "user field set");
  ok(exists $json->{notifier}->{created}, "created field was set");

# update notifier requires admin access
  $json = viewerPutToken("/api/notifier/test1?molochRegressionUser=notadmin", '{}', $notAdminToken);
  is($json->{text}, "You do not have permission to access this resource", "update notifier requires admin");

# update notifier needs valid name
  $json = viewerPutToken("/notifiers/badname", '{}', $token);
  is($json->{text}, "Cannot find notifer to udpate", "update notifier needs valid name");

# update notifier required fields
  $json = viewerPutToken("/api/notifier/test1", '{}', $token);
  is($json->{text}, "Missing a unique notifier name", "notifier name required");
  $json = viewerPutToken("/notifiers/test1", '{"name":"test1a"}', $token);
  is($json->{text}, "Missing notifier type", "notifier type required");
  $json = viewerPutToken("/api/notifier/test1", '{"name":"test1a","type":"slack"}', $token);
  is($json->{text}, "Missing notifier fields", "notifier fields required");
  $json = viewerPutToken("/notifiers/test1", '{"name":"test1a","type":"slack","fields":"badfields"}', $token);
  is($json->{text}, "Notifier fields must be an array", "notifier fields must be an array");

# update notifier needs valid notifier type
  $json = viewerPutToken("/notifiers/test1", '{"name":"test1a","type":"unknown","fields":[]}', $token);
  is($json->{text}, "Unknown notifier type", "invalid notifier type");

# update notifier
  $json = viewerPutToken("/notifiers/test1", '{"name":"test1a","type":"slack","fields":[{"slackWebhookUrl":{"value":"test1aurl"}}]}', $token);
  ok($json->{success}, "notifier update success");
  is($json->{notifier}->{name}, "test1a", "notifier name update");
  is($json->{notifier}->{fields}[0]->{slackWebhookUrl}->{value}, "test1aurl", "notifier field value update");

# updated field should be set when updating a notifier
  ok(exists $json->{notifier}->{updated}, "updated field was set");

# non admin no fields
  $notifiers = viewerGetToken("/notifiers?molochRegressionUser=notadmin", $notAdminToken);
  ok(exists $notifiers->[0], "notifier update");
  ok(!exists $notifiers->[0]->{fields}, "fields shouldn't exist for non admin");

# cleanup
  $json = viewerDeleteToken("/api/notifier/test1a", $token);
  ok($json->{success}, "notifier delete success");
  $json = viewerDeleteToken("/notifiers/test2", $token);
  ok($json->{success}, "notifier delete success");
  esGet("/_refresh");
  $notifiers = viewerGetToken("/notifiers", $token);
  is (@{$notifiers}, 0, "Removed notifiers");

# remove shared user that gets added when creating notifiers
  viewerPostToken("/user/delete", "userId=_moloch_shared", $token);
