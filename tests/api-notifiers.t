use Test::More tests => 31;
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
  my $notifierTypes = viewerGetToken("/notifierTypes", $token);
  ok(exists $notifierTypes->{email}, "email notifier exists");
  ok(exists $notifierTypes->{slack}, "slack notifier exists");
  ok(exists $notifierTypes->{twilio}, "twilio notifier exists");

# empty notifiers
  my $notifiers = viewerGetToken("/notifiers", $token);
  ok(!keys %{$notifiers}, "Empty notifiers");

# create notifier required items
  my $json = viewerPostToken("/notifiers", '{}', $token);
  is($json->{text}, "Missing notifier", "notifier object required");
  $json = viewerPostToken("/notifiers", '{"notifier":{}}', $token);
  is($json->{text}, "Missing a unique notifier name", "notifier name required");
  $json = viewerPostToken("/notifiers", '{"notifier":{"name":"test1"}}', $token);
  is($json->{text}, "Missing notifier type", "notifier type required");
  $json = viewerPostToken("/notifiers", '{"notifier":{"name":"test1","type":"slack"}}', $token);
  is($json->{text}, "Missing notifier fields", "notifier fields required");
  $json = viewerPostToken("/notifiers", '{"notifier":{"name":"test1","type":"slack","fields":"badfields"}}', $token);
  is($json->{text}, "Notifier fields must be an array", "notifier fields must be an array");

# create notifier requires token and admin access
  $json = viewerPost("/notifiers", '{}');
  is($json->{text}, "Missing token", "create notifier requires token");
  $json = viewerPostToken("/notifiers?molochRegressionUser=notadmin", '{}', $notAdminToken);
  is($json->{text}, "Need admin privelages to create a notifier", "create notifier requires admin");

# create notifier needs valid notifier type
  $json = viewerPostToken("/notifiers", '{"notifier":{"name":"test1","type":"unknown","fields":[]}}', $token);
  is($json->{text}, "Unknown notifier type", "invalid notifier type");

# create notifier
  $json = viewerPostToken("/notifiers", '{"notifier":{"name":"test1","type":"slack","fields":[{"slackWebhookUrl":{"value":"test1url"}}]}}', $token);
  ok($json->{success}, "notifier create success");

# create notifier requires unique notifier name
  $json = viewerPostToken("/notifiers", '{"notifier":{"name":"test1","type":"slack","fields":[{"slackWebhookUrl":{"value":"test1url"}}]}}', $token);
  is($json->{text}, "Notifier already exists", "notifier must have a unique name");

# create notifier sanitizes notifier name
  $json = viewerPostToken("/notifiers", '{"notifier":{"name":"test2`~!@#$%^&*+[]{}(),.<>?","type":"slack","fields":[{"slackWebhookUrl":{"value":"test1url"}}]}}', $token);
  is($json->{name}, "test2", "notifier name sanitization");

# update notifier requires admin access
  $json = viewerPutToken("/notifiers/test1?molochRegressionUser=notadmin", '{}', $notAdminToken);
  is($json->{text}, "Need admin privelages to update a notifier", "update notifier requires admin");

# update notifier needs valid name
  $json = viewerPutToken("/notifiers/badname", '{}', $token);
  is($json->{text}, "Cannot find notifer to udpate", "update notifier needs valid name");

# update notifier required fields
  $json = viewerPutToken("/notifiers/test1", '{}', $token);
  is($json->{text}, "Missing notifier", "notifier object required");
  $json = viewerPutToken("/notifiers/test1", '{"notifier":{}}', $token);
  is($json->{text}, "Missing a unique notifier name", "notifier name required");
  $json = viewerPutToken("/notifiers/test1", '{"notifier":{"name":"test1a"}}', $token);
  is($json->{text}, "Missing notifier type", "notifier type required");
  $json = viewerPutToken("/notifiers/test1", '{"notifier":{"name":"test1a","type":"slack"}}', $token);
  is($json->{text}, "Missing notifier fields", "notifier fields required");
  $json = viewerPutToken("/notifiers/test1", '{"notifier":{"name":"test1a","type":"slack","fields":"badfields"}}', $token);
  is($json->{text}, "Notifier fields must be an array", "notifier fields must be an array");

# update notifier needs valid notifier type
  $json = viewerPutToken("/notifiers/test1", '{"notifier":{"name":"test1a","type":"unknown","fields":[]}}', $token);
  is($json->{text}, "Unknown notifier type", "invalid notifier type");

# update notifier
  $json = viewerPutToken("/notifiers/test1", '{"notifier":{"name":"test1a","type":"slack","fields":[{"slackWebhookUrl":{"value":"test1aurl"}}]}}', $token);
  ok($json->{success}, "notifier update success");
  is($json->{name}, "test1a", "notifier name update");
  $notifiers = viewerGetToken("/notifiers", $token);
  ok(exists $notifiers->{test1a}, "notifier update");
  is($notifiers->{test1a}->{fields}[0]->{slackWebhookUrl}->{value}, "test1aurl", "notifier field value update");

# cleanup
  $json = viewerDeleteToken("/notifiers/test1a", $token);
  ok($json->{success}, "notifier delete success");
  $json = viewerDeleteToken("/notifiers/test2", $token);
  ok($json->{success}, "notifier delete success");
  esGet("/_refresh");
  $notifiers = viewerGetToken("/notifiers", $token);
  ok(!exists $notifiers->{test1a}, "removed test1a notifier");
  ok(!exists $notifiers->{test2}, "removed test2 notifier");
  # remove shared user that gets added when creating notifiers
  viewerPostToken("/user/delete", "userId=_moloch_shared", $token);
