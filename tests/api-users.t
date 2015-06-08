use Test::More tests => 19;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use strict;

my $pwd = getcwd() . "/pcap";

    my $token = getToken();
    my $token2 = getToken2();

# users
    my $users = viewerPost("/users.json", "");
    is (@{$users->{data}}, 0, "Empty users table");

# Add User 1
    my $json = viewerPost("/addUser", "token=$token&userId=test1&userName=UserName&enabled=on&password=password");

    $users = viewerPost("/users.json", "");
    is (@{$users->{data}}, 1, "Check add #1");
    eq_or_diff($users->{data}->[0], from_json('{"createEnabled": false, "userId": "test1", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName", "id": "test1", "emailSearch": false, "enabled": true, "webEnabled": false}', {relaxed => 1}), "Test User Add", { context => 3 });

    $users = viewerPost2("/users.json", "");
    is (@{$users->{data}}, 1, "Check add #2");
    eq_or_diff($users->{data}->[0], from_json('{"createEnabled": false, "userId": "test1", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName", "id": "test1", "emailSearch": false, "enabled": true, "webEnabled": false}', {relaxed => 1}), "Test User Add", { context => 3 });


# Update User Server 1
    $json = viewerPost("/updateUser/test1?userName=UserNameUpdated&removeEnabled=true&headerAuthEnabled=true&expression=foo&emailSearch=true&webEnabled=true&createEnabled=true", "token=$token");

    $users = viewerPost("/users.json", "");
    is (@{$users->{data}}, 1, "Check Update #1");
    eq_or_diff($users->{data}->[0], from_json('{"createEnabled": true, "userId": "test1", "removeEnabled": true, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated", "id": "test1", "emailSearch": true, "enabled": true, "webEnabled": true}', {relaxed => 1}), "Test User Update", { context => 3 });

    $users = viewerPost2("/users.json", "");
    is (@{$users->{data}}, 1, "Check Update #2");
    eq_or_diff($users->{data}->[0], from_json('{"createEnabled": true, "userId": "test1", "removeEnabled": true, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated", "id": "test1", "emailSearch": true, "enabled": true, "webEnabled": true}', {relaxed => 1}), "Test User Update", { context => 3 });

# Add User 2
    my $json = viewerPost2("/addUser", "token=$token2&userId=test2&userName=UserName2&enabled=on&password=password");

    $users = viewerPost("/users.json", "");
    is (@{$users->{data}}, 2, "Check second add #1");
    eq_or_diff($users->{data}->[1], from_json('{"createEnabled": false, "userId": "test2", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName2", "id": "test2", "emailSearch": false, "enabled": true, "webEnabled": false}', {relaxed => 1}), "Test User Add", { context => 3 });

    $users = viewerPost2("/users.json", "");
    is (@{$users->{data}}, 2, "Check second add #2");
    eq_or_diff($users->{data}->[1], from_json('{"createEnabled": false, "userId": "test2", "removeEnabled": false, "expression": "", "headerAuthEnabled": false, "userName": "UserName2", "id": "test2", "emailSearch": false, "enabled": true, "webEnabled": false}', {relaxed => 1}), "Test User Add", { context => 3 });

# Update User Shared Server
    $json = viewerPost2("/updateUser/test2?userName=UserNameUpdated2&removeEnabled=false&headerAuthEnabled=true&expression=foo&emailSearch=true&webEnabled=true&createEnabled=true", "token=$token2");

    $users = viewerPost("/users.json", "");
    is (@{$users->{data}}, 2, "Check second Update #1");
    eq_or_diff($users->{data}->[1], from_json('{"createEnabled": true, "userId": "test2", "removeEnabled": false, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated2", "id": "test2", "emailSearch": true, "enabled": true, "webEnabled": true}', {relaxed => 1}), "Test User Update", { context => 3 });

    $users = viewerPost2("/users.json", "");
    is (@{$users->{data}}, 2, "Check second Update #2");
    eq_or_diff($users->{data}->[1], from_json('{"createEnabled": true, "userId": "test2", "removeEnabled": false, "expression": "foo", "headerAuthEnabled": true, "userName": "UserNameUpdated2", "id": "test2", "emailSearch": true, "enabled": true, "webEnabled": true}', {relaxed => 1}), "Test User Update", { context => 3 });


# Delete Users
    $json = viewerPost("/deleteUser/test1", "token=$token");
    $json = viewerPost2("/deleteUser/test2", "token=$token2");
    esGet("/_refresh");
    $users = viewerPost("/users.json", "");
    is (@{$users->{data}}, 0, "Removed user #1");
    $users = viewerPost2("/users.json", "");
    is (@{$users->{data}}, 0, "Removed user #2");
