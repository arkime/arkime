# Test addUser.js and general authentication
use Test::More tests => 104;
use Test::Differences;
use Data::Dumper;
use ArkimeTest;
use JSON;
use strict;

viewerGet("/regressionTests/deleteAllUsers");

esGet("/_refresh");
my $adminToken = getTokenCookie('admin');
my $test6Token = getTokenCookie('test6');
my $test7Token = getTokenCookie('test7');
esGet("/_refresh");


# script exits successfully
my $result = addUser("-n testuser admin admin admin --admin");
eq_or_diff($result, "0", "script exited successfully");

# create a user with each flag
addUser("-n testuser role:role role:role role:role --roles 'superAdmin' ");
addUser("-n testuser test1 test1 test1");
addUser("-n testuser test2 test2 test2 --apionly");
addUser("-n testuser test3 test3 test3 --email");
addUser("-n testuser test4 test4 test4 --expression 'ip.src == 10.0.0.1'");
addUser("-n testuser test5 test5 test5 --remove");
addUser("-n testuser test6 test6 test6 --webauth --roles arkimeUser");
addUser("-n testuser test7 test7 test7 --packetSearch --roles arkimeUser,parliamentUser");
addUser("-n testuser test8 test8 test8 --roles 'parliamentUser' ");
esGet("/_refresh");

# fetch the users
my $users = viewerPostToken("/api/users?arkimeRegressionUser=admin", "", $adminToken);

diag Dumper($users) if ($users->{recordsTotal} != 10);

# validate the flags
eq_or_diff($users->{recordsTotal}, 10, "Should have 11 users");
eq_or_diff($users->{data}->[0]->{roles}, from_json('["superAdmin"]'));
is($users->{data}->[1]->{userId}, 'role:role');
eq_or_diff($users->{data}->[2]->{roles}, from_json('["arkimeUser","cont3xtUser","parliamentUser","wiseUser"]'));
ok(!$users->{data}->[3]->{webEnabled}, "API only");
ok($users->{data}->[4]->{emailSearch}, "Email Search");
eq_or_diff($users->{data}->[5]->{expression}, "ip.src == 10.0.0.1");
ok($users->{data}->[6]->{removeEnabled}, "Remove");
ok($users->{data}->[7]->{headerAuthEnabled}, "Web auth");
ok($users->{data}->[8]->{packetSearch}, "Packet search");
eq_or_diff($users->{data}->[9]->{roles}, from_json('["parliamentUser"]'));

# user should have password
my $response = viewerGet("/regressionTests/getUser/test1");
ok(exists $response->{passStore}, "Users has password");

# --createOnly flag should not overwrite the user if it already exists
my $user8 = $users->{data}->[8];
addUser("-n testuser test7 test7 test7 --createOnly --email --remove --expression 'ip.src == 10.0.0.2'");
$users = viewerPostToken("/api/users?arkimeRegressionUser=admin", "", $adminToken);
eq_or_diff($users->{data}->[8], $user8, "Create only doesn't overwrite user");

# can update a user
my $user1 = $users->{data}->[2];
is($user1->{id}, "test1");
addUser("-n testuser test1 test1 test1 --email");
$users = viewerPostToken("/api/users?arkimeRegressionUser=admin", "", $adminToken);
is($users->{data}->[2]->{id}, "test1");
ok($users->{data}->[2]->{emailSearch}, "Can update exiting user");


#### Auth Header tests
my $mresponse;

$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/");
is ($response->code, 401);
is ($response->content, 'Unauthorized');

$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/", ':arkime_user' => '');
is ($response->code, 403);
is ($response->content, '{"success":false,"text":"User name header is empty"}');

$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/", ':arkime_user' => 'authtest1');
is ($response->code, 200);

$response = viewerGet("/regressionTests/getUser/authtest1");
delete $response->{lastUsed};
$mresponse = multiGet("/regressionTests/getUser/authtest1");
delete $mresponse->{lastUsed};
eq_or_diff($response, $mresponse);

delete $response->{passStore};
eq_or_diff($response, from_json('{"headerAuthEnabled":true,"enabled":true,"userId":"authtest1","webEnabled":true,"removeEnabled":false,"userName":"authtest1","packetSearch":true,"emailSearch":true,"expression":"","settings":{},"roles":["arkimeUser","cont3xtUser","parliamentUser","wiseUser"]}'));

addUser("-n test3 authtest2 authtest2 authtest2");
$response = viewerGet("/regressionTests/getUser/authtest2");

$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/", ':arkime_user' => 'authtest2');
is ($response->content, '{"success":false,"text":"User header auth not enabled"}');
is ($response->code, 403);

# Bad password
$ArkimeTest::userAgent->credentials( "$ArkimeTest::host:8126", 'Moloch', 'authtest2', 'authtest222' );
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/");
is ($response->content, 'Unauthorized');
is ($response->code, 401);

# Bad password but username header
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/", ':arkime_user' => 'authtest2');
is ($response->content, '{"success":false,"text":"User header auth not enabled"}');
is ($response->code, 403);

# Good password
$ArkimeTest::userAgent->credentials( "$ArkimeTest::host:8126", 'Moloch', 'authtest2', 'authtest2' );
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/");
is ($response->code, 200);

# /receiveSession
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/receiveSession");
is ($response->content, "receive session only allowed s2s");
is ($response->code, 401);

# /ReceiveSession
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/ReceiveSession");
is ($response->content, "receive session only allowed s2s");
is ($response->code, 401);

# No arkimeUser role
$ArkimeTest::userAgent->credentials( "$ArkimeTest::host:8126", 'Moloch', 'test6', 'test6' );
$response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8126/api/upload", "x-arkime-cookie" => $test6Token);
is ($response->content, "Not covered by role");
is ($response->code, 403);

$ArkimeTest::userAgent->credentials( "$ArkimeTest::host:8126", 'Moloch', 'test7', 'test7');
$response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8126/api/upload", "x-arkime-cookie" => $test7Token);
is ($response->content, "Missing file");
is ($response->code, 403);

# No arkimeUser role
$ArkimeTest::userAgent->credentials( "$ArkimeTest::host:8126", 'Moloch', 'test8', 'test8' );
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/");
is ($response->content, "Need arkimeUser role assigned");
is ($response->code, 403);

# No role auth
$ArkimeTest::userAgent->credentials( "$ArkimeTest::host:8126", 'Moloch', 'role:role', 'role:role' );
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/");
is ($response->code, 403);
is ($response->content, '{"success":false,"text":"Can not authenticate with role"}');

$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/", ':arkime_user' => 'role:role');
is ($response->code, 403);
is ($response->content, '{"success":false,"text":"Can not authenticate with role"}');

$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/?arkimeRegressionUser=role:role");
is ($response->code, 403);
is ($response->content, '{"success":false,"text":"Can not authenticate with role"}');

# s2s

# /receiveSession - empty
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/receiveSession", ':x-arkime-auth' => '');
is ($response->content, '{"success":false,"text":"S2S auth header corrupt"}');
is ($response->code, 403);

# /receiveSession - garbage
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/receiveSession", ':x-arkime-auth' => 'garbage');
is ($response->content, '{"success":false,"text":"S2S auth header corrupt"}');
is ($response->code, 403);

# /receiveSession - empty json
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/receiveSession", ':x-arkime-auth' => '{}');
is ($response->content, '{"success":false,"text":"S2S bad path"}');
is ($response->code, 403);

# /receiveSession - bad path
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/receiveSession", ':x-arkime-auth' => '{"path": "/"}');
is ($response->content, '{"success":false,"text":"S2S bad user"}');
is ($response->code, 403);

# /receiveSession - bad date
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/receiveSession", ':x-arkime-auth' => '{"path": "/", "user": "authtest2"}');
is ($response->content, '{"success":false,"text":"S2S bad date"}');
is ($response->code, 403);

# /receiveSession - user role
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/receiveSession", ':x-arkime-auth' => '{"path": "/", "user": "role:authtest2", "date": 1}');
is ($response->content, '{"success":false,"text":"Can not authenticate with role"}');
is ($response->code, 403);

# /receiveSession - url mismatch
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/receiveSession", ':x-arkime-auth' => '{"path": "/", "user": "authtest2", "date": 1}');
is ($response->content, '{"success":false,"text":"Unauthorized based on bad url"}');
is ($response->code, 403);

# /receiveSession - url mismatch
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/receiveSession", ':x-arkime-auth' => '{"path": "/receiveSession", "user": "authtest2", "date": 1}');
is ($response->content, '{"success":false,"text":"Unauthorized based on timestamp - check that all Arkime viewer machines have accurate clocks"}');
is ($response->code, 403);

# /receiveSession - good but wrong method
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/receiveSession", ':x-arkime-auth' => '{"path": "/receiveSession", "user": "authtest2", "date": ' . time() * 1000 .'}');
is ($response->content, 'Permission denied');
is ($response->code, 403);

# /receiveSession - good
$response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8126/receiveSession", ':x-arkime-auth' => '{"path": "/receiveSession", "user": "authtest2", "date": ' . time() * 1000 .'}');
my $rjson = from_json($response->content);
is ($rjson->{success}, 0, "receiveSession missing saveId");
is ($rjson->{i18n}, "api.sessions.missingSaveId", "receiveSession missing saveId i18n");
is ($response->code, 200);

# /users - bad
$ArkimeTest::userAgent->credentials( "$ArkimeTest::host:8126", 'Moloch', 'test7', 'test7');
$response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8126/users", "x-arkime-cookie" => $test7Token);
is ($response->content, "Permission denied");

$ArkimeTest::userAgent->credentials( "$ArkimeTest::host:8126", 'Moloch', 'test7', 'test7');
$response = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8126/Users", "x-arkime-cookie" => $test7Token);
is ($response->content, "Permission denied");


#### user-role-mappings tests

# Test role based on this.userId ending with '-test'
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/", ':arkime_user' => 'roleuser-test');
is ($response->code, 200, "roleuser-test auth success");
$response = viewerGet("/regressionTests/getUser/roleuser-test");
ok(grep(/^role:testRole$/, @{$response->{roles}}), "roleuser-test has role:testRole");
ok(grep(/^arkimeUser$/, @{$response->{roles}}), "roleuser-test still has arkimeUser");

# Test role based on header value
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/", ':arkime_user' => 'headeruser', 'x-test-role' => 'special');
is ($response->code, 200, "headeruser auth success");
$response = viewerGet("/regressionTests/getUser/headeruser");
ok(grep(/^role:headerRole$/, @{$response->{roles}}), "headeruser has role:headerRole");
ok(grep(/^arkimeUser$/, @{$response->{roles}}), "headeruser still has arkimeUser");

# Test that user without matching conditions does NOT get the role
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/", ':arkime_user' => 'normaluser');
is ($response->code, 200, "normaluser auth success");
$response = viewerGet("/regressionTests/getUser/normaluser");
ok(!grep(/^role:testRole$/, @{$response->{roles}}), "normaluser does NOT have role:testRole");
ok(!grep(/^role:headerRole$/, @{$response->{roles}}), "normaluser does NOT have role:headerRole");
ok(grep(/^arkimeUser$/, @{$response->{roles}}), "normaluser still has arkimeUser");

# Test combined role (both userId and header must match)
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/", ':arkime_user' => 'combouser', 'x-test-role' => 'combo');
is ($response->code, 200, "combouser auth success");
$response = viewerGet("/regressionTests/getUser/combouser");
ok(grep(/^role:combinedRole$/, @{$response->{roles}}), "combouser has role:combinedRole");

# Test that role is REMOVED when header is no longer present
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/", ':arkime_user' => 'headeruser');
is ($response->code, 200, "headeruser without header auth success");
$response = viewerGet("/regressionTests/getUser/headeruser");
ok(!grep(/^role:headerRole$/, @{$response->{roles}}), "headeruser loses role:headerRole when header missing");
ok(grep(/^arkimeUser$/, @{$response->{roles}}), "headeruser still has arkimeUser after role removal");

# Test that expression returning false removes the role (combouser without matching header)
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8126/", ':arkime_user' => 'combouser', 'x-test-role' => 'wrong');
is ($response->code, 200, "combouser with wrong header auth success");
$response = viewerGet("/regressionTests/getUser/combouser");
ok(!grep(/^role:combinedRole$/, @{$response->{roles}}), "combouser loses role:combinedRole when expression is false");


#### JWT Auth Header tests (test4 node on port 8127)

# Valid JWT with short_id claim — should auto-create user
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8127/", 'x-jwt-data' => 'eyJhbGciOiJub25lIiwidHlwIjoiSldUIn0.eyJzaG9ydF9pZCI6Imp3dHVzZXIxIiwicHJlZmVycmVkX3VzZXJuYW1lIjoiSldUIFVzZXIgMSJ9.fakesig');
is ($response->code, 200, "JWT header auth success");

# Verify user was created with short_id as userId
$response = viewerGet("/regressionTests/getUser/jwtuser1");
is ($response->{userId}, "jwtuser1", "JWT user created with short_id as userId");

# Missing JWT header — should 401
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8127/");
is ($response->code, 401, "Missing JWT header returns 401");

# Invalid JWT (not 3 dot-separated parts) — should 403
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8127/", 'x-jwt-data' => 'not-a-jwt');
is ($response->code, 403, "Invalid JWT returns 403");
is ($response->content, '{"success":false,"text":"Invalid JWT in header"}');

# JWT without the required short_id claim — should 403
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8127/", 'x-jwt-data' => 'eyJhbGciOiJub25lIiwidHlwIjoiSldUIn0.eyJlbWFpbCI6InRlc3RAdGVzdC5jb20ifQ.fakesig');
is ($response->code, 403, "JWT without required claim returns 403");
is ($response->content, '{"success":false,"text":"User name header is empty"}');

# JWT with role: prefix in claim — should be rejected
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8127/", 'x-jwt-data' => 'eyJhbGciOiJub25lIiwidHlwIjoiSldUIn0.eyJzaG9ydF9pZCI6InJvbGU6ZXZpbCIsInByZWZlcnJlZF91c2VybmFtZSI6IkhhY2tlciJ9.fakesig');
is ($response->code, 403, "JWT with role: prefix rejected");
is ($response->content, '{"success":false,"text":"Can not authenticate with role"}');

# JWT with department claim — should get role:securityTeam via role mappings
$response = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8127/", 'x-jwt-data' => 'eyJhbGciOiJub25lIiwidHlwIjoiSldUIn0.eyJzaG9ydF9pZCI6Imp3dHVzZXIyIiwicHJlZmVycmVkX3VzZXJuYW1lIjoiSldUIFVzZXIgMiIsImRlcGFydG1lbnQiOiJzZWN1cml0eSJ9.fakesig');
is ($response->code, 200, "JWT with department claim auth success");
$response = viewerGet("/regressionTests/getUser/jwtuser2");
ok(grep(/^role:securityTeam$/, @{$response->{roles}}), "jwtuser2 has role:securityTeam from JWT department claim");
ok(grep(/^arkimeUser$/, @{$response->{roles}}), "jwtuser2 still has arkimeUser");


# cleanup
my $token = getTokenCookie();
$response = viewerDeleteToken("/api/user/role:role", $token);
eq_or_diff($response, from_json('{"success": false, "text": "Can not delete superAdmin unless you are superAdmin"}'));
$response = viewerDeleteToken("/api/user/admin", $token);
eq_or_diff($response, from_json('{"success": false, "text": "Can not delete superAdmin unless you are superAdmin"}'));
viewerDeleteToken("/api/user/test1", $token);
viewerDeleteToken("/api/user/test2", $token);
viewerDeleteToken("/api/user/test3", $token);
viewerDeleteToken("/api/user/test4", $token);
viewerDeleteToken("/api/user/test5", $token);
viewerDeleteToken("/api/user/test6", $token);
viewerDeleteToken("/api/user/test7", $token);
viewerDeleteToken("/api/user/test8", $token);
viewerDeleteToken("/api/user/authtest1", $token);
viewerDeleteToken("/api/user/authtest2", $token);
viewerDeleteToken("/api/user/roleuser-test", $token);
viewerDeleteToken("/api/user/headeruser", $token);
viewerDeleteToken("/api/user/normaluser", $token);
viewerDeleteToken("/api/user/combouser", $token);
viewerDeleteToken("/api/user/jwtuser1", $token);
viewerDeleteToken("/api/user/jwtuser2", $token);

$response = viewerGet("/api/user/__proto__");
eq_or_diff($response, from_json('{"success": false, "text": "Bad path &#47;api&#47;user&#47;__proto__"}'));

$users = viewerPostToken("/api/users?arkimeRegressionUser=admin", "", $adminToken);
is (@{$users->{data}}, 7, "Two supers plus 4 role mappings left");

viewerGet("/regressionTests/deleteAllUsers");

$users = viewerPost("/api/users", "");
diag Dumper($users) if (@{$users->{data}} != 1);;
is (@{$users->{data}}, 1, "Empty users table");
viewerGet("/regressionTests/deleteAllUsers");
