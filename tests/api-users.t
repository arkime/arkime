use Test::More tests => 104;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $pwd = "*/pcap";

    my $token = getTokenCookie();
    my $token2 = getTokenCookie2();
    my $token3 = getTokenCookie('test1');

# users
    my $users = viewerPost("/user/list", "");
    is (@{$users->{data}}, 0, "Empty users table");

# Add User 1
    my $json = viewerPostToken("/user/create", '{"userId": "test1", "userName": "UserName", "enabled":true, "password":"password"}', $token);

    $users = viewerPost("/user/list?molochRegressionUser=notadmin", "");
    eq_or_diff($users, from_json('{"text": "You do not have permission to access this resource", "success": false}'));

    $users = viewerPost("/user/list", "");
    is (@{$users->{data}}, 1, "Check add #1");
    is (!exists $users->{data}->[0]->{lastUsed}, 1, "last used doesn't exist #1");
    eq_or_diff($users->{data}->[0], from_json('{"createEnabled": false, "userId": "test1", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName", "id": "test1", "emailSearch": false, "enabled": true, "webEnabled": false, "packetSearch": false, "welcomeMsgNum": 0, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false}', {relaxed => 1}), "Test User Add", { context => 3 });


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

    eq_or_diff($users->{data}->[0], from_json('{"createEnabled": false, "userId": "test1", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName", "id": "test1", "emailSearch": false, "enabled": true, "webEnabled": false, "packetSearch": false, "welcomeMsgNum": 0, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false}', {relaxed => 1}), "Test User Add", { context => 3 });


# Update User Server 1
    $json = viewerPostToken("/user/update", '{"userId":"test1", "userName":"UserNameUpdated", "removeEnabled":true, "headerAuthEnabled":true, "expression":"foo", "emailSearch":true, "webEnabled":true, "createEnabled":true, "packetSearch": false}', $token);

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

    eq_or_diff($users->{data}->[0], from_json('{"createEnabled": true, "userId": "test1", "removeEnabled": true, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated", "id": "test1", "emailSearch": true, "enabled": false, "webEnabled": true, "packetSearch": false, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "welcomeMsgNum": 0}', {relaxed => 1}), "Test User Update", { context => 3 });

    $users = viewerPost2("/user/list", "");
    is (@{$users->{data}}, 1, "Check Update #2");
    is (exists $users->{data}->[0]->{lastUsed}, 1, "last used exists #3");
    delete $users->{data}->[0]->{lastUsed};
    eq_or_diff($users->{data}->[0], from_json('{"createEnabled": true, "userId": "test1", "removeEnabled": true, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated", "id": "test1", "emailSearch": true, "enabled": false, "webEnabled": true, "packetSearch": false, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "welcomeMsgNum": 0}', {relaxed => 1}), "Test User Update", { context => 3 });

# Add User 2
    my $json = viewerPostToken2("/api/user", '{"userId": "test2", "userName": "UserName2", "enabled":true, "password":"password"}', $token2);

    $users = viewerPost("/api/users", "");
    is (@{$users->{data}}, 2, "Check second add #1");
    eq_or_diff($users->{data}->[1], from_json('{"createEnabled": false, "userId": "test2", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName2", "id": "test2", "emailSearch": false, "enabled": true, "webEnabled": false, "packetSearch": false, "welcomeMsgNum": 0, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false}', {relaxed => 1}), "Test User Add", { context => 3 });

    $users = viewerPost2("/api/users", "");
    is (@{$users->{data}}, 2, "Check second add #2");
    eq_or_diff($users->{data}->[1], from_json('{"createEnabled": false, "userId": "test2", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName2", "id": "test2", "emailSearch": false, "enabled": true, "webEnabled": false, "packetSearch": false, "welcomeMsgNum": 0, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false}', {relaxed => 1}), "Test User Add", { context => 3 });

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

    $users = viewerPost("/user/list", "start=0&length=100000");
    is (@{$users->{data}}, 0, "start=0&length=100000");
    is ($users->{recordsTotal}, 0);
    is ($users->{recordsFiltered}, 0);

# Update User Shared Server
    $json = viewerPostToken2("/user/update", '{"userId":"test2","userName":"UserNameUpdated2", "enabled":true, "removeEnabled":false, "headerAuthEnabled":true, "expression":"foo", "emailSearch":true, "webEnabled":true, "createEnabled":true, "packetSearch": false}', $token2);

    $users = viewerPost("/user/list", "");
    is (@{$users->{data}}, 2, "Check second Update #1");
    eq_or_diff($users->{data}->[1], from_json('{"createEnabled": true, "userId": "test2", "removeEnabled": false, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated2", "id": "test2", "emailSearch": true, "enabled": true, "webEnabled": true, "packetSearch": false, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "welcomeMsgNum": 0}', {relaxed => 1}), "Test User Update", { context => 3 });

    $users = viewerPost2("/user/list", "");
    is (@{$users->{data}}, 2, "Check second Update #2");
    eq_or_diff($users->{data}->[1], from_json('{"createEnabled": true, "userId": "test2", "removeEnabled": false, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated2", "id": "test2", "emailSearch": true, "enabled": true, "webEnabled": true, "packetSearch": false, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "welcomeMsgNum": 0}', {relaxed => 1}), "Test User Update", { context => 3 });

# Reverse settings
    $json = viewerPostToken2("/user/update", '{"userId":"test2","userName":"UserNameUpdated3", "enabled":false, "removeEnabled":true, "headerAuthEnabled":false, "expression":"foo3", "emailSearch":false, "webEnabled":false, "createEnabled": false, "packetSearch": false}', $token2);

    $users = viewerPost("/user/list", "");
    is (@{$users->{data}}, 2, "Check second Update #1");
    eq_or_diff($users->{data}->[1], from_json('{"createEnabled": false, "userId": "test2", "removeEnabled": true, "expression": "foo3", "headerAuthEnabled": false, "userName": "UserNameUpdated3", "id": "test2", "emailSearch": false, "enabled": false, "webEnabled": false, "packetSearch": false, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "welcomeMsgNum": 0}', {relaxed => 1}), "Test User Update", { context => 3 });

    $users = viewerPost2("/user/list", "");
    is (@{$users->{data}}, 2, "Check second Update #2");
    eq_or_diff($users->{data}->[1], from_json('{"createEnabled": false, "userId": "test2", "removeEnabled": true, "expression": "foo3", "headerAuthEnabled": false, "userName": "UserNameUpdated3", "id": "test2", "emailSearch": false, "enabled": false, "webEnabled": false, "packetSearch": false, "disablePcapDownload": false, "hideFiles": false, "hidePcap": false, "hideStats": false, "welcomeMsgNum": 0}', {relaxed => 1}), "Test User Update", { context => 3 });

# Column
    my $info = viewerGet("/user/columns?molochRegressionUser=test1");
    eq_or_diff($info, from_json("[]"), "column: empty");

    $info = viewerPostToken("/user/columns/create?molochRegressionUser=test1", '{"name": "column1", "columns": ["a1","dstIp"], "order": [["lp", "asc"]]}', $test1Token);
    ok($info->{success}, "column: create success");
    is($info->{name}, "column1", "column: create name");

    $info = viewerPostToken("/api/user/column?molochRegressionUser=test1", '{"name": "column2", "columns": ["a1","dstIp"], "order": [["lp", "asc"]]}', $test1Token);
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

# views

    # No views
    $info = viewerGet("/user/views?molochRegressionUser=test1");
    eq_or_diff($info, from_json("{}"), "view: empty");

    # Create view and make sure we get it
    $info = viewerPostToken("/user/views/create?molochRegressionUser=test1", '{"name": "view1", "expression": "ip == 1.2.3.4"}', $test1Token);
    ok($info->{success}, "view: create success");
    is($info->{viewName}, "view1", "view: create name");
    eq_or_diff($info->{view}, from_json('{"expression":"ip == 1.2.3.4","user":"test1","shared":false}'), "view: 1 item");

    $info = viewerGet("/user/views?molochRegressionUser=test1");
    eq_or_diff($info, from_json('{"view1":{"expression":"ip == 1.2.3.4","user":"test1","shared":false}}'), "view: 1 item");


    # Fail duplicate create
    $info = viewerPostToken("/api/user/view?molochRegressionUser=test1", '{"name": "view1", "expression": "ip == 1.2.3.5"}', $test1Token);
    ok(!$info->{success}, "view: create failure with same view name");


    # As admin see test1 views
    $info = viewerGet("/api/user/views?molochRegressionUser=anonymous&userId=test1");
    eq_or_diff($info, from_json('{"view1":{"expression":"ip == 1.2.3.4","user":"test1","shared":false}}'), "view: 1 item admin");


    # Fail delete wrong name
    $info = viewerPostToken("/user/views/delete?molochRegressionUser=test1", '{"name":"fred"}', $test1Token);
    ok(!$info->{success}, "view: delete not found");


    # As see test1 views again
    $info = viewerGet("/user/views?molochRegressionUser=test1");
    eq_or_diff($info, from_json('{"view1":{"expression":"ip == 1.2.3.4","user":"test1","shared":false}}'), "view: 1 item");



    # Share the test1 view
    $info = viewerPostToken("/user/views/toggleShare?molochRegressionUser=test1", '{"expression":"ip == 1.2.3.7","user":"test1","shared":true,"name":"view1"}', $test1Token);
    ok($info->{success}, "view: share");

    # Make sure test1 view is shared
    $info = viewerGet("/user/views?molochRegressionUser=test1");
    eq_or_diff($info, from_json('{"view1":{"expression":"ip == 1.2.3.4","user":"test1","shared":true}}'), "view: 1 shared item");

    # Unshare the test1 view
    $info = viewerPostToken("/user/views/toggleShare?molochRegressionUser=test1", '{"expression":"ip == 1.2.3.8","user":"test1","shared":false,"name":"view1"}', $test1Token);
    ok($info->{success}, "view: unshare");

    $info = viewerDeleteToken("/api/user/view/view1?molochRegressionUser=test1", $test1Token);
    ok($info->{success}, "view: delete found");

    $info = viewerPostToken("/user/views/create?molochRegressionUser=test1", '{"name": "view2", "expression": "ip == 1.2.3.10", "shared": true}', $test1Token);
    ok($info->{success}, "view2: create success");
    is($info->{viewName}, "view2", "view2: create name");
    ok($info->{view}->{shared}, "view2: create shared");
    eq_or_diff($info->{view}, from_json('{"expression":"ip == 1.2.3.10","user":"test1","shared":true}'), "view: 2 item");

    $info = viewerPostToken("/user/views/create?molochRegressionUser=test1", '{"name": "view2", "expression": "ip == 1.2.3.4", "shared": true}', $test1Token);
    ok(!$info->{success}, "view2: create failure with same shared view name");

    $info = viewerPostToken("/api/user/view/view2/toggleshare?molochRegressionUser=test1", '{"expression":"ip == 1.2.3.4","user":"test1","shared":false,"name":"view2"}', $test1Token);
    ok($info->{success}, "view2: unshare");
    $info = viewerPostToken("/user/views/delete?molochRegressionUser=test1", '{"expression":"ip == 1.2.3.4","user":"test1","shared":false,"name":"view2"}', $test1Token);

    $info = viewerGet("/user/views?molochRegressionUser=test1");
    eq_or_diff($info, from_json("{}"), "view: empty");

    $info = viewerPutToken("/api/user/view/view2?molochRegressionUser=test1", '{"expression":"ip == 1.2.3.4","user":"test1","shared":false,"name":"view2"}', $test1Token);
    ok($info->{success}, "view: update");

# Messages
    $info = viewerPutToken("/user/test1/acknowledgeMsg", '{"msgNum":2}', $token2);
    ok(!$info->{success}, "can't update welcome message number for another user");
    $info = viewerPutToken("/api/user/test1/acknowledge?molochRegressionUser=test1", '{"msgNum":2}', $token3);
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

    $json = viewerGet("/api/valueactions?molochRegressionUser=test2");
    eq_or_diff($json, from_json('{"text": "You do not have permission to access this resource", "success": false}'), 'test2 valueActions');

# state tests
    $json = viewerPostToken("/api/user/state/state1?molochRegressionUser=test1", '{"order":"test","visibleHeaders":["firstPacket","lastPacket","src","srcPort","dst","dstPort","totPackets","dbby","node"]}', $test1Token);
    ok($json->{success}, "state create success");
    $json = viewerPostToken("/api/user/state/state1?molochRegressionUser=test1", '{"order":[["firstPacket","asc"]],"visibleHeaders":["firstPacket","lastPacket"]}', $test1Token);
    ok($json->{success}, "state update success");
    $json = viewerGetToken("/api/user/state/state1?molochRegressionUser=test1", $test1Token);
    eq_or_diff($json->{order}, from_json('[["firstPacket","asc"]]'), "share fetch success");
    eq_or_diff($json->{visibleHeaders}, from_json('["firstPacket","lastPacket"]'), "share fetch success");

# Delete Users
    $json = viewerDeleteToken("/api/user/test1", $token);
    $json = viewerPostToken2("/user/delete", "userId=test2", $token2);
    esGet("/_refresh");
    $users = viewerPost("/user/list", "");
    is (@{$users->{data}}, 0, "Removed user #1");
    $users = viewerPost2("/user/list", "");
    is (@{$users->{data}}, 0, "Removed user #2");
