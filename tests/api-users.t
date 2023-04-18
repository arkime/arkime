use Test::More tests => 140;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $json;

# clean old users
    esPost("/tests_users/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }', 1);

# Get tokens
    my $token = getTokenCookie();
    my $token2 = getTokenCookie2();
    my $test1Token = getTokenCookie('test1');
    my $test2Token = getTokenCookie('test2');
    my $superAdminToken = getTokenCookie('superAdmin');

# clean old crons
    esPost("/tests_queries/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }');

# users
    my $users = viewerPost("/user/list", "");
    is (@{$users->{data}}, 0, "Empty users table");

# Can't create system rule
    $json = viewerPostToken("/user/create", '{"userId": "usersAdmin", "userName": "UserName", "enabled":true, "password":"password"}', $token);
    eq_or_diff($json, from_json('{"text": "User ID can\'t be a system role id", "success": false}'));

    $json = viewerPostToken("/user/update", '{"userId": "usersAdmin", "userName": "UserName", "enabled":true, "password":"password", "roles":["superAdmin"]}', $token);
    eq_or_diff($json, from_json('{"text": "User ID can\'t be a system role id", "success": false}'));

    $json = viewerPostToken("/user/update", '{"userId": "usersAdmin\u001b", "userName": "UserName", "enabled":true, "password":"password", "roles":["arkimeUser"]}', $token);
    eq_or_diff($json, from_json('{"text": "User not found", "success": false}'));

# Create Missing/Emptry fields
    $json = viewerPostToken("/user/create", '{"userName": "UserName", "enabled":true, "password":"password"}', $token);
    eq_or_diff($json, from_json('{"text": "Missing/Empty required fields", "success": false}'));

    $json = viewerPostToken("/user/create", '{"userId": "", "userName": "UserName", "enabled":true, "password":"password"}', $token);
    eq_or_diff($json, from_json('{"text": "Missing/Empty required fields", "success": false}'));

    $json = viewerPostToken("/user/create", '{"userId": "<script>", "userName": "UserName", "enabled":true, "password":"password"}', $token);
    eq_or_diff($json, from_json('{"text": "User ID must be word characters", "success": false}'));

    $json = viewerPostToken("/user/create", '{"userId": "test1", "enabled":true, "password":"password"}', $token);
    eq_or_diff($json, from_json('{"text": "Missing/Empty required fields", "success": false}'));

    $json = viewerPostToken("/user/create", '{"userId": "test1", "userName": "", "enabled":true, "password":"password"}', $token);
    eq_or_diff($json, from_json('{"text": "Missing/Empty required fields", "success": false}'));

    $json = viewerPostToken("/user/create", '{"userId": "test1", "userName": " ", "enabled":true, "password":"password"}', $token);
    eq_or_diff($json, from_json('{"text": "Username can not be empty", "success": false}'));

    $json = viewerPostToken("/user/create", '{"userId": "test1", "userName": "UserName", "enabled":true}', $token);
    eq_or_diff($json, from_json('{"text": "Password needs to be at least 3 characters", "success": false}'));

    $json = viewerPostToken("/user/create", '{"userId": "test1", "userName": "UserName", "enabled":true, "password":""}', $token);
    eq_or_diff($json, from_json('{"text": "Password needs to be at least 3 characters", "success": false}'));

    $json = viewerPostToken("/user/create", '{"userId": "test1", "userName": "UserName", "enabled":true, "password":"ab"}', $token);
    eq_or_diff($json, from_json('{"text": "Password needs to be at least 3 characters", "success": false}'));

    $json = viewerPostToken("/user/create", '{"userId": "test1", "userName": "UserName", "enabled":true, "password":"abc", "expression": false}', $token);
    eq_or_diff($json, from_json('{"text": "Expression must be a string when present", "success": false}'));

# Add User 1
    $json = viewerPostToken("/user/create", '{"userId": "test1", "userName": "UserName", "enabled":true, "password":"password"}', $token);
    eq_or_diff($json, from_json('{"text": "User created succesfully", "success": true}'));

    $users = viewerPost("/user/list?molochRegressionUser=notadmin", "");
    eq_or_diff($users, from_json('{"text": "You do not have permission to access this resource", "success": false}'));

    $users = viewerPost("/user/list", "");
    is (@{$users->{data}}, 1, "Check add #1");

    eq_or_diff($users->{data}->[0], from_json('{"roles": [], "userId": "test1", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName", "id": "test1", "emailSearch": false, "enabled": true, "webEnabled": false, "packetSearch": false, "welcomeMsgNum": 0, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "lastUsed": 0, "roleAssigners": []}', {relaxed => 1}), "Test User Add", { context => 3 });


    # This will set a lastUsed time, make sure DB is updated with sleep
    my $test1Token = getTokenCookie("test1");
    sleep(1);
    esGet("/_flush");
    esGet("/_refresh");

    $users = viewerPost2("/user/list", "");
    is (@{$users->{data}}, 1, "Check add #2");

    is (exists $users->{data}->[0]->{lastUsed}, 1, "last used exists #1");
    my $lastUsedTimestamp1 = $users->{data}->[0]->{lastUsed};
    delete $users->{data}->[0]->{lastUsed};

    eq_or_diff($users->{data}->[0], from_json('{"roles": [], "userId": "test1", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName", "id": "test1", "emailSearch": false, "enabled": true, "webEnabled": false, "packetSearch": false, "welcomeMsgNum": 0, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "roleAssigners": []}', {relaxed => 1}), "Test User Add", { context => 3 });

# Check appinfo works
    $json = viewerGetToken("/api/appInfo", $token);
    eq_or_diff(sort($json->{roles}), from_json('["arkimeAdmin", "arkimeUser", "cont3xtAdmin", "cont3xtUser", "parliamentAdmin", "parliamentUser", "superAdmin", "usersAdmin", "wiseAdmin", "wiseUser"]'));
    my @roles = sort @{$json->{user}->{roles}};
    eq_or_diff(\@roles, from_json('["arkimeAdmin", "arkimeUser", "cont3xtUser", "parliamentUser", "usersAdmin", "wiseUser"]'));

# Can we create superAdmin
    $json = viewerPostToken("/user/create", '{"userId": "testSuper", "userName": "SUserName", "enabled":true, "password":"password", "roles":["superAdmin"]}', $token);
    eq_or_diff($json, from_json('{"text": "Can not create superAdmin unless you are superAdmin", "success": false}'));

# Can we update superAdmin
    $json = viewerPostToken("/user/update", '{"userId":"test1", "userName":"UserNameUpdated", "removeEnabled":true, "headerAuthEnabled":true, "expression":"foo", "emailSearch":true, "webEnabled":true, "roles":["superAdmin"], "packetSearch": false}', $token);
    eq_or_diff($json, from_json('{"text": "Can not modify superAdmin unless you are superAdmin", "success": false}'));

    $json = viewerPostToken("/user/update", '{"userId":"test1", "userName":"UserNameUpdated", "removeEnabled":true, "headerAuthEnabled":true, "expression":"foo", "emailSearch":true, "webEnabled":true, "roles":["usersAdmin"], "packetSearch": false}', $token);
    eq_or_diff($json, from_json('{"text": "Only superAdmin user can enable Admin roles on a user", "success": false}'));
# Update User Server 1
    $json = viewerPostToken("/user/update?molochRegressionUser=superAdmin", '{"userId":"test1", "userName":"UserNameUpdated", "removeEnabled":true, "headerAuthEnabled":true, "expression":"foo", "emailSearch":true, "webEnabled":true, "roles":["usersAdmin"], "packetSearch": false}', $superAdminToken);
    eq_or_diff($json, from_json('{"text": "User test1 updated successfully", "success": true}'));

    $users = viewerPost("/user/list?molochRegressionUser=test1", "");
    is (@{$users->{data}}, 1, "Check Update #1");

    # Force last used to be increased
    esGet("/_flush");
    esGet("/_refresh");
    sleep(2);

    $users = viewerPost("/user/list", "");
    is (exists $users->{data}->[0]->{lastUsed}, 1, "last used exists #2");
    my $lastUsedTimestamp2 = $users->{data}->[0]->{lastUsed};
    cmp_ok($lastUsedTimestamp1, '<', $lastUsedTimestamp2, "last used updated");
    delete $users->{data}->[0]->{lastUsed};

    eq_or_diff($users->{data}->[0], from_json('{"roles": ["usersAdmin"], "userId": "test1", "removeEnabled": true, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated", "id": "test1", "emailSearch": true, "enabled": false, "webEnabled": true, "packetSearch": false, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "welcomeMsgNum": 0, "roleAssigners": []}', {relaxed => 1}), "Test User Update", { context => 3 });

    $users = viewerPost2("/user/list", "");
    is (@{$users->{data}}, 1, "Check Update #2");
    is (exists $users->{data}->[0]->{lastUsed}, 1, "last used exists #3");
    delete $users->{data}->[0]->{lastUsed};
    eq_or_diff($users->{data}->[0], from_json('{"roles": ["usersAdmin"], "userId": "test1", "removeEnabled": true, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated", "id": "test1", "emailSearch": true, "enabled": false, "webEnabled": true, "packetSearch": false, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "welcomeMsgNum": 0, "roleAssigners": []}', {relaxed => 1}), "Test User Update", { context => 3 });

# update user settings
    $json = viewerPostToken("/api/user/settings?molochRegressionUser=test1", '{"logo":"testlogo.png","__proto":{"bad":"stuff"}}', $test1Token);
    eq_or_diff($json, from_json('{"text": "Updated user settings successfully", "success": true}'));

    $json = viewerGetToken("/api/user/settings?molochRegressionUser=test1", $test1Token);
    ok(!exists $json->{__proto__}, "no prototype pollution");
    eq_or_diff($json->{logo}, "testlogo.png");

# Add User 2
    my $json = viewerPostToken2("/api/user", '{"userId": "test2", "userName": "UserName2", "enabled":true, "password":"password"}', $token2);

    $users = viewerPost("/api/users", "");
    is (@{$users->{data}}, 2, "Check second add #1");
    eq_or_diff($users->{data}->[1], from_json('{"roles": [], "userId": "test2", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName2", "id": "test2", "emailSearch": false, "enabled": true, "webEnabled": false, "packetSearch": false, "welcomeMsgNum": 0, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "lastUsed": 0, "roleAssigners": []}', {relaxed => 1}), "Test User Add", { context => 3 });

    $users = viewerPost2("/api/users", "");
    is (@{$users->{data}}, 2, "Check second add #2");
    eq_or_diff($users->{data}->[1], from_json('{"roles": [], "userId": "test2", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName2", "id": "test2", "emailSearch": false, "enabled": true, "webEnabled": false, "packetSearch": false, "welcomeMsgNum": 0, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "lastUsed": 0, "roleAssigners": []}', {relaxed => 1}), "Test User Add", { context => 3 });

# Filter
    $users = viewerPost("/user/list", "filter=test");
    is (@{$users->{data}}, 2, "filter both");
    is ($users->{recordsTotal}, 2);
    is ($users->{recordsFiltered}, 2);

    $users = viewerPost("/user/list", "filter=test1");
    is (@{$users->{data}}, 1, "filter one");
    is ($users->{recordsTotal}, 2);
    is ($users->{recordsFiltered}, 1);

# start, length
    $users = viewerPost("/user/list", "start=0&length=2");
    is (@{$users->{data}}, 2, "start=0&length=2");
    is ($users->{recordsTotal}, 2);
    is ($users->{recordsFiltered}, 2);

    $users = viewerPost("/user/list", "start=1&length=2");
    is (@{$users->{data}}, 1, "start=1&length=2");
    is ($users->{recordsTotal}, 2);
    is ($users->{recordsFiltered}, 2);

    $users = viewerPost("/user/list", "start=0&length=1");
    is (@{$users->{data}}, 1, "start=1&length=1");
    is ($users->{recordsTotal}, 2);
    is ($users->{recordsFiltered}, 2);

    $users = viewerPost("/user/list", "start=666&length=100000");
    is (@{$users->{data}}, 0, "start=0&length=100000");
    is ($users->{recordsTotal}, 0);
    is ($users->{recordsFiltered}, 0);

# Update User Shared Server
    $json = viewerPostToken2("/user/update", '{"userId":"test2","userName":"UserNameUpdated2", "enabled":true, "removeEnabled":false, "headerAuthEnabled":true, "expression":"foo", "emailSearch":true, "webEnabled":true, "roles": [], "packetSearch": false}', $token2);

    $users = viewerPost("/user/list", "");
    is (@{$users->{data}}, 2, "Check second Update #1");
    delete $users->{data}->[1]->{lastUsed};
    eq_or_diff($users->{data}->[1], from_json('{"roles": [], "userId": "test2", "removeEnabled": false, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated2", "id": "test2", "emailSearch": true, "enabled": true, "webEnabled": true, "packetSearch": false, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "welcomeMsgNum": 0, "roleAssigners": []}', {relaxed => 1}), "Test User Update", { context => 3 });

    $users = viewerPost2("/user/list", "");
    is (@{$users->{data}}, 2, "Check second Update #2");
    delete $users->{data}->[1]->{lastUsed};
    eq_or_diff($users->{data}->[1], from_json('{"roles": [], "userId": "test2", "removeEnabled": false, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated2", "id": "test2", "emailSearch": true, "enabled": true, "webEnabled": true, "packetSearch": false, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "welcomeMsgNum": 0, "roleAssigners": []}', {relaxed => 1}), "Test User Update", { context => 3 });

# Reverse settings
    $json = viewerPostToken2("/user/update", '{"userId":"test2","userName":"UserNameUpdated3", "enabled":false, "removeEnabled":true, "headerAuthEnabled":false, "expression":"", "emailSearch":false, "webEnabled":false, "roles": [], "packetSearch": false}', $token2);

    $users = viewerPost("/user/list", "");
    is (@{$users->{data}}, 2, "Check second Update #1");
    delete $users->{data}->[1]->{lastUsed};
    eq_or_diff($users->{data}->[1], from_json('{"roles": [], "userId": "test2", "removeEnabled": true, "expression": "", "headerAuthEnabled": false, "userName": "UserNameUpdated3", "id": "test2", "emailSearch": false, "enabled": false, "webEnabled": false, "packetSearch": false, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "welcomeMsgNum": 0, "roleAssigners": []}', {relaxed => 1}), "Test User Update", { context => 3 });

    $users = viewerPost2("/user/list", "");
    is (@{$users->{data}}, 2, "Check second Update #2");
    delete $users->{data}->[1]->{lastUsed};
    eq_or_diff($users->{data}->[1], from_json('{"roles": [], "userId": "test2", "removeEnabled": true, "expression": "", "headerAuthEnabled": false, "userName": "UserNameUpdated3", "id": "test2", "emailSearch": false, "enabled": false, "webEnabled": false, "packetSearch": false, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "welcomeMsgNum": 0, "roleAssigners": []}', {relaxed => 1}), "Test User Update", { context => 3 });

# Column
    my $info = viewerGet("/user/columns?molochRegressionUser=test1");
    eq_or_diff($info, from_json("[]"), "column: empty");

    $info = viewerPostToken("/user/columns/create?molochRegressionUser=test1", '{"name": "column1", "columns": ["srcIp","dstIp"], "order": [["lastPacket", "asc"]]}', $test1Token);
    ok($info->{success}, "column: create success");
    is($info->{name}, "column1", "column: create name");

    $info = viewerPostToken("/api/user/column?molochRegressionUser=test1", '{"name": "column2", "columns": ["srcIp","dstIp"], "order": [["lastPacket", "asc"]]}', $test1Token);
    ok($info->{success}, "column: create success");
    is($info->{name}, "column2", "column: create name");

    $info = viewerGet("/api/user/columns?molochRegressionUser=test1");
    eq_or_diff($info, from_json('[{"name":"column1","order":[["lastPacket","asc"]],"columns":["srcIp","dstIp"]},{"name":"column2","order":[["lastPacket","asc"]],"columns":["srcIp","dstIp"]}]'), "column: 1 item");

    $info = viewerGet("/user/columns?molochRegressionUser=anonymous&userId=test1");
    eq_or_diff($info, from_json('[{"name":"column1","order":[["lastPacket","asc"]],"columns":["srcIp","dstIp"]},{"name":"column2","order":[["lastPacket","asc"]],"columns":["srcIp","dstIp"]}]'), "column: 1 item admin");

    $info = viewerPostToken("/user/columns/delete?molochRegressionUser=test1", 'name=fred', $test1Token);
    ok(! $info->{success}, "column: delete not found");

    $info = viewerGet("/user/columns?molochRegressionUser=test1");
    eq_or_diff($info, from_json('[{"name":"column1","order":[["lastPacket","asc"]],"columns":["srcIp","dstIp"]},{"name":"column2","order":[["lastPacket","asc"]],"columns":["srcIp","dstIp"]}]'), "column: 1 item");

    $info = viewerPutToken("/api/user/column/column1?molochRegressionUser=test1", '{"name": "column1", "columns": ["srcIp","dstIp","info"], "order": [["lastPacket","asc"]]}', $test1Token);
    ok($info->{success}, "column: update");

    $info = viewerDeleteToken("/api/user/column/column1?molochRegressionUser=test1", $test1Token);
    ok($info->{success}, "column: delete found");

    $info = viewerPostToken("/user/columns/delete?molochRegressionUser=test1", 'name=column2', $test1Token);
    ok($info->{success}, "column: delete found");

    $info = viewerGet("/user/columns?molochRegressionUser=test1");
    eq_or_diff($info, from_json("[]"), "column: empty");


# Current
    $info = viewerGet("/api/user?molochRegressionUser=test1");
    ok(!exists $info->{passStore}, "current: no passtore");

# spiview fields
    $info = viewerGet("/user/spiview/fields?molochRegressionUser=test1");
    eq_or_diff($info, from_json("[]"), "spiview fields: empty");

    $info = viewerPostToken("/api/user/spiview?molochRegressionUser=test1", '{"name": "sfields1", "fields": ["srcIp","dstIp"]}', $test1Token);
    ok($info->{success}, "spiview fields: create success");
    is($info->{name}, "sfields1", "spiview fields: create name");

    $info = viewerGet("/api/user/spiview?molochRegressionUser=test1");
    eq_or_diff($info, from_json('[{"name":"sfields1","fields":["srcIp","dstIp"]}]'), "spiview fields: 1 item");

    $info = viewerGet("/user/spiview/fields?molochRegressionUser=anonymous&userId=test1");
    eq_or_diff($info, from_json('[{"name":"sfields1","fields":["srcIp","dstIp"]}]'), "spiview fields: 1 item admin");

    $info = viewerPutToken("/api/user/spiview/sfields1?molochRegressionUser=test1", '{"name": "sfields1", "fields": ["srcIp","dstIp","node"]}', $test1Token);
    ok($info->{success}, "spiview fields: update success");

    $info = viewerPostToken("/user/spiview/fields/delete?molochRegressionUser=test1", 'name=fred', $test1Token);
    ok(!$info->{success}, "spiview fields: delete not found");

    $info = viewerGet("/user/spiview/fields?molochRegressionUser=test1");
    eq_or_diff($info, from_json('[{"name":"sfields1","fields":["srcIp","dstIp","node"]}]'), "spiview fields: 1 item");

    $info = viewerDeleteToken("/api/user/spiview/sfields1?molochRegressionUser=test1", $test1Token);
    ok($info->{success}, "spiview fields: delete found");

    $info = viewerGet("/user/spiview/fields?molochRegressionUser=test1");
    eq_or_diff($info, from_json("[]"), "spiview fields: empty");

# Messages
    $info = viewerPutToken("/user/test1/acknowledgeMsg", '{"msgNum":2}', $token2);
    ok(!$info->{success}, "can't update welcome message number for another user");

    $info = viewerPutToken("/api/user/test1/acknowledge?molochRegressionUser=test1", '{}', $test1Token);
    eq_or_diff($info, from_json('{"text": "Message number required", "success": false}'));

    $info = viewerPutToken("/api/user/test1/acknowledge?molochRegressionUser=test1", '{"msgNum":"foo"}', $test1Token);
    eq_or_diff($info, from_json('{"text": "welcomeMsgNum is not integer", "success": false}'));

    $info = viewerPutToken("/api/user/test1/acknowledge?molochRegressionUser=test1", '{"msgNum":2}', $test1Token);
    ok($info->{success}, "update welcome message number");

    $info = viewerGet("/user/current?molochRegressionUser=test1");
    eq_or_diff($info->{welcomeMsgNum}, 2, "welcome message number is correct");

# user time limit
    $json = viewerPostToken2("/api/user/test2", '{"timeLimit":"72"}', $token2);
    $users = viewerPost("/user/list", "");
    eq_or_diff($users->{data}->[1]->{timeLimit}, 72, "time limit updated");
    $json = viewerGet("/sessions.json?molochRegressionUser=test2&date=-1");
    eq_or_diff($json->{error}, "User time limit (72 hours) exceeded", "user can't exceed their time limit");
    $json = viewerGet("/api/sessions?molochRegressionUser=test2&date=72");
    is (exists $json->{data}, 1, "user can make a query within their time range");

# valueActions tests
    $json = viewerGet("/api/valueactions?molochRegressionUser=test1");
    eq_or_diff($json->{reverseDNS}, from_json('{"url": "api/reversedns?ip=%TEXT%", "name": "Get Reverse DNS", "actionType": "fetch", "category": "ip"}'), 'test1 valueActions');
    eq_or_diff($json->{USERTEST}, from_json('{"url": "https://example.com", "name": "usertest", "category": "url"}'), 'test1 valueActions');

    # not web enabled
    $json = viewerGet("/api/valueactions?molochRegressionUser=test2");
    eq_or_diff($json, from_json('{"text": "You do not have permission to access this resource", "success": false}'), 'test2 valueActions');

    $json = viewerGet("/api/valueactions?molochRegressionUser=test100");
    ok(! exists $json->{USERTEST});

    $json = viewerGet("/api/valueactions?molochRegressionUser=test101");
    ok(! exists $json->{USERTEST});

# fieldActions tests
    $json = viewerGet("/api/fieldActions?molochRegressionUser=test1");
    eq_or_diff($json->{ASDF}, from_json('{"url": "https://www.asdf.com?expression=%EXPRESSION%&date=%DATE%&field=%FIELD%&dbField=%DBFIELD%", "name": "Field Action %FIELDNAME%!", "category": "ip"}'), 'fetches field actions');

    # not web enabled
    $json = viewerGet("/api/fieldActions?molochRegressionUser=test2");
    eq_or_diff($json, from_json('{"text": "You do not have permission to access this resource", "success": false}'), 'user cannot access field action');

    # not a user:
    $json = viewerGet("/api/fieldActions?molochRegressionUser=test100");
    eq_or_diff($json, from_json('{}'), 'not a fieldActions user:');

    # notUser:
    $json = viewerGet("/api/fieldActions?molochRegressionUser=test101");
    eq_or_diff($json, from_json('{}'), 'notUser fieldActions');

# state tests
    $json = viewerPostToken("/api/user/state/state1?molochRegressionUser=test1", '{"order":"test","visibleHeaders":["firstPacket","lastPacket","src","srcPort","dst","dstPort","totPackets","dbby","node"]}', $test1Token);
    ok($json->{success}, "state create success");
    $json = viewerPostToken("/api/user/state/state1?molochRegressionUser=test1", '{"order":[["firstPacket","asc"]],"visibleHeaders":["firstPacket","lastPacket"]}', $test1Token);
    ok($json->{success}, "state update success");
    $json = viewerGetToken("/api/user/state/state1?molochRegressionUser=test1", $test1Token);
    eq_or_diff($json->{order}, from_json('[["firstPacket","asc"]]'), "share fetch success");
    eq_or_diff($json->{visibleHeaders}, from_json('["firstPacket","lastPacket"]'), "share fetch success");

# roles
    $json = viewerPostToken("/user/create", '{"userId": "role:test1", "userName": "UserName", "enabled":true, "roles":"bad"}', $token);
    eq_or_diff($json, from_json('{"text": "Roles field must be an array of strings", "success": false}'));

    $json = viewerPostToken("/user/create", '{"userId": "role:test1", "userName": "UserName", "enabled":true, "roles":[false]}', $token);
    eq_or_diff($json, from_json('{"text": "Roles field must be an array of strings", "success": false}'));

    $json = viewerPostToken("/user/create", '{"userId": "role:test1", "userName": "UserName", "enabled":true}', $token);
    eq_or_diff($json, from_json('{"text": "Role created succesfully", "success": true}'));

    $json = viewerPostToken("/user/create", '{"userId": "role:test2", "userName": "UserName", "enabled":true, "roles":["role:test1", "superAdmin"]}', $token);
    eq_or_diff($json, from_json('{"text": "User defined roles can\'t have superAdmin", "success": false}'));

    $json = viewerPostToken("/user/create", '{"userId": "role:test2", "userName": "UserName", "enabled":true, "roles":["role:test1", "usersAdmin"]}', $token);
    eq_or_diff($json, from_json('{"text": "User defined roles can\'t have a system Admin role", "success": false}'));

    $json = viewerPostToken("/user/create", '{"userId": "role:test2", "userName": "UserName", "enabled":true, "roles":["role:test1"]}', $token);
    eq_or_diff($json, from_json('{"text": "Role created succesfully", "success": true}'));

    $json = viewerPost("/user/list?molochRegressionUser=role:test1", "");
    eq_or_diff($json, from_json('{"text": "Can not authenticate with role", "success": false}'));

# role tests
    $json = viewerPostToken("/user/update", '{"userId": "role:test1", "roles":["superAdmin"]}', $token);
    eq_or_diff($json, from_json('{"text": "User defined roles can\'t have superAdmin", "success": false}'));

    $json = viewerPostToken("/user/update", '{"userId": "role:test1", "roles":["usersAdmin"]}', $token);
    eq_or_diff($json, from_json('{"text": "User defined roles can\'t have a system Admin role", "success": false}'));

    $json = viewerPostToken("/user/update", '{"userId": "role:test1", "roles":"usersAdmin"}', $token);
    eq_or_diff($json, from_json('{"text": "Roles field must be an array of strings", "success": false}'));

    $json = viewerPostToken("/user/update", '{"userId": "role:test1", "roles":[false]}', $token);
    eq_or_diff($json, from_json('{"text": "Roles field must be an array of strings", "success": false}'));

# role assigner
    $json = viewerPostToken("/user/update", '{"userId": "role:test1", "userName": "UserName", "enabled":true, "roleAssigners": "foo"}', $token);
    eq_or_diff($json, from_json('{"text": "roleAssigners field must be an array of strings", "success": false}'));

    $json = viewerPostToken("/user/update", '{"userId": "role:test1", "userName": "UserName", "enabled":true, "roleAssigners": [false]}', $token);
    eq_or_diff($json, from_json('{"text": "roleAssigners field must be an array of strings", "success": false}'));

    $json = viewerPostToken("/user/update", '{"userId": "role:test1", "userName": "UserName", "enabled":true, "roleAssigners": ["test1"]}', $token);
    eq_or_diff($json, from_json('{"text": "User role:test1 updated successfully", "success": true}'));

    $json = viewerPost("/user/list", "filter=role:test1");
    eq_or_diff($json->{data}->[0]->{roleAssigners}, from_json('["test1"]'));

    $json = viewerPostToken("/api/users/min?molochRegressionUser=test1", "", $test1Token);
    eq_or_diff($json, from_json('{"data":[{"userName":"UserNameUpdated","userId":"test1"},{"userId":"test2","userName":"UserNameUpdated3"}],"success":true}'));

    $json = viewerPostToken("/api/users/min?molochRegressionUser=test1", '{"roleId": "role:test1"}', $test1Token);
    eq_or_diff($json, from_json('{"data":[{"hasRole": false, "userName":"UserNameUpdated","userId":"test1"},{"hasRole": false, "userId":"test2","userName":"UserNameUpdated3"}],"success":true}'));

    $json = viewerPostToken("/api/users/min?molochRegressionUser=test2", "", $test2Token);
    eq_or_diff($json, from_json('{"data":[{"userName":"UserNameUpdated","userId":"test1"},{"userId":"test2","userName":"UserNameUpdated3"}],"success":true}'));

    $json = viewerPostToken("/api/users/min?molochRegressionUser=test2", '{"roleId": "role:test1"}', $test2Token);
    eq_or_diff($json, from_json('{"text": "You do not have permission to access this resource","success":false}'));

# Delete Users
    $json = viewerDeleteToken("/api/user/test1", $token);
    $json = viewerPostToken2("/user/delete", "userId=test2", $token2);
    $json = viewerPostToken2("/user/delete", "userId=role:test1", $token2);
    $json = viewerPostToken2("/user/delete", "userId=role:test2", $token2);
    esGet("/_refresh");
    $users = viewerPost("/user/list", "");
    is (@{$users->{data}}, 0, "Removed user #1");
    $users = viewerPost2("/user/list", "");
    is (@{$users->{data}}, 0, "Removed user #2");

#####
# Roles Tests - Some of these are repeats
my $es = "-o 'elasticsearch=$MolochTest::elasticsearch' -o 'usersElasticsearch=$MolochTest::elasticsearch' $ENV{INSECURE}";
system("cd ../viewer ; node addUser.js $es -c ../tests/config.test.ini -n testuser testsuperadmin testsuperadmin testsuperadmin --roles superAdmin");
system("cd ../viewer ; node addUser.js $es -c ../tests/config.test.ini -n testuser testusersadmin testusersadmin testusersadmin --roles usersAdmin");
system("cd ../viewer ; node addUser.js $es -c ../tests/config.test.ini -n testuser testuser testuser testuser");

my $saToken = getTokenCookie('testsuperadmin');
my $uaToken = getTokenCookie('testusersadmin');

# Make sure only superadmin can create superadmin
    $json = viewerPostToken("/user/create?molochRegressionUser=testsuperadmin", '{"userId": "testsuperadmin1", "userName": "testsuperadmin1", "enabled":true, "password":"password", "roles": ["superAdmin"]}', $saToken);
    eq_or_diff($json, from_json('{"text": "User created succesfully", "success": true}'));

    $json = viewerPostToken("/user/create?molochRegressionUser=testusersadmin", '{"userId": "testsuperadmin2", "userName": "testsuperadmin2", "enabled":true, "password":"password", "roles": ["superAdmin"]}', $uaToken);
    eq_or_diff($json, from_json('{"text": "Can not create superAdmin unless you are superAdmin", "success": false}'));


# Add super admin - only superadmin can add
    $json = viewerPostToken("/user/update?molochRegressionUser=testusersadmin", '{"userId": "testuser", "userName": "testuser", "enabled":true, "password":"password", "roles": ["superAdmin"]}', $uaToken);
    eq_or_diff($json, from_json('{"text": "Can not modify superAdmin unless you are superAdmin", "success": false}'));

    $json = viewerPostToken("/user/update?molochRegressionUser=testsuperadmin", '{"userId": "testuser", "userName": "testuser", "enabled":true, "password":"password", "roles": ["superAdmin"]}', $saToken);
    eq_or_diff($json, from_json('{"text": "User testuser updated successfully", "success": true}'));


# Remove super admin - only superadmin can remove
    $json = viewerPostToken("/user/update?molochRegressionUser=testusersadmin", '{"userId": "testsuperadmin", "userName": "testsuperadmin", "enabled":true, "password":"password", "roles": ["arkimeAdmin"]}', $uaToken);
    eq_or_diff($json, from_json('{"text": "Can not disable superAdmin unless you are superAdmin", "success": false}'));

    $json = viewerPostToken("/user/update?molochRegressionUser=testusersadmin", '{"userId": "testuser", "userName": "testuser", "enabled":true, "password":"password", "roles": ["arkimeAdmin"]}', $uaToken);
    eq_or_diff($json, from_json('{"text": "Can not disable superAdmin unless you are superAdmin", "success": false}'));

    $json = viewerPostToken("/user/update?molochRegressionUser=testsuperadmin", '{"userId": "testuser", "userName": "testuser", "enabled":true, "password":"password", "roles": ["arkimeAdmin"]}', $saToken);
    eq_or_diff($json, from_json('{"text": "User testuser updated successfully", "success": true}'));


# clean old users
    esPost("/tests_users/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }', 1);
