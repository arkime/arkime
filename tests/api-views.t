use Test::More tests => 41;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

esPost("/tests_views/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }', 1);

my $adminToken = getTokenCookie();
my $token = getTokenCookie('test1');
my $token2 = getTokenCookie('test2');

# create test users
viewerPostToken("/api/user", '{"userId": "test1", "userName": "test1", "enabled":true, "password":"password", "roles":["arkimeUser"]}', $adminToken);
viewerPostToken("/api/user", '{"userId": "test2", "userName": "test2", "enabled":true, "password":"password", "roles":["cont3xtUser"]}', $adminToken);

# No views
my $info = viewerGet("/api/views?molochRegressionUser=test1");
eq_or_diff($info->{data}, from_json("[]"), "empty views");

# create view and and it gets returned and sanitizes name
$info = viewerPostToken("/api/view?molochRegressionUser=test1", '{"name": "view1~`!@#$%^&*()[]{};<>?/", "expression": "ip == 1.2.3.4"}', $token);
my $id1 = $info->{view}->{id};
ok($info->{success}, "create view success");
delete $info->{view}->{id};
eq_or_diff($info->{view}, from_json('{"expression":"ip == 1.2.3.4","user":"test1","name":"view1","users":""}'), "view: 1 item");

# fetch views returns newly created view
$info = viewerGet("/api/views?molochRegressionUser=test1");
eq_or_diff($info->{recordsTotal}, 1, "returns 1 recordsTotal");
delete $info->{data}->[0]->{id};
eq_or_diff($info->{data}->[0], from_json('{"expression":"ip == 1.2.3.4","user":"test1","name":"view1","users":""}'), "view: 1 item");

# admin can see test1 user's views
$info = viewerGet("/api/views?molochRegressionUser=anonymous&userId=test1");
eq_or_diff($info->{recordsTotal}, 1, "returns 1 recordsTotal");
delete $info->{data}->[0]->{id};
eq_or_diff($info->{data}->[0], from_json('{"expression":"ip == 1.2.3.4","user":"test1","name":"view1","users":""}'), "admin can view test1's view");

# fail delete with invalid id
$info = viewerDeleteToken("/api/view/badid?molochRegressionUser=test1", $token);
ok(!$info->{success}, "can't delete with bad id");

# still see 1 view because it wasn't deleted
$info = viewerGet("/api/views?molochRegressionUser=test1");
eq_or_diff($info->{recordsTotal}, 1, "returns 1 recordsTotal");
delete $info->{data}->[0]->{id};
eq_or_diff($info->{data}->[0], from_json('{"expression":"ip == 1.2.3.4","user":"test1","name":"view1","users":""}'), "view wasn't deleted");

# can update view and share it with arkimeUser roles
$info = viewerPutToken("/api/view/${id1}?molochRegressionUser=test1", '{"name": "view1update", "expression": "ip == 4.3.2.1", "roles":["arkimeUser"]}', $token);
is ($info->{view}->{id}, $id1);
delete $info->{view}->{id};
eq_or_diff($info->{view}, from_json('{"expression":"ip == 4.3.2.1","user":"test1","name":"view1update","roles":["arkimeUser"],"users":""}'), "view fields updated");
ok($info->{success}, "update view success");
$info = viewerGet("/api/views?molochRegressionUser=test1");
delete $info->{data}->[0]->{id};
eq_or_diff($info->{data}->[0], from_json('{"expression":"ip == 4.3.2.1","user":"test1","name":"view1update","roles":["arkimeUser"],"users":""}'), "view fields updated");

# test2 doesn't have arkimeUser role so they can't see the view
$info = viewerGet("/api/views?molochRegressionUser=test2");
eq_or_diff($info->{recordsTotal}, 0, "returns 0 recordsTotal for test2 user");

# can update users
$info = viewerPutToken("/api/view/${id1}?molochRegressionUser=test1", '{"name": "view1update", "expression": "ip == 4.3.2.1", "roles":["arkimeUser"], "users":"test2"}', $token);
$info = viewerGet("/api/views?molochRegressionUser=test1");
eq_or_diff($info->{data}->[0]->{users}, "test2", "view users field udpated");

# test2 can see view because it is shared with him via users field
$info = viewerGet("/api/views?molochRegressionUser=test2");
eq_or_diff($info->{recordsTotal}, 1, "returns 1 recordsTotal for test2 user");
delete $info->{data}->[0]->{id};
# and it doesn't show users and roles field because test2 is not the creator
eq_or_diff($info->{data}->[0], from_json('{"expression":"ip == 4.3.2.1","user":"test1","name":"view1update"}'), "can't see roles and users fields");

# can not delete view from other user
$info = viewerDeleteToken("/api/view/${id1}?molochRegressionUser=test2", $token2);
ok(!$info->{success}, "can't delete view created by an other user");

# can delete view with id
$info = viewerDeleteToken("/api/view/${id1}?molochRegressionUser=test1", $token);
ok($info->{success}, "can delete view");

# can create a view with users and roles
$info = viewerPostToken("/api/view?molochRegressionUser=test2", '{"name": "view2", "expression": "ip == 10.0.0.1", "roles":["arkimeUser"], "users":"baduser,test1"}', $token2);
my $id2 = $info->{view}->{id};
ok($info->{success}, "can create view with roles and users");
eq_or_diff($info->{invalidUsers}, from_json('["baduser"]'), "returns invalid users");
eq_or_diff($info->{view}->{users}, "test1", "has one valid user added to the view");
eq_or_diff($info->{view}->{roles}, from_json('["arkimeUser"]'), "added roles");

# can remove users and update editRoles
$info = viewerPutToken("/api/view/${id2}?molochRegressionUser=test2", '{"name": "view2", "expression": "ip == 10.0.0.1", "roles":["arkimeUser"], "users":""}', $token2);
ok($info->{success}, "can update users");
eq_or_diff($info->{view}->{users}, "", "removed users from view");

# test1 user can see the view created by test2 because one is shared via the arkimeUser role
$info = viewerGet("/api/views?molochRegressionUser=test1");
delete $info->{data}->[0]->{id};
eq_or_diff($info->{recordsTotal}, 1, "returns 1 recordsTotal for test1 user");
eq_or_diff($info->{data}->[0], from_json('{"name": "view2", "expression": "ip == 10.0.0.1","user":"test2"}'), "view fields updated");

# admin can view all views when all param is supplied
$info = viewerPostToken("/api/view?molochRegressionUser=test1", '{"name": "asdf", "expression": "ip == 1.2.3.4"}', $token);
my $id3 = $info->{view}->{id};
$info = viewerGet("/api/views?molochRegressionUser=anonymous");
eq_or_diff($info->{recordsTotal}, 1, "returns 1 recordsTotal without all flag");
$info = viewerGet("/api/views?molochRegressionUser=anonymous&all=true");
eq_or_diff($info->{recordsTotal}, 2, "returns 2 recordsTotal with all flag");

# test2 can edit view using editRoles
$info = viewerPostToken("/api/view?molochRegressionUser=test1", '{"name": "view4", "expression": "ip == 4.3.2.1", "roles":["arkimeUser"], "users":"", "editRoles":["cont3xtUser"]}', $token);
my $id4 = $info->{view}->{id};
eq_or_diff($info->{view}->{editRoles}, from_json('["cont3xtUser"]'), "added editRoles");
$info = viewerPutToken("/api/view/${id4}?molochRegressionUser=test2", '{"name": "updated!", "expression": "ip == 4.3.2.1", "roles":["arkimeUser"], "users":"", "editRoles":["cont3xtUser"]}', $token2);
ok($info->{success}, "can update view with editRoles");

# test2 cannot transfer ownership (not admin or creator)
$info = viewerPutToken("/api/view/${id4}?molochRegressionUser=test2", '{"name": "view4", "expression": "ip == 4.3.2.1", "roles":["arkimeUser"], "users":"", "editRoles":["cont3xtUser"], "user":"asdf"}', $token2);
ok(!$info->{success}, "cannot transfer ownership without being admin or creator");
eq_or_diff($info->{text}, "Permission denied");

# can't transfer ownership to invalid user
$info = viewerPutToken("/api/view/${id4}?molochRegressionUser=test1", '{"name": "view4", "expression": "ip == 4.3.2.1", "roles":["arkimeUser"], "users":"", "editRoles":["cont3xtUser"], "user":"asdf"}', $token);
ok(!$info->{success}, "cannot transfer ownership to an invalid user");
eq_or_diff($info->{text}, "User not found");

# can transfer ownership
$info = viewerPutToken("/api/view/${id4}?molochRegressionUser=test1", '{"name": "view4", "expression": "ip == 4.3.2.1", "roles":["arkimeUser"], "users":"", "editRoles":["cont3xtUser"], "user":"test2"}', $token);
ok($info->{success}, "can transfer ownership to valid user");
eq_or_diff($info->{view}->{user}, "test2");

# test2 can delete view using editRoles (plus bonus cleanup)
viewerDeleteToken("/api/view/${id4}?molochRegressionUser=test2", $token2);

# cleanup views
viewerDeleteToken("/api/view/${id2}?molochRegressionUser=test2", $token2);
viewerDeleteToken("/api/view/${id3}?molochRegressionUser=test1", $token);

# views are empty
$info = viewerGet("/api/views?molochRegressionUser=test1");
eq_or_diff($info->{recordsTotal}, 0, "returns 0 recordsTotal");
eq_or_diff($info->{recordsFiltered}, 0, "returns 0 recordsFiltered");
eq_or_diff($info->{data}, from_json("[]"), "empty views");

# cleanup users
viewerDeleteToken2("/api/user/test1", $adminToken);
viewerDeleteToken2("/api/user/test2", $adminToken);
