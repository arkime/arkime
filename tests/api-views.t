use Test::More tests => 76;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

# clean old users
viewerGet("/regressionTests/deleteAllUsers");
viewerGet("/regressionTests/deleteAllViews");

my $adminToken = getTokenCookie();
my $token = getTokenCookie('sac-test1');
my $token2 = getTokenCookie('sac-test2');

# create test users
viewerPostToken("/api/user", '{"userId": "sac-test1", "userName": "sac-test1", "enabled":true, "password":"password", "roles":["arkimeUser"]}', $adminToken);
viewerPostToken("/api/user", '{"userId": "sac-test2", "userName": "sac-test2", "enabled":true, "password":"password", "roles":["arkimeUser", "cont3xtUser"]}', $adminToken);

# No views
my $info = viewerGet("/api/views?arkimeRegressionUser=sac-test1");
eq_or_diff($info->{data}, from_json("[]"), "empty views");

# create view and and it gets returned and sanitizes name
$info = viewerPostToken("/api/view?arkimeRegressionUser=sac-test1", '{"name": "view1~`!@#$%^&*()[]{};<>?/", "expression": "ip == 1.2.3.4"}', $token);
my $id1 = $info->{view}->{id};
ok($info->{success}, "create view success");
delete $info->{view}->{id};
eq_or_diff($info->{view}, from_json('{"expression":"ip == 1.2.3.4","user":"sac-test1","name":"view1","users":""}'), "view: 1 item");

# fetch views returns newly created view
$info = viewerGet("/api/views?arkimeRegressionUser=sac-test1");
eq_or_diff($info->{recordsTotal}, 1, "returns 1 recordsTotal");
delete $info->{data}->[0]->{id};
eq_or_diff($info->{data}->[0], from_json('{"expression":"ip == 1.2.3.4","user":"sac-test1","name":"view1","users":""}'), "view: 1 item");

# admin can see sac-test1 user's views
$info = viewerGet("/api/views?arkimeRegressionUser=anonymous&userId=sac-test1");
eq_or_diff($info->{recordsTotal}, 1, "returns 1 recordsTotal");
delete $info->{data}->[0]->{id};
eq_or_diff($info->{data}->[0], from_json('{"expression":"ip == 1.2.3.4","user":"sac-test1","name":"view1","users":""}'), "admin can view sac-test1's view");

# fail delete with invalid id
$info = viewerDeleteToken("/api/view/badid?arkimeRegressionUser=sac-test1", $token);
ok(!$info->{success}, "can't delete with bad id");

# still see 1 view because it wasn't deleted
$info = viewerGet("/api/views?arkimeRegressionUser=sac-test1");
eq_or_diff($info->{recordsTotal}, 1, "returns 1 recordsTotal");
delete $info->{data}->[0]->{id};
eq_or_diff($info->{data}->[0], from_json('{"expression":"ip == 1.2.3.4","user":"sac-test1","name":"view1","users":""}'), "view wasn't deleted");

# can update view and share it with arkimeUser roles
$info = viewerPutToken("/api/view/${id1}?arkimeRegressionUser=sac-test1", '{"name": "view1update", "expression": "ip == 4.3.2.1", "roles":["arkimeUser"]}', $token);
is ($info->{view}->{id}, $id1);
delete $info->{view}->{id};
eq_or_diff($info->{view}, from_json('{"expression":"ip == 4.3.2.1","user":"sac-test1","name":"view1update","roles":["arkimeUser"],"users":""}'), "view fields updated");
ok($info->{success}, "update view success");
$info = viewerGet("/api/views?arkimeRegressionUser=sac-test1");
delete $info->{data}->[0]->{id};
eq_or_diff($info->{data}->[0], from_json('{"expression":"ip == 4.3.2.1","user":"sac-test1","name":"view1update","roles":["arkimeUser"],"users":""}'), "view fields updated");

# can update users
$info = viewerPutToken("/api/view/${id1}?arkimeRegressionUser=sac-test1", '{"name": "view1update", "expression": "ip == 4.3.2.1", "roles":["arkimeUser"], "users":"sac-test2"}', $token);
$info = viewerGet("/api/views?arkimeRegressionUser=sac-test1");
eq_or_diff($info->{data}->[0]->{users}, "sac-test2", "view users field udpated");

# sac-test2 can see view because it is shared with him via users field
$info = viewerGet("/api/views?arkimeRegressionUser=sac-test2");
eq_or_diff($info->{recordsTotal}, 1, "returns 1 recordsTotal for sac-test2 user");
delete $info->{data}->[0]->{id};
# and it doesn't show users and roles field because sac-test2 is not the creator
eq_or_diff($info->{data}->[0], from_json('{"expression":"ip == 4.3.2.1","user":"sac-test1","name":"view1update"}'), "can't see roles and users fields");

# can not delete view from other user
$info = viewerDeleteToken("/api/view/${id1}?arkimeRegressionUser=sac-test2", $token2);
ok(!$info->{success}, "can't delete view created by an other user");

# can delete view with id
$info = viewerDeleteToken("/api/view/${id1}?arkimeRegressionUser=sac-test1", $token);
ok($info->{success}, "can delete view");

# can create a view with users and roles
$info = viewerPostToken("/api/view?arkimeRegressionUser=sac-test2", '{"name": "view2", "expression": "ip == 10.0.0.1", "roles":["arkimeUser"], "users":"baduser,sac-test1"}', $token2);
my $id2 = $info->{view}->{id};
ok($info->{success}, "can create view with roles and users");
eq_or_diff($info->{invalidUsers}, from_json('["baduser"]'), "returns invalid users");
eq_or_diff($info->{view}->{users}, "sac-test1", "has one valid user added to the view");
eq_or_diff($info->{view}->{roles}, from_json('["arkimeUser"]'), "added roles");

# can remove users and update editRoles
$info = viewerPutToken("/api/view/${id2}?arkimeRegressionUser=sac-test2", '{"name": "view2", "expression": "ip == 10.0.0.1", "roles":["arkimeUser"], "users":""}', $token2);
ok($info->{success}, "can update users");
eq_or_diff($info->{view}->{users}, "", "removed users from view");

# sac-test1 user can see the view created by sac-test2 because one is shared via the arkimeUser role
$info = viewerGet("/api/views?arkimeRegressionUser=sac-test1");
delete $info->{data}->[0]->{id};
eq_or_diff($info->{recordsTotal}, 1, "returns 1 recordsTotal for sac-test1 user");
eq_or_diff($info->{data}->[0], from_json('{"name": "view2", "expression": "ip == 10.0.0.1","user":"sac-test2"}'), "view fields updated");

# admin can view all views when all param is supplied
$info = viewerPostToken("/api/view?arkimeRegressionUser=sac-test1", '{"name": "asdf", "expression": "ip == 1.2.3.4"}', $token);
my $id3 = $info->{view}->{id};
$info = viewerGet("/api/views?arkimeRegressionUser=anonymous");
eq_or_diff($info->{recordsTotal}, 1, "returns 1 recordsTotal without all flag");
$info = viewerGet("/api/views?arkimeRegressionUser=anonymous&all=true");
eq_or_diff($info->{recordsTotal}, 2, "returns 2 recordsTotal with all flag");

# sac-test2 can edit view using editRoles
$info = viewerPostToken("/api/view?arkimeRegressionUser=sac-test1", '{"name": "view4", "expression": "ip == 4.3.2.1", "roles":["arkimeUser"], "users":"", "editRoles":["cont3xtUser"]}', $token);
my $id4 = $info->{view}->{id};
eq_or_diff($info->{view}->{editRoles}, from_json('["cont3xtUser"]'), "added editRoles");
$info = viewerPutToken("/api/view/${id4}?arkimeRegressionUser=sac-test2", '{"name": "updated!", "expression": "ip == 4.3.2.1", "roles":["arkimeUser"], "users":"", "editRoles":["cont3xtUser"]}', $token2);
ok($info->{success}, "can update view with editRoles");

# sac-test2 cannot transfer ownership (not admin or creator)
$info = viewerPutToken("/api/view/${id4}?arkimeRegressionUser=sac-test2", '{"name": "view4", "expression": "ip == 4.3.2.1", "roles":["arkimeUser"], "users":"", "editRoles":["cont3xtUser"], "user":"asdf"}', $token2);
ok(!$info->{success}, "cannot transfer ownership without being admin or creator");
eq_or_diff($info->{text}, "Permission denied");

# editor cannot transfer ownership even to a valid user
$info = viewerPutToken("/api/view/${id4}?arkimeRegressionUser=sac-test2", '{"name": "view4", "expression": "ip == 4.3.2.1", "roles":["arkimeUser"], "users":"", "editRoles":["cont3xtUser"], "user":"sac-test2"}', $token2);
ok(!$info->{success}, "editor cannot transfer ownership to valid user");
eq_or_diff($info->{text}, "Permission denied", "editor gets permission denied not user not found");

# can't transfer ownership to invalid user
$info = viewerPutToken("/api/view/${id4}?arkimeRegressionUser=sac-test1", '{"name": "view4", "expression": "ip == 4.3.2.1", "roles":["arkimeUser"], "users":"", "editRoles":["cont3xtUser"], "user":"asdf"}', $token);
ok(!$info->{success}, "cannot transfer ownership to an invalid user");
eq_or_diff($info->{text}, "User not found");

# can transfer ownership
$info = viewerPutToken("/api/view/${id4}?arkimeRegressionUser=sac-test1", '{"name": "view4", "expression": "ip == 4.3.2.1", "roles":["arkimeUser"], "users":"", "editRoles":["cont3xtUser"], "user":"sac-test2"}', $token);
ok($info->{success}, "can transfer ownership to valid user");
eq_or_diff($info->{view}->{user}, "sac-test2");

# sac-test2 can delete view using editRoles (plus bonus cleanup)
viewerDeleteToken("/api/view/${id4}?arkimeRegressionUser=sac-test2", $token2);

# --- Input validation tests ---

# create view missing name
$info = viewerPostToken("/api/view?arkimeRegressionUser=sac-test1", '{"expression": "ip == 1.2.3.4"}', $token);
ok(!$info->{success}, "create view fails without name");
is($info->{text}, "Missing view name", "correct error for missing name");

# create view missing expression
$info = viewerPostToken("/api/view?arkimeRegressionUser=sac-test1", '{"name": "noexpr"}', $token);
ok(!$info->{success}, "create view fails without expression");
is($info->{text}, "Missing view expression", "correct error for missing expression");

# update view missing name
$info = viewerPutToken("/api/view/${id2}?arkimeRegressionUser=sac-test2", '{"expression": "ip == 1.2.3.4"}', $token2);
ok(!$info->{success}, "update view fails without name");
is($info->{text}, "Missing view name", "correct error for missing name on update");

# update view missing expression
$info = viewerPutToken("/api/view/${id2}?arkimeRegressionUser=sac-test2", '{"name": "view2"}', $token2);
ok(!$info->{success}, "update view fails without expression");
is($info->{text}, "Missing view expression", "correct error for missing expression on update");

# update non-existent view
$info = viewerPutToken("/api/view/nonexistent123?arkimeRegressionUser=sac-test1", '{"name": "x", "expression": "ip == 1.2.3.4"}', $token);
ok(!$info->{success}, "update non-existent view fails");

# --- Extra field sanitization tests ---

# create view with extra top-level fields
$info = viewerPostToken("/api/view?arkimeRegressionUser=sac-test1", '{"name": "sanitize-test", "expression": "ip == 1.2.3.4", "evil": "data", "badField": 123}', $token);
ok($info->{success}, "create view with extra fields succeeds");
my $idSan = $info->{view}->{id};
ok(!exists $info->{view}->{evil}, "extra field 'evil' not in create response");
ok(!exists $info->{view}->{badField}, "extra field 'badField' not in create response");

# verify extra fields not persisted
$info = viewerGet("/api/views?arkimeRegressionUser=sac-test1");
my @san = grep { $_->{id} eq $idSan } @{$info->{data}};
ok(!exists $san[0]->{evil}, "extra field 'evil' not in list response");
ok(!exists $san[0]->{badField}, "extra field 'badField' not in list response");

# update view with extra top-level fields
$info = viewerPutToken("/api/view/${idSan}?arkimeRegressionUser=sac-test1", '{"name": "sanitize-test", "expression": "ip == 1.2.3.4", "injected": true, "foo": "bar"}', $token);
ok($info->{success}, "update view with extra fields succeeds");
ok(!exists $info->{view}->{injected}, "extra field 'injected' not in update response");
ok(!exists $info->{view}->{foo}, "extra field 'foo' not in update response");

viewerDeleteToken("/api/view/${idSan}?arkimeRegressionUser=sac-test1", $token);

# cleanup id2 and id3 before sort/search/pagination tests
viewerDeleteToken("/api/view/${id2}?arkimeRegressionUser=sac-test2", $token2);
viewerDeleteToken("/api/view/${id3}?arkimeRegressionUser=sac-test1", $token);

# --- Sorting tests ---
# create views for sort/search/pagination tests
$info = viewerPostToken("/api/view?arkimeRegressionUser=sac-test1", '{"name": "alpha", "expression": "ip == 1.2.3.4"}', $token);
my $idA = $info->{view}->{id};
$info = viewerPostToken("/api/view?arkimeRegressionUser=sac-test1", '{"name": "beta", "expression": "ip == 5.6.7.8"}', $token);
my $idB = $info->{view}->{id};
$info = viewerPostToken("/api/view?arkimeRegressionUser=sac-test1", '{"name": "gamma", "expression": "ip == 9.9.9.9"}', $token);
my $idG = $info->{view}->{id};

# default sort by name asc
$info = viewerGet("/api/views?arkimeRegressionUser=sac-test1");
is($info->{data}->[0]->{name}, "alpha", "default sort: first is alpha");
is($info->{data}->[1]->{name}, "beta", "default sort: second is beta");
is($info->{data}->[2]->{name}, "gamma", "default sort: third is gamma");

# sort by name desc
$info = viewerGet("/api/views?arkimeRegressionUser=sac-test1&sort=name&desc=true");
is($info->{data}->[0]->{name}, "gamma", "desc sort: first is gamma");
is($info->{data}->[2]->{name}, "alpha", "desc sort: last is alpha");

# invalid sort field falls back to name
$info = viewerGet("/api/views?arkimeRegressionUser=sac-test1&sort=invalidfield");
is($info->{data}->[0]->{name}, "alpha", "invalid sort falls back to name asc");

# --- Search tests ---

# searchTerm filters by name
$info = viewerGet("/api/views?arkimeRegressionUser=sac-test1&searchTerm=bet");
is($info->{recordsFiltered}, 1, "searchTerm filters to 1 result");
is($info->{data}->[0]->{name}, "beta", "searchTerm finds beta");

# searchTerm with no match
$info = viewerGet("/api/views?arkimeRegressionUser=sac-test1&searchTerm=nonexistent");
is($info->{recordsFiltered}, 0, "searchTerm with no match returns 0");
eq_or_diff($info->{data}, from_json("[]"), "searchTerm with no match returns empty data");

# recordsTotal is unfiltered count, recordsFiltered reflects search
$info = viewerGet("/api/views?arkimeRegressionUser=sac-test1&searchTerm=bet");
is($info->{recordsTotal}, 3, "recordsTotal is unfiltered count");
is($info->{recordsFiltered}, 1, "recordsFiltered is filtered count");

# --- Pagination tests ---

# length limits results
$info = viewerGet("/api/views?arkimeRegressionUser=sac-test1&length=2");
is(scalar @{$info->{data}}, 2, "length=2 returns 2 views");
is($info->{recordsTotal}, 3, "recordsTotal still 3 with length=2");

# start offsets results
$info = viewerGet("/api/views?arkimeRegressionUser=sac-test1&length=2&start=2");
is(scalar @{$info->{data}}, 1, "start=2 length=2 returns 1 remaining view");
is($info->{data}->[0]->{name}, "gamma", "pagination offset returns gamma");

# --- Non-admin all flag ---

# non-admin all=true should not show other users' views
$info = viewerGet("/api/views?arkimeRegressionUser=sac-test2&all=true");
is($info->{recordsTotal}, 0, "non-admin all=true doesn't show other users views");

# cleanup sort/search/pagination views
viewerDeleteToken("/api/view/${idA}?arkimeRegressionUser=sac-test1", $token);
viewerDeleteToken("/api/view/${idB}?arkimeRegressionUser=sac-test1", $token);
viewerDeleteToken("/api/view/${idG}?arkimeRegressionUser=sac-test1", $token);

# views are empty
$info = viewerGet("/api/views?arkimeRegressionUser=sac-test1");
eq_or_diff($info->{recordsTotal}, 0, "returns 0 recordsTotal");
eq_or_diff($info->{recordsFiltered}, 0, "returns 0 recordsFiltered");
eq_or_diff($info->{data}, from_json("[]"), "empty views");

# cleanup users
viewerDeleteToken2("/api/user/sac-test1", $adminToken);
viewerDeleteToken2("/api/user/sac-test2", $adminToken);
