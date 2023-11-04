use Test::More tests => 166;
use Cwd;
use URI::Escape;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $json;

# clean old users
    clearIndex("tests_users");

# Get tokens
    my $token = getTokenCookie();
    my $token2 = getTokenCookie2();
    my $superAdminToken = getTokenCookie('superAdmin');

# clean old crons
    clearIndex("tests_queries");

# users
    my $users = viewerPost("/api/users", "");
    is (@{$users->{data}}, 1, "anonymous, superAdmin");

# csv
    my $csv = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8123/api/users.csv", Content => "")->content;
    $csv =~ s/\r//g;
    eq_or_diff ($csv, 'userId, userName, enabled, webEnabled, headerAuthEnabled, roles, emailSearch, removeEnabled, packetSearch, hideStats, hideFiles, hidePcap, disablePcapDownload, expression, timeLimit
anonymous,,true,true,false,"arkimeAdmin, cont3xtUser, parliamentUser, usersAdmin, wiseUser",true,true,true,,,,,,
', "CSV Users");

# Can't create system rule
    $json = viewerPostToken("/api/user", '{"userId": "usersAdmin", "userName": "UserName", "enabled":true, "password":"password"}', $token);
    eq_or_diff($json, from_json('{"text": "User ID can\'t be a system role id", "success": false}'));

    $json = viewerPostToken("/api/user/usersAdmin", '{"userName": "UserName", "enabled":true, "password":"password", "roles":["superAdmin"]}', $token);
    eq_or_diff($json, from_json('{"text": "User ID can\'t be a system role id", "success": false}'));

    $json = viewerPostToken("/api/user/sac-usersAdmin!", '{"userId": "usersAdmin\u001b", "userName": "UserName", "enabled":true, "password":"password", "roles":["arkimeUser"]}', $token);
    eq_or_diff($json, from_json('{"text": "User not found", "success": false}'));

# Create Missing/Emptry fields
    $json = viewerPostToken("/api/user", '{"userName": "UserName", "enabled":true, "password":"password"}', $token);
    eq_or_diff($json, from_json('{"text": "Missing/Empty required fields", "success": false}'));

    $json = viewerPostToken("/api/user", '{"userId": "", "userName": "UserName", "enabled":true, "password":"password"}', $token);
    eq_or_diff($json, from_json('{"text": "Missing/Empty required fields", "success": false}'));

    $json = viewerPostToken("/api/user", '{"userId": "<script>", "userName": "UserName", "enabled":true, "password":"password"}', $token);
    eq_or_diff($json, from_json('{"text": "User ID must be word characters", "success": false}'));

    $json = viewerPostToken("/api/user", '{"userId": "sac-test1", "enabled":true, "password":"password"}', $token);
    eq_or_diff($json, from_json('{"text": "Missing/Empty required fields", "success": false}'));

    $json = viewerPostToken("/api/user", '{"userId": "sac-test1", "userName": "", "enabled":true, "password":"password"}', $token);
    eq_or_diff($json, from_json('{"text": "Missing/Empty required fields", "success": false}'));

    $json = viewerPostToken("/api/user", '{"userId": "sac-test1", "userName": " ", "enabled":true, "password":"password"}', $token);
    eq_or_diff($json, from_json('{"text": "Username can not be empty", "success": false}'));

    $json = viewerPostToken("/api/user", '{"userId": "sac-test1", "userName": "UserName", "enabled":true}', $token);
    eq_or_diff($json, from_json('{"text": "Password needs to be at least 3 characters", "success": false}'));

    $json = viewerPostToken("/api/user", '{"userId": "sac-test1", "userName": "UserName", "enabled":true, "password":""}', $token);
    eq_or_diff($json, from_json('{"text": "Password needs to be at least 3 characters", "success": false}'));

    $json = viewerPostToken("/api/user", '{"userId": "sac-test1", "userName": "UserName", "enabled":true, "password":"ab"}', $token);
    eq_or_diff($json, from_json('{"text": "Password needs to be at least 3 characters", "success": false}'));

    $json = viewerPostToken("/api/user", '{"userId": "sac-test1", "userName": "UserName", "enabled":true, "password":"abc", "expression": false}', $token);
    eq_or_diff($json, from_json('{"text": "Expression must be a string when present", "success": false}'));

# Add User 1
    $json = viewerPostToken("/api/user", '{"userId": "sac-test1", "userName": "UserName", "enabled":true, "password":"password", "roles": ["arkimeUser"]}', $token);
    eq_or_diff($json, from_json('{"text": "User created succesfully", "success": true}'));

    $users = viewerPost("/api/users?arkimeRegressionUser=notadmin", "");
    eq_or_diff($users, from_json('{"text": "You do not have permission to access this resource", "success": false}'));

    esGet("/_flush");
    esGet("/_refresh");

    $users = viewerPost("/api/users", "");
    is (@{$users->{data}}, 3, "Check add #1");

    eq_or_diff($users->{data}->[2], from_json('{"roles": ["arkimeUser"], "userId": "sac-test1", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName", "id": "sac-test1", "emailSearch": false, "enabled": true, "webEnabled": false, "packetSearch": false, "welcomeMsgNum": 0, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "lastUsed": 0, "roleAssigners": []}', {relaxed => 1}), "Test User Add", { context => 3 });


    # This will set a lastUsed time, make sure DB is updated with sleep
    sleep(1);
    esGet("/_flush");
    esGet("/_refresh");
    my $test1Token = getTokenCookie("sac-test1");

    $users = viewerPost2("/api/users", "");
    is (@{$users->{data}}, 3, "Check add #2");

    is (exists $users->{data}->[1]->{lastUsed}, 1, "last used exists #1");
    my $lastUsedTimestamp1 = $users->{data}->[1]->{lastUsed};
    delete $users->{data}->[2]->{lastUsed};

    eq_or_diff($users->{data}->[2], from_json('{"roles": ["arkimeUser"], "userId": "sac-test1", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName", "id": "sac-test1", "emailSearch": false, "enabled": true, "webEnabled": false, "packetSearch": false, "welcomeMsgNum": 0, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "roleAssigners": []}', {relaxed => 1}), "Test User Add", { context => 3 });

# Check appinfo works
    $json = viewerGetToken("/api/appInfo", $token);
    eq_or_diff(sort($json->{roles}), from_json('["arkimeAdmin", "arkimeUser", "cont3xtAdmin", "cont3xtUser", "parliamentAdmin", "parliamentUser", "superAdmin", "usersAdmin", "wiseAdmin", "wiseUser"]'));
    my @roles = sort @{$json->{user}->{roles}};
    eq_or_diff(\@roles, from_json('["arkimeAdmin", "arkimeUser", "cont3xtUser", "parliamentUser", "usersAdmin", "wiseUser"]'));

# Can we create superAdmin
    $json = viewerPostToken("/api/user", '{"userId": "testSuper", "userName": "SUserName", "enabled":true, "password":"password", "roles":["superAdmin"]}', $token);
    eq_or_diff($json, from_json('{"text": "Can not create superAdmin unless you are superAdmin", "success": false}'));

# Can we update superAdmin
    $json = viewerPostToken("/api/user/sac-test1", '{"userName":"UserNameUpdated", "removeEnabled":true, "headerAuthEnabled":true, "expression":"foo", "emailSearch":true, "webEnabled":true, "roles":["superAdmin"], "packetSearch": false}', $token);
    eq_or_diff($json, from_json('{"text": "Can not modify superAdmin unless you are superAdmin", "success": false}'));

    $json = viewerPostToken("/api/user/sac-test1", '{"userName":"UserNameUpdated", "removeEnabled":true, "headerAuthEnabled":true, "expression":"foo", "emailSearch":true, "webEnabled":true, "roles":["usersAdmin"], "packetSearch": false}', $token);
    eq_or_diff($json, from_json('{"text": "Only superAdmin user can enable Admin roles on a user", "success": false}'));
# Update User Server 1
    $json = viewerPostToken("/api/user/sac-test1?arkimeRegressionUser=superAdmin", '{"userName":"UserNameUpdated", "removeEnabled":true, "headerAuthEnabled":true, "expression":"foo", "emailSearch":true, "webEnabled":true, "roles":["usersAdmin", "arkimeUser"], "packetSearch": false}', $superAdminToken);
    eq_or_diff($json, from_json('{"text": "User sac-test1 updated successfully", "success": true}'));

    esGet("/_flush");
    esGet("/_refresh");

    my $sacpos=2;
    $users = viewerPost("/api/users?arkimeRegressionUser=sac-test1", "");
    is (@{$users->{data}}, 4, "Check Update #1");

    # Force last used to be increased
    esGet("/_flush");
    esGet("/_refresh");
    sleep(2);

    $users = viewerPost("/api/users", "");
    is (exists $users->{data}->[$sacpos]->{lastUsed}, 1, "last used exists #2");
    my $lastUsedTimestamp2 = $users->{data}->[$sacpos]->{lastUsed};
    cmp_ok($lastUsedTimestamp1, '<', $lastUsedTimestamp2, "last used updated");
    delete $users->{data}->[$sacpos]->{lastUsed};

    eq_or_diff($users->{data}->[$sacpos], from_json('{"roles": ["usersAdmin", "arkimeUser"], "userId": "sac-test1", "removeEnabled": true, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated", "id": "sac-test1", "emailSearch": true, "enabled": false, "webEnabled": true, "packetSearch": false, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "welcomeMsgNum": 0, "roleAssigners": []}', {relaxed => 1}), "Test User Update", { context => 3 });

    $users = viewerPost2("/api/users", "");
    is (@{$users->{data}}, 4, "Check Update #2");
    is (exists $users->{data}->[$sacpos]->{lastUsed}, 1, "last used exists #3");
    delete $users->{data}->[$sacpos]->{lastUsed};
    eq_or_diff($users->{data}->[$sacpos], from_json('{"roles": ["usersAdmin", "arkimeUser"], "userId": "sac-test1", "removeEnabled": true, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated", "id": "sac-test1", "emailSearch": true, "enabled": false, "webEnabled": true, "packetSearch": false, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "welcomeMsgNum": 0, "roleAssigners": []}', {relaxed => 1}), "Test User Update", { context => 3 });

# update user settings
    $json = viewerPostToken("/api/user/settings?arkimeRegressionUser=sac-test1", '{"logo":"testlogo.png","__proto":{"bad":"stuff"}}', $test1Token);
    eq_or_diff($json, from_json('{"text": "Updated user settings successfully", "success": true}'));

    $json = viewerGetToken("/api/user/settings?arkimeRegressionUser=sac-test1", $test1Token);
    ok(!exists $json->{__proto__}, "no prototype pollution");
    eq_or_diff($json->{logo}, "testlogo.png");

# Add User 2
    my $json = viewerPostToken2("/api/user", '{"userId": "sac-test2", "userName": "UserName2", "enabled":true, "password":"password"}', $token2);
    eq_or_diff($json, from_json('{"text": "User created succesfully", "success": true}'));

    esGet("/_flush");
    esGet("/_refresh");
    # flush the cache on 1 since we created the user on 2
    viewerPost("/regressionTests/flushCache");
    my $test2pos = 3;
    my $test2Token = getTokenCookie('sac-test2');

    $users = viewerPost("/api/users", "");
    is (@{$users->{data}}, 5, "Check second add #1");
    eq_or_diff($users->{data}->[$test2pos], from_json('{"roles": [], "lastUsed": 0, "userId": "sac-test2", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName2", "id": "sac-test2", "emailSearch": false, "enabled": true, "webEnabled": false, "packetSearch": false, "welcomeMsgNum": 0, "roleAssigners": [], "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false}', {relaxed => 1}), "Test User Add", { context => 3 });

    $users = viewerPost2("/api/users", "");
    is (@{$users->{data}}, 5, "Check second add #2");
    eq_or_diff($users->{data}->[$test2pos], from_json('{"roles": [], "lastUsed": 0, "userId": "sac-test2", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName2", "id": "sac-test2", "emailSearch": false, "enabled": true, "webEnabled": false, "packetSearch": false, "welcomeMsgNum": 0, "roleAssigners": [], "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false}', {relaxed => 1}), "Test User Add", { context => 3 });

# Filter
    $users = viewerPost("/api/users", "filter=test");
    is (@{$users->{data}}, 2, "filter both");
    is ($users->{recordsTotal}, 5);
    is ($users->{recordsFiltered}, 2);

    $users = viewerPost("/api/users", "filter=sac-test1");
    is (@{$users->{data}}, 1, "filter one");
    is ($users->{recordsTotal}, 5);
    is ($users->{recordsFiltered}, 1);

# start, length
    $users = viewerPost("/api/users", "start=0&length=2");
    is (@{$users->{data}}, 2, "start=0&length=2");
    is ($users->{recordsTotal}, 5);
    is ($users->{recordsFiltered}, 5);

    $users = viewerPost("/api/users", "start=1&length=2");
    is (@{$users->{data}}, 2, "start=1&length=2");
    is ($users->{recordsTotal}, 5);
    is ($users->{recordsFiltered}, 5);

    $users = viewerPost("/api/users", "start=0&length=1");
    is (@{$users->{data}}, 1, "start=1&length=1");
    is ($users->{recordsTotal}, 5);
    is ($users->{recordsFiltered}, 5);

    $users = viewerPost("/api/users", "start=666&length=100000");
    is (@{$users->{data}}, 0, "start=0&length=100000");
    is ($users->{recordsTotal}, 0);
    is ($users->{recordsFiltered}, 0);

# Update User Shared Server
    $json = viewerPostToken2("/api/user/sac-test2", '{"userName":"UserNameUpdated2", "enabled":true, "removeEnabled":false, "headerAuthEnabled":true, "expression":"foo", "emailSearch":true, "webEnabled":true, "roles": [], "packetSearch": false}', $token2);
    eq_or_diff($json, from_json('{"text": "User sac-test2 updated successfully", "success": true}'));

    $users = viewerPost("/api/users", "");
    is (@{$users->{data}}, 5, "Check second Update #1");
    delete $users->{data}->[$test2pos]->{lastUsed};
    eq_or_diff($users->{data}->[$test2pos], from_json('{"roles": [], "userId": "sac-test2", "removeEnabled": false, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated2", "id": "sac-test2", "emailSearch": true, "enabled": true, "webEnabled": true, "packetSearch": false, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "welcomeMsgNum": 0, "roleAssigners": []}', {relaxed => 1}), "Test User Update", { context => 3 });

    $users = viewerPost2("/api/users", "");
    is (@{$users->{data}}, 5, "Check second Update #2");
    delete $users->{data}->[$test2pos]->{lastUsed};
    eq_or_diff($users->{data}->[$test2pos], from_json('{"roles": [], "userId": "sac-test2", "removeEnabled": false, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated2", "id": "sac-test2", "emailSearch": true, "enabled": true, "webEnabled": true, "packetSearch": false, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "welcomeMsgNum": 0, "roleAssigners": []}', {relaxed => 1}), "Test User Update", { context => 3 });

# Reverse settings
    $json = viewerPostToken2("/api/user/sac-test2", '{"userName":"UserNameUpdated3", "enabled":false, "removeEnabled":true, "headerAuthEnabled":false, "expression":"", "emailSearch":false, "webEnabled":false, "roles": [], "packetSearch": false}', $token2);
    eq_or_diff($json, from_json('{"text": "User sac-test2 updated successfully", "success": true}'));

    $users = viewerPost("/api/users", "");
    is (@{$users->{data}}, 5, "Check second Update #1");
    delete $users->{data}->[$test2pos]->{lastUsed};
    eq_or_diff($users->{data}->[$test2pos], from_json('{"roles": [], "userId": "sac-test2", "removeEnabled": true, "expression": "", "headerAuthEnabled": false, "userName": "UserNameUpdated3", "id": "sac-test2", "emailSearch": false, "enabled": false, "webEnabled": false, "packetSearch": false, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "welcomeMsgNum": 0, "roleAssigners": []}', {relaxed => 1}), "Test User Update", { context => 3 });

    $users = viewerPost2("/api/users", "");
    is (@{$users->{data}}, 5, "Check second Update #2");
    delete $users->{data}->[$test2pos]->{lastUsed};
    eq_or_diff($users->{data}->[$test2pos], from_json('{"roles": [], "userId": "sac-test2", "removeEnabled": true, "expression": "", "headerAuthEnabled": false, "userName": "UserNameUpdated3", "id": "sac-test2", "emailSearch": false, "enabled": false, "webEnabled": false, "packetSearch": false, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "welcomeMsgNum": 0, "roleAssigners": []}', {relaxed => 1}), "Test User Update", { context => 3 });

# Column
    my $info = viewerGet("/api/user/columns?arkimeRegressionUser=sac-test1");
    eq_or_diff($info, from_json("[]"), "column: empty");

    $info = viewerPostToken("/api/user/column?arkimeRegressionUser=sac-test1", '{"name": "column1", "columns": ["srcIp","dstIp"], "order": [["lastPacket", "asc"]]}', $test1Token);
    ok($info->{success}, "column: create success");
    is($info->{name}, "column1", "column: create name");

    $info = viewerPostToken("/api/user/column?arkimeRegressionUser=sac-test1", '{"name": "column2", "columns": ["srcIp","dstIp"], "order": [["lastPacket", "asc"]]}', $test1Token);
    ok($info->{success}, "column: create success");
    is($info->{name}, "column2", "column: create name");

    $info = viewerGet("/api/user/columns?arkimeRegressionUser=sac-test1");
    eq_or_diff($info, from_json('[{"name":"column1","order":[["lastPacket","asc"]],"columns":["srcIp","dstIp"]},{"name":"column2","order":[["lastPacket","asc"]],"columns":["srcIp","dstIp"]}]'), "column: 1 item");

    $info = viewerGet("/api/user/columns?arkimeRegressionUser=anonymous&userId=sac-test1");
    eq_or_diff($info, from_json('[{"name":"column1","order":[["lastPacket","asc"]],"columns":["srcIp","dstIp"]},{"name":"column2","order":[["lastPacket","asc"]],"columns":["srcIp","dstIp"]}]'), "column: 1 item admin");

    $info = viewerDeleteToken("/api/user/column/fred?arkimeRegressionUser=sac-test1", $test1Token);
    ok(! $info->{success}, "column: delete not found");

    $info = viewerGet("/api/user/columns?arkimeRegressionUser=sac-test1");
    eq_or_diff($info, from_json('[{"name":"column1","order":[["lastPacket","asc"]],"columns":["srcIp","dstIp"]},{"name":"column2","order":[["lastPacket","asc"]],"columns":["srcIp","dstIp"]}]'), "column: 1 item");

    $info = viewerPutToken("/api/user/column/column1?arkimeRegressionUser=sac-test1", '{"name": "column1", "columns": ["srcIp","dstIp","info"], "order": [["lastPacket","asc"]]}', $test1Token);
    ok($info->{success}, "column: update");

    $info = viewerDeleteToken("/api/user/column/column1?arkimeRegressionUser=sac-test1", $test1Token);
    ok($info->{success}, "column: delete found");

    $info = viewerDeleteToken("/api/user/column/column2?arkimeRegressionUser=sac-test1", $test1Token);
    ok($info->{success}, "column: delete found");

    $info = viewerGet("/api/user/columns?arkimeRegressionUser=sac-test1");
    eq_or_diff($info, from_json("[]"), "column: empty");


# Current
    $info = viewerGet("/api/user?arkimeRegressionUser=sac-test1");
    ok(!exists $info->{passStore}, "current: no passtore");

# spiview fields
    $info = viewerGet("/api/user/spiview?arkimeRegressionUser=sac-test1");
    eq_or_diff($info, from_json("[]"), "spiview fields: empty");

    $info = viewerPostToken("/api/user/spiview?arkimeRegressionUser=sac-test1", '{"name": "sfields1", "fields": ["srcIp","dstIp"]}', $test1Token);
    ok($info->{success}, "spiview fields: create success");
    is($info->{name}, "sfields1", "spiview fields: create name");

    $info = viewerGet("/api/user/spiview?arkimeRegressionUser=sac-test1");
    eq_or_diff($info, from_json('[{"name":"sfields1","fields":["srcIp","dstIp"]}]'), "spiview fields: 1 item");

    $info = viewerGet("/api/user/spiview?arkimeRegressionUser=anonymous&userId=sac-test1");
    eq_or_diff($info, from_json('[{"name":"sfields1","fields":["srcIp","dstIp"]}]'), "spiview fields: 1 item admin");

    $info = viewerPutToken("/api/user/spiview/sfields1?arkimeRegressionUser=sac-test1", '{"name": "sfields1", "fields": ["srcIp","dstIp","node"]}', $test1Token);
    ok($info->{success}, "spiview fields: update success");

    $info = viewerDeleteToken("/api/user/spiview/fred?arkimeRegressionUser=sac-test1", $test1Token);
    ok(!$info->{success}, "spiview fields: delete not found");

    $info = viewerGet("/api/user/spiview?arkimeRegressionUser=sac-test1");
    eq_or_diff($info, from_json('[{"name":"sfields1","fields":["srcIp","dstIp","node"]}]'), "spiview fields: 1 item");

    $info = viewerDeleteToken("/api/user/spiview/sfields1?arkimeRegressionUser=sac-test1", $test1Token);
    ok($info->{success}, "spiview fields: delete found");

    $info = viewerGet("/api/user/spiview?arkimeRegressionUser=sac-test1");
    eq_or_diff($info, from_json("[]"), "spiview fields: empty");

# Messages
    $info = viewerPutToken("/api/user/sac-test1/acknowledge", '{"msgNum":2}', $token2);
    ok(!$info->{success}, "can't update welcome message number for another user");

    $info = viewerPutToken("/api/user/sac-test1/acknowledge?arkimeRegressionUser=sac-test1", '{}', $test1Token);
    eq_or_diff($info, from_json('{"text": "Message number required", "success": false}'));

    $info = viewerPutToken("/api/user/sac-test1/acknowledge?arkimeRegressionUser=sac-test1", '{"msgNum":"foo"}', $test1Token);
    eq_or_diff($info, from_json('{"text": "welcomeMsgNum is not integer", "success": false}'));

    $info = viewerPutToken("/api/user/sac-test1/acknowledge?arkimeRegressionUser=sac-test1", '{"msgNum":2}', $test1Token);
    ok($info->{success}, "update welcome message number");

    $info = viewerGet("/api/user?arkimeRegressionUser=sac-test1");
    eq_or_diff($info->{welcomeMsgNum}, 2, "welcome message number is correct");

# user time limit
    $json = viewerPostToken("/api/user/sac-test2", '{"timeLimit":"72", "roles": ["arkimeUser"]}', $token);
    eq_or_diff($json, from_json('{"text": "User sac-test2 updated successfully", "success": true}'));

    $users = viewerPost("/api/users", "");
    eq_or_diff($users->{data}->[$test2pos]->{timeLimit}, 72, "time limit updated");
    $json = viewerGet("/sessions.json?arkimeRegressionUser=sac-test2&date=-1");
    eq_or_diff($json->{error}, "User time limit (72 hours) exceeded", "user can't exceed their time limit");
    $json = viewerGet("/api/sessions?arkimeRegressionUser=sac-test2&date=72");
    is (exists $json->{data}, 1, "user can make a query within their time range");

# valueActions tests
    $json = viewerGet("/api/valueactions?arkimeRegressionUser=sac-test1");
    eq_or_diff($json->{reverseDNS}, from_json('{"url": "api/reversedns?ip=%TEXT%", "name": "Get Reverse DNS", "actionType": "fetch", "category": "ip"}'), 'sac-test1 valueActions');
    eq_or_diff($json->{USERTEST}, from_json('{"url": "https://example.com", "name": "usertest", "category": "url"}'), 'sac-test1 valueActions');

    # not web enabled
    $json = viewerGet("/api/valueactions?arkimeRegressionUser=sac-test2");
    eq_or_diff($json, from_json('{"text": "You do not have permission to access this resource", "success": false}'), 'sac-test2 valueActions');

    $json = viewerGet("/api/valueactions?arkimeRegressionUser=test100");
    ok(! exists $json->{USERTEST});

    $json = viewerGet("/api/valueactions?arkimeRegressionUser=test101");
    ok(! exists $json->{USERTEST});

# fieldActions tests
    $json = viewerGet("/api/fieldActions?arkimeRegressionUser=sac-test1");
    eq_or_diff($json->{ASDF}, from_json('{"url": "https://www.asdf.com?expression=%EXPRESSION%&date=%DATE%&field=%FIELD%&dbField=%DBFIELD%", "name": "Field Action %FIELDNAME%!", "category": "ip"}'), 'fetches field actions');

    # not web enabled
    $json = viewerGet("/api/fieldActions?arkimeRegressionUser=sac-test2");
    eq_or_diff($json, from_json('{"text": "You do not have permission to access this resource", "success": false}'), 'user cannot access field action');

    # not a user:
    $json = viewerGet("/api/fieldActions?arkimeRegressionUser=test100");
    eq_or_diff($json, from_json('{"ALLTESTWISE":{"url":"http:/www.example.com","all":true,"name":"AllWiseTest"},"ALLTEST":{"name":"All Field Action %FIELDNAME%!","url":"https://www.asdf.com?expression=%EXPRESSION%&date=%DATE%&field=%FIELD%&dbField=%DBFIELD%","all":true}}'), 'not a fieldActions user:');

    # notUser:
    $json = viewerGet("/api/fieldActions?arkimeRegressionUser=test101");
    eq_or_diff($json, from_json('{"ALLTEST":{"url":"https://www.asdf.com?expression=%EXPRESSION%&date=%DATE%&field=%FIELD%&dbField=%DBFIELD%","all":true,"name":"All Field Action %FIELDNAME%!"},"ALLTESTWISE":{"url":"http:/www.example.com","all":true,"name":"AllWiseTest"}}'), 'notUser fieldActions');

# state tests
    $json = viewerPostToken("/api/user/state/state1?arkimeRegressionUser=sac-test1", '{"order":"test","visibleHeaders":["firstPacket","lastPacket","src","srcPort","dst","dstPort","totPackets","dbby","node"]}', $test1Token);
    ok($json->{success}, "state create success");
    $json = viewerPostToken("/api/user/state/state1?arkimeRegressionUser=sac-test1", '{"order":[["firstPacket","asc"]],"visibleHeaders":["firstPacket","lastPacket"]}', $test1Token);
    ok($json->{success}, "state update success");
    $json = viewerGetToken("/api/user/state/state1?arkimeRegressionUser=sac-test1", $test1Token);
    eq_or_diff($json->{order}, from_json('[["firstPacket","asc"]]'), "share fetch success");
    eq_or_diff($json->{visibleHeaders}, from_json('["firstPacket","lastPacket"]'), "share fetch success");

# roles
    $json = viewerPostToken("/api/user", '{"userId": "role:sac-test1", "userName": "UserName", "enabled":true, "roles":"bad"}', $token);
    eq_or_diff($json, from_json('{"text": "Roles field must be an array of strings", "success": false}'));

    $json = viewerPostToken("/api/user", '{"userId": "role:sac-test1", "userName": "UserName", "enabled":true, "roles":[false]}', $token);
    eq_or_diff($json, from_json('{"text": "Roles field must be an array of strings", "success": false}'));

    $json = viewerPostToken("/api/user", '{"userId": "role:sac-test1", "userName": "UserName", "enabled":true, "roles": ["role:sac-test1"]}', $token);
    eq_or_diff($json, from_json('{"text": "Can\'t have circular role dependencies", "success": false}'));

    $json = viewerPostToken("/api/user", '{"userId": "role:sac-test1", "userName": "UserName", "enabled":true}', $token);
    eq_or_diff($json, from_json('{"text": "Role created succesfully", "success": true}'));

    $json = viewerPostToken("/api/user", '{"userId": "role:sac-test2", "userName": "UserName", "enabled":true, "roles":["role:sac-test1", "superAdmin"]}', $token);
    eq_or_diff($json, from_json('{"text": "User defined roles can\'t have superAdmin", "success": false}'));

    $json = viewerPostToken("/api/user", '{"userId": "role:sac-test2", "userName": "UserName", "enabled":true, "roles":["role:sac-test1", "usersAdmin"]}', $token);
    eq_or_diff($json, from_json('{"text": "User defined roles can\'t have a system Admin role", "success": false}'));

    $json = viewerPostToken("/api/user", '{"userId": "role:sac-test2", "userName": "UserName", "enabled":true, "roles":["role:sac-test1"]}', $token);
    eq_or_diff($json, from_json('{"text": "Role created succesfully", "success": true}'));

    $json = viewerPost("/api/users?arkimeRegressionUser=role:sac-test1", "");
    eq_or_diff($json, from_json('{"text": "Can not authenticate with role", "success": false}'));

# role tests
    $json = viewerPostToken("/api/user/role:sac-test1", '{"roles":["superAdmin"]}', $token);
    eq_or_diff($json, from_json('{"text": "User defined roles can\'t have superAdmin", "success": false}'));

    $json = viewerPostToken("/api/user/role:sac-test1", '{"roles":["usersAdmin"]}', $token);
    eq_or_diff($json, from_json('{"text": "User defined roles can\'t have a system Admin role", "success": false}'));

    $json = viewerPostToken("/api/user/role:sac-test1", '{"roles":"usersAdmin"}', $token);
    eq_or_diff($json, from_json('{"text": "Roles field must be an array of strings", "success": false}'));

    $json = viewerPostToken("/api/user/role:sac-test1", '{"roles":[false]}', $token);
    eq_or_diff($json, from_json('{"text": "Roles field must be an array of strings", "success": false}'));

    $json = viewerPostToken("/api/user/role:sac-test1", '{"roles":["role:sac-test1"]}', $token);
    eq_or_diff($json, from_json('{"text": "Can\'t have circular role dependencies", "success": false}'));

    $json = viewerPostToken("/api/user/role:sac-test1", '{"userName": "sac-test1", "enabled":true, "password":"password", "roles": ["arkimeUser","user"]}', $token);
    eq_or_diff($json, from_json('{"text": "User roles must be system roles or start with \"role:\"", "success": false}'));

    $json = viewerPostToken("/api/user", '{"userId": "asdf", "userName": "asdf", "enabled":true, "password":"password", "roles":["role:sac-test1","user"]}', $token);
    eq_or_diff($json, from_json('{"text": "User roles must be system roles or start with \"role:\"", "success": false}'));

# role assigner
    $json = viewerPostToken("/api/user/role:sac-test1", '{"userName": "UserName", "enabled":true, "roleAssigners": "foo"}', $token);
    eq_or_diff($json, from_json('{"text": "roleAssigners field must be an array of strings", "success": false}'));

    $json = viewerPostToken("/api/user/role:sac-test1", '{"userName": "UserName", "enabled":true, "roleAssigners": [false]}', $token);
    eq_or_diff($json, from_json('{"text": "roleAssigners field must be an array of strings", "success": false}'));

    $json = viewerPostToken("/api/user/role:sac-test1", '{"userName": "UserName", "enabled":true, "roleAssigners": ["sac-test1"]}', $token);
    eq_or_diff($json, from_json('{"text": "User role:sac-test1 updated successfully", "success": true}'));

    $json = viewerPost("/api/users", "filter=role:sac-test1");
    eq_or_diff($json->{data}->[0]->{roleAssigners}, from_json('["sac-test1"]'));

    $json = viewerPostToken("/api/users/min?arkimeRegressionUser=sac-test1", "", $test1Token);
    eq_or_diff($json, from_json('{"data":[{"userName":"","userId":"anonymous"},{"userId":"notadmin","userName":""},{"userName":"UserNameUpdated","userId":"sac-test1"},{"userName":"UserNameUpdated3","userId":"sac-test2"},{"userName":"","userId":"superAdmin"},{"userId":"test100","userName":""},{"userName":"","userId":"test101"}],"success":true}'));

    # yes - hasRole
    $json = viewerPostToken("/api/users/min?arkimeRegressionUser=sac-test1", '{"roleId": "role:sac-test1"}', $test1Token);
    eq_or_diff($json, from_json('{"data":[{"userName":"","hasRole":false,"userId":"anonymous"},{"userName":"","userId":"notadmin","hasRole":false},{"userId":"sac-test1","hasRole":false,"userName":"UserNameUpdated"},{"hasRole":false,"userId":"sac-test2","userName":"UserNameUpdated3"},{"userName":"","hasRole":false,"userId":"superAdmin"},{"userName":"","hasRole":false,"userId":"test100"},{"userName":"","userId":"test101","hasRole":false}],"success":true}'));

    # no - hasRole
    $json = viewerPostToken("/api/users/min?arkimeRegressionUser=sac-test2", "", $test2Token);
    eq_or_diff($json, from_json('{"data":[{"userName":"","userId":"anonymous"},{"userName":"","userId":"notadmin"},{"userId":"sac-test1","userName":"UserNameUpdated"},{"userId":"sac-test2","userName":"UserNameUpdated3"},{"userName":"","userId":"superAdmin"},{"userName":"","userId":"test100"},{"userName":"","userId":"test101"}],"success":true}'));

    $json = viewerPostToken("/api/users/min?arkimeRegressionUser=sac-test2", '{"roleId": "role:sac-test1"}', $test2Token);
    eq_or_diff($json, from_json('{"text": "You do not have permission to access this resource","success":false}'));

    $json = viewerPostToken("/api/user/sac-test1/assignment?arkimeRegressionUser=sac-test1", '{"roleId": "role:sac-test1", "newRoleState": true}', $test1Token);
    eq_or_diff($json, from_json('{"text": "User sac-test1\'s role role:sac-test1 updated successfully","success":true}'));

    $json = viewerPostToken("/api/user/sac-test1/assignment?arkimeRegressionUser=sac-test1", '{"roleId": "role:sac-test1", "newRoleState": true}', $test1Token);

    $json = viewerPostToken("/api/user/sac-test1/assignment?arkimeRegressionUser=sac-test1", '{"roleId": "role:sac-test1", "newRoleState": false}', $test1Token);
    eq_or_diff($json, from_json('{"text": "User sac-test1\'s role role:sac-test1 updated successfully","success":true}'));

    $json = viewerPostToken("/api/user/sac-test1/assignment?arkimeRegressionUser=sac-test1", '{"roleId": "role:sac-test1", "newRoleState": false}', $test1Token);
    eq_or_diff($json, from_json('{"text": "Can not remove a role that the user does not have","success":false}'));

    $json = viewerPostToken("/api/user/sac-test1/assignment?arkimeRegressionUser=sac-test1", '{"roleId": "role:sac-test1", "newRoleState": "true"}', $test1Token);
    eq_or_diff($json, from_json('{"text": "Missing boolean newRoleState","success":false}'));

    $json = viewerPostToken("/api/user/sac-test1/assignment?arkimeRegressionUser=sac-test1", '{"roleId": "role:unknown", "newRoleState": true}', $test1Token);
    eq_or_diff($json, from_json('{"text": "You do not have permission to access this resource","success":false}'));

    $json = viewerPostToken("/api/user/sac-test1/assignment?arkimeRegressionUser=sac-test1", '{"roleId": false, "newRoleState": true}', $test1Token);
    eq_or_diff($json, from_json('{"text": "You do not have permission to access this resource","success":false}'));

# csv
    my $csv = $ArkimeTest::userAgent->post("http://$ArkimeTest::host:8123/api/users.csv", Content => "")->content;
    $csv =~ s/\r//g;
    eq_or_diff ($csv, 'userId, userName, enabled, webEnabled, headerAuthEnabled, roles, emailSearch, removeEnabled, packetSearch, hideStats, hideFiles, hidePcap, disablePcapDownload, expression, timeLimit
anonymous,,true,true,false,"arkimeAdmin, cont3xtUser, parliamentUser, usersAdmin, wiseUser",true,true,true,,,,,,
notadmin,,true,true,false,"arkimeUser, cont3xtUser, parliamentUser, wiseUser",true,true,true,,,,,,
role:sac-test1,UserName,true,false,false,"",false,false,false,false,false,false,false,,
role:sac-test2,UserName,true,false,false,"role:sac-test1",false,false,false,false,false,false,false,,
sac-test1,UserNameUpdated,false,true,true,"usersAdmin, arkimeUser",true,true,false,false,false,false,false,foo,
sac-test2,UserNameUpdated3,false,false,false,"arkimeUser",false,false,false,false,false,false,false,,72
superAdmin,,true,true,false,"superAdmin",true,true,true,,,,,,
test100,,true,true,false,"arkimeUser, cont3xtUser, parliamentUser, wiseUser",true,true,true,,,,,,
test101,,true,true,false,"arkimeUser, cont3xtUser, parliamentUser, wiseUser",true,true,true,,,,,,
', "CSV Users 2");

# Delete Users
    $json = viewerDeleteToken("/api/user/notadmin", $token);
    $json = viewerDeleteToken("/api/user/sac-test1", $token);
    $json = viewerDeleteToken("/api/user/sac-test2", $token);
    $json = viewerDeleteToken("/api/user/role:sac-test1", $token);
    $json = viewerDeleteToken("/api/user/role:sac-test2", $token);
    esGet("/_refresh");
    $users = viewerPost("/api/users", "");
    is ($users->{data}->[1]->{userId}, "superAdmin", "Removed user #1");
    $users = viewerPost2("/api/users", "");
    is ($users->{data}->[1]->{userId}, "superAdmin", "Removed user #2");

#####
# Roles Tests - Some of these are repeats
addUser("-n testuser testsuperadmin testsuperadmin testsuperadmin --roles superAdmin");
addUser("-n testuser testusersadmin testusersadmin testusersadmin --roles usersAdmin,cont3xtAdmin,arkimeUser");
addUser("-n testuser testarkimeadmin testarkimeadmin testarkimeadmin --roles arkimeAdmin");
addUser("-n testuser testcont3xtadmin testcont3xtadmin testcont3xtadmin --roles cont3xtAdmin,arkimeUser");
addUser("-n testuser testuser testuser testuser");

my $tuToken = getTokenCookie('testuser');
my $saToken = getTokenCookie('testsuperadmin');
my $uaToken = getTokenCookie('testusersadmin');

# Password changing - by user
    $json = viewerPostToken('/api/user/password?arkimeRegressionUser=testuser', '{"newPassword": "wrong"}', $tuToken);
    eq_or_diff($json, from_json('{"text": "Password mismatch", "success": false}'));

    $json = viewerPostToken('/api/user/password?arkimeRegressionUser=testuser', '{"currentPassword": "wrong", "newPassword": "wrong"}', $tuToken);
    eq_or_diff($json, from_json('{"text": "Password mismatch", "success": false}'));

    $json = viewerPostToken('/api/user/password?arkimeRegressionUser=testuser', '{"currentPassword": "testuser"}', $tuToken);
    eq_or_diff($json, from_json('{"text": "New password needs to be at least 3 characters", "success": false}'));

    $json = viewerPostToken('/api/user/password?arkimeRegressionUser=testuser', '{"currentPassword": "testuser", "newPassword": "testuserpass"}', $tuToken);
    eq_or_diff($json, from_json('{"text": "Changed password successfully", "success": true}'));

# Password changing - by usersAdmin
    $json = viewerPostToken('/api/user/password?arkimeRegressionUser=testusersadmin&userId=testuser', '{"newPassword": "sac-test1"}', $uaToken);
    eq_or_diff($json, from_json('{"text": "Changed password successfully", "success": true}'));

    $json = viewerPostToken('/api/user/password?arkimeRegressionUser=testusersadmin&userId=testarkimeadmin', '{"newPassword": "sac-test2"}', $uaToken);
    eq_or_diff($json, from_json('{"text": "Not allowed to change arkimeAdmin password", "success": false}'));

    $json = viewerPostToken('/api/user/password?arkimeRegressionUser=testusersadmin&userId=testcont3xtadmin', '{"newPassword": "test3"}', $uaToken);
    eq_or_diff($json, from_json('{"text": "Changed password successfully", "success": true}'));

# Password changing - by superAdmin
    $json = viewerPostToken('/api/user/password?arkimeRegressionUser=testsuperadmin&userId=testuser', '{"newPassword": "test4"}', $saToken);
    eq_or_diff($json, from_json('{"text": "Changed password successfully", "success": true}'));

    $json = viewerPostToken('/api/user/password?arkimeRegressionUser=testsuperadmin&userId=testarkimeadmin', '{"newPassword": "test5"}', $saToken);
    eq_or_diff($json, from_json('{"text": "Changed password successfully", "success": true}'));

    $json = viewerPostToken('/api/user/password?arkimeRegressionUser=testsuperadmin&userId=testcont3xtadmin', '{"newPassword": "test6"}', $saToken);
    eq_or_diff($json, from_json('{"text": "Changed password successfully", "success": true}'));

# Make sure only superadmin can create superadmin
    $json = viewerPostToken("/api/user?arkimeRegressionUser=testsuperadmin", '{"userId": "sac-testsuperadmin1", "userName": "testsuperadmin1", "enabled":true, "password":"password", "roles": ["superAdmin"]}', $saToken);
    eq_or_diff($json, from_json('{"text": "User created succesfully", "success": true}'));

    $json = viewerPostToken("/api/user?arkimeRegressionUser=testusersadmin", '{"userId": "sac-testsuperadmin2", "userName": "testsuperadmin2", "enabled":true, "password":"password", "roles": ["superAdmin"]}', $uaToken);
    eq_or_diff($json, from_json('{"text": "Can not create superAdmin unless you are superAdmin", "success": false}'));


# Add super admin - only superadmin can add
    $json = viewerPostToken("/api/user/testuser?arkimeRegressionUser=testusersadmin", '{"userName": "testuser", "enabled":true, "password":"password", "roles": ["superAdmin"]}', $uaToken);
    eq_or_diff($json, from_json('{"text": "Can not modify superAdmin unless you are superAdmin", "success": false}'));

    $json = viewerPostToken("/api/user/testuser?arkimeRegressionUser=testsuperadmin", '{"userName": "testuser", "enabled":true, "password":"password", "roles": ["superAdmin"]}', $saToken);
    eq_or_diff($json, from_json('{"text": "User testuser updated successfully", "success": true}'));


# Remove super admin - only superadmin can remove
    $json = viewerPostToken("/api/user/testsuperadmin?arkimeRegressionUser=testusersadmin", '{"userName": "testsuperadmin", "enabled":true, "password":"password", "roles": ["arkimeAdmin"]}', $uaToken);
    eq_or_diff($json, from_json('{"text": "Can not disable superAdmin unless you are superAdmin", "success": false}'));

    $json = viewerPostToken("/api/user/testuser?arkimeRegressionUser=testusersadmin", '{"userName": "testuser", "enabled":true, "password":"password", "roles": ["arkimeAdmin"]}', $uaToken);
    eq_or_diff($json, from_json('{"text": "Can not disable superAdmin unless you are superAdmin", "success": false}'));

    $json = viewerPostToken("/api/user/testuser?arkimeRegressionUser=testsuperadmin", '{"userName": "testuser", "enabled":true, "password":"password", "roles": ["arkimeAdmin"]}', $saToken);
    eq_or_diff($json, from_json('{"text": "User testuser updated successfully", "success": true}'));


# clean old users
    clearIndex("tests_users");
