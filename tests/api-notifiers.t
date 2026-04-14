use Test::More tests => 94;
use Cwd;
use URI::Escape;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $token = getTokenCookie();
my $notAdminToken = getTokenCookie('sac-notadmin');

viewerGet("/regressionTests/deleteAllNotifiers");

# add users for sharing tests
  viewerPostToken("/api/user", '{"userId": "sac-notadmin", "userName": "notadmin", "enabled":true, "password":"password", "roles":["arkimeUser"]}', $token);
  viewerPostToken("/api/user", '{"userId": "sac-user2", "userName": "user2", "enabled":true, "password":"password", "roles":["arkimeUser"]}', $token);

# notifier types
  my $notifierTypes = viewerGetToken("/api/notifiertypes", $token);
  ok(exists $notifierTypes->{email}, "email notifier exists");
  ok(exists $notifierTypes->{slack}, "slack notifier exists");
  ok(exists $notifierTypes->{snmp}, "snmp notifier exists");
  ok(exists $notifierTypes->{syslog}, "syslog notifier exists");
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

# create notifier requires required fields
  $json = viewerPostToken("/api/notifier", '{"name":"test1","type":"slack","fields":[]}', $token);
  is($json->{text}, "Missing a value for slackWebhookUrl", "missing required field caught");
  $json = viewerPostToken("/api/notifier", '{"name":"test1","type":"slack","fields":[{"name":"slackWebhookUrl","value":""}]}', $token);
  is($json->{text}, "Missing a value for slackWebhookUrl", "empty required field caught");

# create notifier needs valid alerts
  $json = viewerPostToken("/api/notifier", '{"name":"test1","type":"slack","alerts":"badstring","fields":[{"name":"slackWebhookUrl","value":"test1url"}]}', $token);
  is($json->{text}, "Alerts must be an object", "Bad alerts type");
  $json = viewerPostToken("/api/notifier", '{"name":"test1","type":"slack","alerts":{"esDown":"badstring"},"fields":[{"name":"slackWebhookUrl","value":"test1url"}]}', $token);
  is($json->{text}, "Alert must be true or false", "Bad alerts type");

# create notifier needs valid on state
  $json = viewerPostToken("/api/notifier", '{"name":"test1","type":"slack","on":"badstring","fields":[{"name":"slackWebhookUrl","value":"test1url"}]}', $token);
  is($json->{text}, "Notifier on state must be true or false", "Bad on type");

# create notifier
  $json = viewerPostToken("/api/notifier", '{"name":"test1","type":"slack","fields":[{"name":"slackWebhookUrl","value":"test1url"}]}', $token);
  ok($json->{success}, "notifier create success");
  my $id1 = $json->{notifier}->{id};

# create notifier sanitizes notifier name
  $json = viewerPostToken("/api/notifier", '{"name":"test2`~!@#$%^&*+[]{}(),.<>?","type":"slack","fields":[{"name":"slackWebhookUrl","value":"test1url"}]}', $token);
  is($json->{notifier}->{name}, "test2", "notifier name sanitization");
  my $id2 = $json->{notifier}->{id};

# created/user fields should be set when creating a notifier
  is($json->{notifier}->{user}, "anonymous", "user field set");
  ok(exists $json->{notifier}->{created}, "created field was set");

# mass assignment - extra fields should not be stored
  $json = viewerPostToken("/api/notifier", '{"name":"testevil","type":"slack","fields":[{"name":"slackWebhookUrl","value":"evilurl"}],"evil":"injected"}', $token);
  ok($json->{success}, "notifier with extra field creates ok");
  my $evilId = $json->{notifier}->{id};
  ok(!exists $json->{notifier}->{evil}, "extra field not in create response");
  $notifiers = viewerGetToken("/api/notifiers", $token);
  my ($evilNotifier) = grep { $_->{id} eq $evilId } @{$notifiers};
  ok(!exists $evilNotifier->{evil}, "extra field not stored in notifier");
  viewerDeleteToken("/api/notifier/$evilId", $token);

# roles validation - create with non-array roles should fail
  $json = viewerPostToken("/api/notifier", '{"name":"badroles","type":"slack","fields":[{"name":"slackWebhookUrl","value":"test"}],"roles":"notanarray"}', $token);
  ok(!$json->{success}, "create notifier with string roles fails");
  is($json->{text}, "Roles field must be an array of strings", "create string roles error message");

# roles validation - create with array of non-strings should fail
  $json = viewerPostToken("/api/notifier", '{"name":"badroles2","type":"slack","fields":[{"name":"slackWebhookUrl","value":"test"}],"roles":[123,456]}', $token);
  ok(!$json->{success}, "create notifier with non-string array roles fails");
  is($json->{text}, "Roles field must be an array of strings", "create non-string array roles error message");

# roles validation - update with non-array roles should fail
  $json = viewerPutToken("/api/notifier/$id1", '{"name":"slack1","type":"slack","fields":[{"name":"slackWebhookUrl","value":"test"}],"roles":{"bad":"object"}}', $token);
  ok(!$json->{success}, "update notifier with object roles fails");
  is($json->{text}, "Roles field must be an array of strings", "update object roles error message");

  $json = viewerPutToken("/api/notifier/$id1?arkimeRegressionUser=sac-notadmin", '{}', $notAdminToken);
  is($json->{text}, "You do not have permission to access this resource", "update notifier requires admin");

# users validation - create with array users should fail
  $json = viewerPostToken("/api/notifier", '{"name":"badusers","type":"slack","fields":[{"name":"slackWebhookUrl","value":"test"}],"users":["arr"]}', $token);
  ok(!$json->{success}, "create notifier with array users fails");
  is($json->{text}, "Users field must be a string", "create array users error message");

# users validation - create with object users should fail
  $json = viewerPostToken("/api/notifier", '{"name":"badusers2","type":"slack","fields":[{"name":"slackWebhookUrl","value":"test"}],"users":{"bad":"obj"}}', $token);
  ok(!$json->{success}, "create notifier with object users fails");
  is($json->{text}, "Users field must be a string", "create object users error message");

# users validation - update with array users should fail
  $json = viewerPutToken("/api/notifier/$id1", '{"name":"slack1","type":"slack","fields":[{"name":"slackWebhookUrl","value":"test"}],"users":["arr"]}', $token);
  ok(!$json->{success}, "update notifier with array users fails");
  is($json->{text}, "Users field must be a string", "update array users error message");

# users validation - update with object users should fail
  $json = viewerPutToken("/api/notifier/$id1", '{"name":"slack1","type":"slack","fields":[{"name":"slackWebhookUrl","value":"test"}],"users":{"bad":"obj"}}', $token);
  ok(!$json->{success}, "update notifier with object users fails");
  is($json->{text}, "Users field must be a string", "update object users error message");

# update notifier needs valid id
  $json = viewerPutToken("/api/notifier/badid", '{"name":"hi","fields":[{"name":"slackWebhookUrl","value":"test"}],"type":"slack"}', $token);
  is($json->{text}, "Notifier not found", "update notifier needs valid id");

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
  $json = viewerPutToken("/api/notifier/$id1", '{"name":"test1a","type":"slack","fields":[{"name":"slackWebhookUrl","value":"test1aurl"}]}', $token);
  ok($json->{success}, "notifier update success");
  is($json->{notifier}->{name}, "test1a", "notifier name update");
  is($json->{notifier}->{fields}[0]->{value}, "test1aurl", "notifier field value update");

# updated field should be set when updating a notifier
  ok(exists $json->{notifier}->{updated}, "updated field was set");

# can share with a users and returns invalid users
  $json = viewerPostToken("/api/notifier", '{"name":"test3","users":"sac-notadmin,asdf","type":"slack","fields":[{"name":"slackWebhookUrl","value":"testurl"}]}', $token);
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
  $json = viewerPostToken("/api/notifier", '{"name":"test4","roles":["parliamentUser"],"type":"slack","fields":[{"name":"slackWebhookUrl","value":"testurl"}]}', $token);
  ok($json->{success}, "notifier create success");
  is($json->{notifier}->{roles}->[0], "parliamentUser", "roles set");
  my $id4 = $json->{notifier}->{id};

# sac-notadmin user cannot see id4 notifier
  $notifiers = viewerGetToken("/api/notifiers?arkimeRegressionUser=sac-notadmin", $notAdminToken);
  is (@{$notifiers}, 1, "Still single notifier shared with sac-notadmin user");

# can update shared users and returns invalid users
  $json = viewerPutToken("/api/notifier/$id3", '{"name":"test3","users":"sac-notadmin,sac-user2,fdsa","type":"slack","fields":[{"name":"slackWebhookUrl","value":"testurl"}]}', $token);
  ok($json->{success}, "notifier update success");
  is($json->{notifier}->{users}, "sac-notadmin,sac-user2", "notifier users update");
  is($json->{invalidUsers}->[0], "fdsa", "correct invalid users on update");

# can update roles
  $json = viewerPutToken("/api/notifier/$id4", '{"name":"test4","roles":["arkimeUser","parliamentUser"],"type":"slack","fields":[{"name":"slackWebhookUrl","value":"testurl"}]}', $token);
  ok($json->{success}, "notifier update success");
  is($json->{notifier}->{roles}->[0], "arkimeUser", "roles updated");
  is($json->{notifier}->{roles}->[1], "parliamentUser", "roles updated");

# sac-notadmin user can now see see id4 notifier
  $notifiers = viewerGetToken("/api/notifiers?arkimeRegressionUser=sac-notadmin", $notAdminToken);
  is (@{$notifiers}, 2, "2 notifiers shared with sac-notadmin user (one by user sharing and one by role sharing)");
  is (${notifiers}->[0]->{name}, "test3", 'can see notifier shared by users');
  is (${notifiers}->[1]->{name}, "test4", 'can see notifier shared by roles');

# syslog notifier - missing required fields
  $json = viewerPostToken("/api/notifier", '{"name":"syslog1","type":"syslog","fields":[]}', $token);
  is($json->{text}, "Missing a value for host", "syslog missing host");
  $json = viewerPostToken("/api/notifier", '{"name":"syslog1","type":"syslog","fields":[{"name":"host","value":"localhost"}]}', $token);
  is($json->{text}, "Missing a value for port", "syslog missing port");
  $json = viewerPostToken("/api/notifier", '{"name":"syslog1","type":"syslog","fields":[{"name":"host","value":"localhost"},{"name":"port","value":"514"}]}', $token);
  is($json->{text}, "Missing a value for protocol", "syslog missing protocol");

# syslog notifier - create success
  $json = viewerPostToken("/api/notifier", '{"name":"syslog1","type":"syslog","fields":[{"name":"host","value":"localhost"},{"name":"port","value":"514"},{"name":"protocol","value":"udp"}]}', $token);
  ok($json->{success}, "syslog notifier create success");
  my $syslogId = $json->{notifier}->{id};
  is($json->{notifier}->{type}, "syslog", "syslog notifier type");

# syslog notifier - update
  $json = viewerPutToken("/api/notifier/$syslogId", '{"name":"syslog1a","type":"syslog","fields":[{"name":"host","value":"syslog.example.com"},{"name":"port","value":"6514"},{"name":"protocol","value":"tls"},{"name":"facility","value":"local1"},{"name":"severity","value":"err"},{"name":"tag","value":"parliament"}]}', $token);
  ok($json->{success}, "syslog notifier update success");
  is($json->{notifier}->{name}, "syslog1a", "syslog notifier name updated");

# cleanup
  $json = viewerDeleteToken("/api/notifier/$syslogId", $token);
  ok($json->{success}, "syslog notifier delete success");

# snmp notifier - missing required fields
  $json = viewerPostToken("/api/notifier", '{"name":"snmp1","type":"snmp","fields":[]}', $token);
  is($json->{text}, "Missing a value for host", "snmp missing host");

# snmp notifier - create success (v2c)
  $json = viewerPostToken("/api/notifier", '{"name":"snmp1","type":"snmp","fields":[{"name":"host","value":"localhost"},{"name":"community","value":"public"}]}', $token);
  ok($json->{success}, "snmp notifier create success");
  my $snmpId = $json->{notifier}->{id};
  is($json->{notifier}->{type}, "snmp", "snmp notifier type");

# snmp notifier - update with v1 fields
  $json = viewerPutToken("/api/notifier/$snmpId", '{"name":"snmp1a","type":"snmp","fields":[{"name":"host","value":"trap.example.com"},{"name":"port","value":"1162"},{"name":"community","value":"private"},{"name":"version","value":"1"},{"name":"trapOid","value":"1.3.6.1.4.1.12345.1"}]}', $token);
  ok($json->{success}, "snmp notifier update success");
  is($json->{notifier}->{name}, "snmp1a", "snmp notifier name updated");

# snmp notifier - update with v3 fields
  $json = viewerPutToken("/api/notifier/$snmpId", '{"name":"snmp1v3","type":"snmp","fields":[{"name":"host","value":"trap.example.com"},{"name":"version","value":"3"},{"name":"v3User","value":"myuser"},{"name":"v3AuthProtocol","value":"sha"},{"name":"v3AuthKey","value":"authpass1"},{"name":"v3PrivProtocol","value":"aes"},{"name":"v3PrivKey","value":"privpass1"}]}', $token);
  ok($json->{success}, "snmp v3 notifier update success");
  is($json->{notifier}->{name}, "snmp1v3", "snmp v3 notifier name updated");

# cleanup
  $json = viewerDeleteToken("/api/notifier/$snmpId", $token);
  ok($json->{success}, "snmp notifier delete success");

# cleanup
  $json = viewerDeleteToken("/api/notifier/badid", $token);
  is($json->{text}, "Notifier not found", "notifier delete needs valid id");
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
