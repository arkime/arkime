use Test::More tests => 71;
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

# users
    my $users = viewerPost("/user/list", "");
    is (@{$users->{data}}, 0, "Empty users table");

# Add User 1
    my $json = viewerPostToken("/user/create", '{"userId": "test1", "userName": "UserName", "enabled":true, "password":"password"}', $token);

    $users = viewerPost("/user/list?molochRegressionUser=notadmin", "");
    eq_or_diff($users, from_json('{"text": "Need admin privileges", "success": false}'));

    my $test1Token = getTokenCookie("test1");

    $users = viewerPost("/user/list", "");
    is (@{$users->{data}}, 1, "Check add #1");
    eq_or_diff($users->{data}->[0], from_json('{"createEnabled": false, "userId": "test1", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName", "id": "test1", "emailSearch": false, "enabled": true, "webEnabled": false}', {relaxed => 1}), "Test User Add", { context => 3 });

    $users = viewerPost2("/user/list", "");
    is (@{$users->{data}}, 1, "Check add #2");
    eq_or_diff($users->{data}->[0], from_json('{"createEnabled": false, "userId": "test1", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName", "id": "test1", "emailSearch": false, "enabled": true, "webEnabled": false}', {relaxed => 1}), "Test User Add", { context => 3 });


# Update User Server 1
    $json = viewerPostToken("/user/update", '{"userId":"test1", "userName":"UserNameUpdated", "removeEnabled":true, "headerAuthEnabled":true, "expression":"foo", "emailSearch":true, "webEnabled":true, "createEnabled":true}', $token);

    $users = viewerPost("/user/list", "");
    is (@{$users->{data}}, 1, "Check Update #1");
    eq_or_diff($users->{data}->[0], from_json('{"createEnabled": true, "userId": "test1", "removeEnabled": true, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated", "id": "test1", "emailSearch": true, "enabled": false, "webEnabled": true}', {relaxed => 1}), "Test User Update", { context => 3 });

    $users = viewerPost2("/user/list", "");
    is (@{$users->{data}}, 1, "Check Update #2");
    eq_or_diff($users->{data}->[0], from_json('{"createEnabled": true, "userId": "test1", "removeEnabled": true, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated", "id": "test1", "emailSearch": true, "enabled": false, "webEnabled": true}', {relaxed => 1}), "Test User Update", { context => 3 });

# Add User 2
    my $json = viewerPostToken2("/user/create", '{"userId": "test2", "userName": "UserName2", "enabled":true, "password":"password"}', $token2);

    $users = viewerPost("/user/list", "");
    is (@{$users->{data}}, 2, "Check second add #1");
    eq_or_diff($users->{data}->[1], from_json('{"createEnabled": false, "userId": "test2", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName2", "id": "test2", "emailSearch": false, "enabled": true, "webEnabled": false}', {relaxed => 1}), "Test User Add", { context => 3 });

    $users = viewerPost2("/user/list", "");
    is (@{$users->{data}}, 2, "Check second add #2");
    eq_or_diff($users->{data}->[1], from_json('{"createEnabled": false, "userId": "test2", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName2", "id": "test2", "emailSearch": false, "enabled": true, "webEnabled": false}', {relaxed => 1}), "Test User Add", { context => 3 });

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
    $json = viewerPostToken2("/user/update", '{"userId":"test2","userName":"UserNameUpdated2", "enabled":true, "removeEnabled":false, "headerAuthEnabled":true, "expression":"foo", "emailSearch":true, "webEnabled":true, "createEnabled":true}', $token2);

    $users = viewerPost("/user/list", "");
    is (@{$users->{data}}, 2, "Check second Update #1");
    eq_or_diff($users->{data}->[1], from_json('{"createEnabled": true, "userId": "test2", "removeEnabled": false, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated2", "id": "test2", "emailSearch": true, "enabled": true, "webEnabled": true}', {relaxed => 1}), "Test User Update", { context => 3 });

    $users = viewerPost2("/user/list", "");
    is (@{$users->{data}}, 2, "Check second Update #2");
    eq_or_diff($users->{data}->[1], from_json('{"createEnabled": true, "userId": "test2", "removeEnabled": false, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated2", "id": "test2", "emailSearch": true, "enabled": true, "webEnabled": true}', {relaxed => 1}), "Test User Update", { context => 3 });

# Reverse settings
    $json = viewerPostToken2("/user/update", '{"userId":"test2","userName":"UserNameUpdated3", "enabled":false, "removeEnabled":true, "headerAuthEnabled":false, "expression":"foo3", "emailSearch":false, "webEnabled":false, "createEnabled": false}', $token2);

    $users = viewerPost("/user/list", "");
    is (@{$users->{data}}, 2, "Check second Update #1");
    eq_or_diff($users->{data}->[1], from_json('{"createEnabled": false, "userId": "test2", "removeEnabled": true, "expression": "foo3", "headerAuthEnabled": false, "userName": "UserNameUpdated3", "id": "test2", "emailSearch": false, "enabled": false, "webEnabled": false}', {relaxed => 1}), "Test User Update", { context => 3 });

    $users = viewerPost2("/user/list", "");
    is (@{$users->{data}}, 2, "Check second Update #2");
    eq_or_diff($users->{data}->[1], from_json('{"createEnabled": false, "userId": "test2", "removeEnabled": true, "expression": "foo3", "headerAuthEnabled": false, "userName": "UserNameUpdated3", "id": "test2", "emailSearch": false, "enabled": false, "webEnabled": false}', {relaxed => 1}), "Test User Update", { context => 3 });

# Column
    my $info = viewerGet("/user/columns?molochRegressionUser=test1");
    eq_or_diff($info, from_json("[]"), "column: empty");

    $info = viewerPostToken("/user/columns/create?molochRegressionUser=test1", '{"name": "column1", "columns": ["a1","dstIp"], "order": [["lp", "asc"]]}', $test1Token);
    ok($info->{success}, "column: create success");
    is($info->{name}, "column1", "column: create name");

    $info = viewerGet("/user/columns?molochRegressionUser=test1");
    eq_or_diff($info, from_json('[{"name":"column1","order":[["lastPacket","asc"]],"columns":["srcIp","dstIp"]}]'), "column: 1 item");

    $info = viewerGet("/user/columns?molochRegressionUser=anonymous&userId=test1");
    eq_or_diff($info, from_json('[{"name":"column1","order":[["lastPacket","asc"]],"columns":["srcIp","dstIp"]}]'), "column: 1 item admin");

    $info = viewerPostToken("/user/columns/delete?molochRegressionUser=test1", 'name=fred', $test1Token);
    ok(! $info->{success}, "column: delete not found");

    $info = viewerGet("/user/columns?molochRegressionUser=test1");
    eq_or_diff($info, from_json('[{"name":"column1","order":[["lastPacket","asc"]],"columns":["srcIp","dstIp"]}]'), "column: 1 item");

    $info = viewerPostToken("/user/columns/delete?molochRegressionUser=test1", 'name=column1', $test1Token);
    ok($info->{success}, "column: delete found");

    $info = viewerGet("/user/columns?molochRegressionUser=test1");
    eq_or_diff($info, from_json("[]"), "column: empty");


# Current
    $info = viewerGet("/user/current?molochRegressionUser=test1");
    ok(!exists $info->{passStore}, "current: no passtore");

# spiview fields
    $info = viewerGet("/user/spiview/fields?molochRegressionUser=test1");
    eq_or_diff($info, from_json("[]"), "spiview fields: empty");

    $info = viewerPostToken("/user/spiview/fields/create?molochRegressionUser=test1", '{"name": "sfields1", "fields": ["srcIp","dstIp"]}', $test1Token);
    ok($info->{success}, "spiview fields: create success");
    is($info->{name}, "sfields1", "spiview fields: create name");

    $info = viewerGet("/user/spiview/fields?molochRegressionUser=test1");
    eq_or_diff($info, from_json('[{"name":"sfields1","fields":["srcIp","dstIp"]}]'), "spiview fields: 1 item");

    $info = viewerGet("/user/spiview/fields?molochRegressionUser=anonymous&userId=test1");
    eq_or_diff($info, from_json('[{"name":"sfields1","fields":["srcIp","dstIp"]}]'), "spiview fields: 1 item admin");

    $info = viewerPostToken("/user/spiview/fields/delete?molochRegressionUser=test1", 'name=fred', $test1Token);
    ok(!$info->{success}, "spiview fields: delete not found");

    $info = viewerGet("/user/spiview/fields?molochRegressionUser=test1");
    eq_or_diff($info, from_json('[{"name":"sfields1","fields":["srcIp","dstIp"]}]'), "spiview fields: 1 item");

    $info = viewerPostToken("/user/spiview/fields/delete?molochRegressionUser=test1", 'name=sfields1', $test1Token);
    ok($info->{success}, "spiview fields: delete found");

    $info = viewerGet("/user/spiview/fields?molochRegressionUser=test1");
    eq_or_diff($info, from_json("[]"), "spiview fields: empty");


# views
    $info = viewerGet("/user/views?molochRegressionUser=test1");
    eq_or_diff($info, from_json("{}"), "view: empty");

    $info = viewerPostToken("/user/views/create?molochRegressionUser=test1", '{"viewName": "view1", "expression": "ip == 1.2.3.4"}', $test1Token);
    ok($info->{success}, "view: create success");
    is($info->{viewName}, "view1", "view: create name");
    eq_or_diff($info->{views}, from_json('{"view1": {"expression":"ip == 1.2.3.4"}}'), "view: 1 item");

    $info = viewerGet("/user/views?molochRegressionUser=test1");
    eq_or_diff($info, from_json('{"view1": {"expression":"ip == 1.2.3.4"}}'), "view: 1 item");

    $info = viewerGet("/user/views?molochRegressionUser=anonymous&userId=test1");
    eq_or_diff($info, from_json('{"view1": {"expression":"ip == 1.2.3.4"}}'), "view: 1 item admin");

    $info = viewerPostToken("/user/views/delete?molochRegressionUser=test1", 'view=fred', $test1Token);
    ok(!$info->{success}, "view: delete not found");

    $info = viewerGet("/user/views?molochRegressionUser=test1");
    eq_or_diff($info, from_json('{"view1": {"expression":"ip == 1.2.3.4"}}'), "view: 1 item");

    $info = viewerPostToken("/user/views/delete?molochRegressionUser=test1", 'view=view1', $test1Token);
    ok($info->{success}, "view: delete found");

    $info = viewerGet("/user/views?molochRegressionUser=test1");
    eq_or_diff($info, from_json("{}"), "view: empty");


# Delete Users
    $json = viewerPostToken("/user/delete", "userId=test1", $token);
    $json = viewerPostToken2("/user/delete", "userId=test2", $token2);
    esGet("/_refresh");
    $users = viewerPost("/user/list", "");
    is (@{$users->{data}}, 0, "Removed user #1");
    $users = viewerPost2("/user/list", "");
    is (@{$users->{data}}, 0, "Removed user #2");
