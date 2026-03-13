use Test::More tests => 110;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

# clean old users
viewerGet("/regressionTests/deleteAllUsers");
viewerGet("/regressionTests/deleteAllShareables");

my $adminToken = getTokenCookie();
my $token = getTokenCookie('sac-test1');
my $token2 = getTokenCookie('sac-test2');

# create test users
viewerPostToken("/api/user", '{"userId": "sac-test1", "userName": "sac-test1", "enabled":true, "password":"password", "roles":["arkimeUser"]}', $adminToken);
viewerPostToken("/api/user", '{"userId": "sac-test2", "userName": "sac-test2", "enabled":true, "password":"password", "roles":["arkimeUser", "cont3xtUser"]}', $adminToken);

# No shareables
my $info = viewerGet("/api/shareables?type=test&arkimeRegressionUser=sac-test1");
eq_or_diff($info->{data}, from_json("[]"), "empty shareables");

# create shareable and it gets returned
$info = viewerPostToken("/api/shareable?arkimeRegressionUser=sac-test1", '{"name": "shareable1", "description": "important item", "type": "test", "data": {"key": "value"}}', $token);
my $id1 = $info->{id};
ok($info->{success}, "create shareable success");
ok($info->{shareable}->{id}, "shareable has id");
is($info->{shareable}->{name}, "shareable1", "shareable name is correct");
is($info->{shareable}->{description}, "important item", "shareable description is correct");
is($info->{shareable}->{type}, "test", "shareable type is correct");
is($info->{shareable}->{creator}, "sac-test1", "shareable creator is set");
ok($info->{shareable}->{created}, "shareable has created date");
ok($info->{shareable}->{updated}, "shareable has updated date");

# fetch shareables returns newly created shareable
$info = viewerGet("/api/shareables?type=test&arkimeRegressionUser=sac-test1");
is($info->{recordsTotal}, 1, "returns 1 recordsTotal");
is($info->{data}->[0]->{name}, "shareable1", "shareable name in list");
is($info->{data}->[0]->{description}, "important item", "shareable description in list");
is($info->{data}->[0]->{canEdit}, 1, "creator can edit");
is($info->{data}->[0]->{canDelete}, 1, "creator can delete");

# get specific shareable
$info = viewerGet("/api/shareable/${id1}?arkimeRegressionUser=sac-test1");
ok($info->{success}, "get shareable success");
is($info->{shareable}->{name}, "shareable1", "got shareable with correct name");
is($info->{shareable}->{description}, "important item", "got shareable with correct description");
is($info->{shareable}->{canEdit}, 1, "creator has canEdit");
is($info->{shareable}->{canDelete}, 1, "creator has canDelete");

# fail delete with invalid id
$info = viewerDeleteToken("/api/shareable/badid?arkimeRegressionUser=sac-test1", $token);
ok(!$info->{success}, "can't delete with bad id");

# still see 1 shareable
$info = viewerGet("/api/shareables?type=test&arkimeRegressionUser=sac-test1");
eq_or_diff($info->{recordsTotal}, 1, "still returns 1 recordsTotal");

# can update shareable
$info = viewerPutToken("/api/shareable/${id1}?arkimeRegressionUser=sac-test1", '{"name": "shareable1updated", "description": "important item updated", "data": {"key": "newvalue"}}', $token);
ok($info->{success}, "update shareable success");
is($info->{shareable}->{name}, "shareable1updated", "shareable name updated");
is($info->{shareable}->{description}, "important item updated", "shareable description updated");
is($info->{shareable}->{data}->{key}, "newvalue", "shareable data updated");
is($info->{shareable}->{creator}, "sac-test1", "creator unchanged");
ok($info->{shareable}->{created}, "created date still exists");

# creator is preserved on update
my $originalCreated = $info->{shareable}->{created};
my $originalUpdated = $info->{shareable}->{updated};
sleep(1);
$info = viewerPutToken("/api/shareable/${id1}?arkimeRegressionUser=sac-test1", '{"name": "shareable1v2", "data": {"key": "value3"}}', $token);
is($info->{shareable}->{created}, $originalCreated, "created date preserved");
ok($info->{shareable}->{updated} ne $originalUpdated, "updated date changed");

# cannot change type on update
$info = viewerPutToken("/api/shareable/${id1}?arkimeRegressionUser=sac-test1", '{"name": "shareable1v2", "type": "different", "data": {"key": "value3"}}', $token);
ok(!$info->{success}, "cannot change shareable type");
ok($info->{text}, "error message returned when trying to change type");

# sac-test2 cannot see view created by sac-test1 without sharing
$info = viewerGet("/api/shareables?type=test&arkimeRegressionUser=sac-test2");
is($info->{recordsTotal}, 0, "sac-test2 cannot see sac-test1's shareable");

# sac-test2 cannot get shareable created by sac-test1
$info = viewerGet("/api/shareable/${id1}?arkimeRegressionUser=sac-test2");
ok(!$info->{success}, "sac-test2 cannot get sac-test1's shareable");

# sac-test2 cannot delete shareable created by sac-test1
$info = viewerDeleteToken("/api/shareable/${id1}?arkimeRegressionUser=sac-test2", $token2);
ok(!$info->{success}, "sac-test2 cannot delete sac-test1's shareable");

# can update shareable to add view permissions
$info = viewerPutToken("/api/shareable/${id1}?arkimeRegressionUser=sac-test1", '{"name": "shareable1v2", "data": {"key": "value3"}, "viewUsers": ["sac-test2"]}', $token);
ok($info->{success}, "update shareable with viewUsers success");

# sac-test2 can now see shareable via viewUsers
$info = viewerGet("/api/shareables?type=test&arkimeRegressionUser=sac-test2");
is($info->{recordsTotal}, 1, "sac-test2 can see sac-test1's shareable via viewUsers");
is($info->{data}->[0]->{canEdit}, 0, "sac-test2 cannot edit (viewUsers only)");
is($info->{data}->[0]->{canDelete}, 0, "sac-test2 cannot delete (viewUsers only)");

# sac-test2 can now get shareable
$info = viewerGet("/api/shareable/${id1}?arkimeRegressionUser=sac-test2");
ok($info->{success}, "sac-test2 can get sac-test1's shareable via viewUsers");
is($info->{shareable}->{canEdit}, 0, "sac-test2 cannot edit");
is($info->{shareable}->{canDelete}, 0, "sac-test2 cannot delete");

# can update shareable to add edit permissions via editUsers
$info = viewerPutToken("/api/shareable/${id1}?arkimeRegressionUser=sac-test1", '{"name": "shareable1v2", "data": {"key": "value3"}, "viewUsers": [], "editUsers": ["sac-test2"]}', $token);
ok($info->{success}, "update shareable with editUsers success");

# sac-test2 can now edit and see it in list
$info = viewerGet("/api/shareables?type=test&arkimeRegressionUser=sac-test2&viewOnly=false");
is($info->{recordsTotal}, 1, "sac-test2 sees shareable via editUsers");
is($info->{data}->[0]->{canEdit}, 1, "sac-test2 can edit (editUsers)");
is($info->{data}->[0]->{canDelete}, 0, "sac-test2 still cannot delete (editUsers only)");

# sac-test2 can update shareable
$info = viewerPutToken("/api/shareable/${id1}?arkimeRegressionUser=sac-test2", '{"name": "shareable1edited", "data": {"key": "newkey"}}', $token2);
ok($info->{success}, "sac-test2 can edit shareable via editUsers");
is($info->{shareable}->{name}, "shareable1edited", "sac-test2 updated shareable");

# can update shareable to add view permissions via viewRoles
$info = viewerPutToken("/api/shareable/${id1}?arkimeRegressionUser=sac-test1", '{"name": "shareable1final", "data": {"key": "value"}, "editUsers": [], "viewRoles": ["cont3xtUser"]}', $token);
ok($info->{success}, "update shareable with viewRoles success");

# sac-test2 can now see it because sac-test2 has cont3xtUser role
$info = viewerGet("/api/shareables?type=test&arkimeRegressionUser=sac-test2");
is($info->{recordsTotal}, 1, "sac-test2 sees shareable via viewRoles");
is($info->{data}->[0]->{canEdit}, 0, "sac-test2 cannot edit (viewRoles only)");

# can delete shareable when creator
$info = viewerDeleteToken("/api/shareable/${id1}?arkimeRegressionUser=sac-test1", $token);
ok($info->{success}, "creator can delete shareable");

# shareable is gone
$info = viewerGet("/api/shareables?type=test&arkimeRegressionUser=sac-test1");
is($info->{recordsTotal}, 0, "shareable deleted");

# create another shareable with editRoles
$info = viewerPostToken("/api/shareable?arkimeRegressionUser=sac-test1", '{"name": "shareable2", "type": "test2", "data": {"test": "data"}, "editRoles": ["cont3xtUser"]}', $token);
my $id2 = $info->{id};
ok($info->{success}, "create shareable with editRoles");

# sac-test2 can edit because they have cont3xtUser role
$info = viewerGet("/api/shareables?type=test2&arkimeRegressionUser=sac-test2&viewOnly=false");
is($info->{recordsTotal}, 1, "sac-test2 sees shareable via editRoles");
is($info->{data}->[0]->{canEdit}, 1, "sac-test2 can edit via editRoles");

# list with viewOnly parameter - default is true so excludes editRoles access
$info = viewerGet("/api/shareables?type=test2&arkimeRegressionUser=sac-test2");
is($info->{recordsTotal}, 0, "viewOnly=true (default) excludes editRoles access");

# list with viewOnly=false shows editRoles access
$info = viewerGet("/api/shareables?type=test2&viewOnly=false&arkimeRegressionUser=sac-test1");
is($info->{recordsTotal}, 1, "viewOnly=false includes editRoles access");

# admin can always delete
$info = viewerDeleteToken("/api/shareable/${id2}?arkimeRegressionUser=anonymous", $adminToken);
ok($info->{success}, "arkimeAdmin can delete shareable");

# type parameter validation
$info = viewerGet("/api/shareables?arkimeRegressionUser=sac-test1");
ok(!$info->{success}, "missing type parameter fails");

$info = viewerGet("/api/shareables?type=&arkimeRegressionUser=sac-test1");
ok(!$info->{success}, "empty type parameter fails");

# create shareable with non-existent user in viewUsers
$info = viewerPostToken("/api/shareable?arkimeRegressionUser=sac-test1", '{"name": "shareable3", "type": "test", "data": {}, "viewUsers": ["nonexistent-user"]}', $token);
my $id3 = $info->{id};
ok($info->{success}, "create shareable with non-existent user");

# verify non-existent user is not in viewUsers
$info = viewerGet("/api/shareable/${id3}?arkimeRegressionUser=sac-test1");
is_deeply($info->{shareable}->{viewUsers}, [], "non-existent user not stored in viewUsers");

# update with non-existent user mixed with valid user
$info = viewerPutToken("/api/shareable/${id3}?arkimeRegressionUser=sac-test1", '{"name": "shareable3", "data": {}, "viewUsers": ["sac-test2", "nonexistent-user", "another-fake-user"]}', $token);
ok($info->{success}, "update shareable with mixed valid and non-existent users");

# verify only valid user is stored
$info = viewerGet("/api/shareable/${id3}?arkimeRegressionUser=sac-test1");
is_deeply($info->{shareable}->{viewUsers}, ["sac-test2"], "only valid users stored");

# delete shareable
viewerDeleteToken("/api/shareable/${id3}?arkimeRegressionUser=sac-test1", $token);

# --- Input validation tests ---

# create shareable missing name
$info = viewerPostToken("/api/shareable?arkimeRegressionUser=sac-test1", '{"type": "test", "data": {}}', $token);
ok(!$info->{success}, "create fails without name");
is($info->{text}, "Missing shareable name", "correct error for missing name");

# create shareable missing type
$info = viewerPostToken("/api/shareable?arkimeRegressionUser=sac-test1", '{"name": "notype", "data": {}}', $token);
ok(!$info->{success}, "create fails without type");
is($info->{text}, "Missing shareable type", "correct error for missing type");

# create shareable with non-string description
$info = viewerPostToken("/api/shareable?arkimeRegressionUser=sac-test1", '{"name": "test", "type": "test", "description": 123}', $token);
ok(!$info->{success}, "create fails with non-string description");
is($info->{text}, "Description must be a string", "correct error for bad description");

# update non-existent shareable
$info = viewerPutToken("/api/shareable/nonexistent123?arkimeRegressionUser=sac-test1", '{"name": "x", "data": {}}', $token);
ok(!$info->{success}, "update non-existent shareable fails");

# get non-existent shareable
$info = viewerGet("/api/shareable/nonexistent123?arkimeRegressionUser=sac-test1");
ok(!$info->{success}, "get non-existent shareable fails");

# update with non-string description
$info = viewerPostToken("/api/shareable?arkimeRegressionUser=sac-test1", '{"name": "desctest", "type": "test", "data": {}}', $token);
my $idDesc = $info->{id};
$info = viewerPutToken("/api/shareable/${idDesc}?arkimeRegressionUser=sac-test1", '{"name": "desctest", "description": ["array"]}', $token);
ok(!$info->{success}, "update fails with non-string description");
is($info->{text}, "Description must be a string", "correct error for bad description on update");

# --- Pagination tests ---
# create multiple shareables for pagination
$info = viewerPostToken("/api/shareable?arkimeRegressionUser=sac-test1", '{"name": "alpha", "type": "pagtest", "data": {}}', $token);
my $idPA = $info->{id};
$info = viewerPostToken("/api/shareable?arkimeRegressionUser=sac-test1", '{"name": "beta", "type": "pagtest", "data": {}}', $token);
my $idPB = $info->{id};
$info = viewerPostToken("/api/shareable?arkimeRegressionUser=sac-test1", '{"name": "gamma", "type": "pagtest", "data": {}}', $token);
my $idPG = $info->{id};

# length limits results
$info = viewerGet("/api/shareables?type=pagtest&arkimeRegressionUser=sac-test1&length=2");
is(scalar @{$info->{data}}, 2, "length=2 returns 2 shareables");
is($info->{recordsTotal}, 3, "recordsTotal still 3 with length=2");

# start offsets results
$info = viewerGet("/api/shareables?type=pagtest&arkimeRegressionUser=sac-test1&length=2&start=2");
is(scalar @{$info->{data}}, 1, "start=2 length=2 returns 1 remaining");

# start beyond data returns empty
$info = viewerGet("/api/shareables?type=pagtest&arkimeRegressionUser=sac-test1&start=100");
is(scalar @{$info->{data}}, 0, "start beyond data returns empty");
is($info->{recordsTotal}, 3, "recordsTotal still correct with large start");

# --- Type isolation tests ---
# shareables of different type should not appear
$info = viewerGet("/api/shareables?type=other&arkimeRegressionUser=sac-test1");
is($info->{recordsTotal}, 0, "different type returns 0");
eq_or_diff($info->{data}, from_json("[]"), "different type returns empty data");

# --- Permission edge cases ---

# viewUser cannot edit
$info = viewerPutToken("/api/shareable/${idPA}?arkimeRegressionUser=sac-test1", '{"name": "alpha", "data": {}, "viewUsers": ["sac-test2"]}', $token);
ok($info->{success}, "add sac-test2 as viewUser");
$info = viewerPutToken("/api/shareable/${idPA}?arkimeRegressionUser=sac-test2", '{"name": "hacked", "data": {}}', $token2);
ok(!$info->{success}, "viewUser cannot edit shareable");

# viewUser cannot delete
$info = viewerDeleteToken("/api/shareable/${idPA}?arkimeRegressionUser=sac-test2", $token2);
ok(!$info->{success}, "viewUser cannot delete shareable");

# editUser cannot delete (only creator/admin)
$info = viewerPutToken("/api/shareable/${idPA}?arkimeRegressionUser=sac-test1", '{"name": "alpha", "data": {}, "viewUsers": [], "editUsers": ["sac-test2"]}', $token);
ok($info->{success}, "add sac-test2 as editUser");
$info = viewerDeleteToken("/api/shareable/${idPA}?arkimeRegressionUser=sac-test2", $token2);
ok(!$info->{success}, "editUser cannot delete shareable");

# shared field is correct
$info = viewerGet("/api/shareables?type=pagtest&arkimeRegressionUser=sac-test1&viewOnly=false");
is($info->{data}->[0]->{shared}, 0, "creator sees shared=false");
$info = viewerGet("/api/shareables?type=pagtest&arkimeRegressionUser=sac-test2&viewOnly=false");
is($info->{data}->[0]->{shared}, 1, "non-creator sees shared=true");

# --- Extra field sanitization tests ---

# create shareable with extra top-level fields
$info = viewerPostToken("/api/shareable?arkimeRegressionUser=sac-test1", '{"name": "sanitize-test", "type": "santest", "data": {"key":"val"}, "evil": "data", "badField": 123}', $token);
ok($info->{success}, "create with extra fields succeeds");
my $idSan = $info->{id};
ok(!exists $info->{shareable}->{evil}, "extra field 'evil' not in create response");
ok(!exists $info->{shareable}->{badField}, "extra field 'badField' not in create response");

# verify extra fields not persisted
$info = viewerGet("/api/shareable/${idSan}?arkimeRegressionUser=sac-test1");
ok(!exists $info->{shareable}->{evil}, "extra field 'evil' not persisted");
ok(!exists $info->{shareable}->{badField}, "extra field 'badField' not persisted");

# update shareable with extra top-level fields
$info = viewerPutToken("/api/shareable/${idSan}?arkimeRegressionUser=sac-test1", '{"name": "sanitize-test", "data": {"key":"val"}, "injected": true, "foo": "bar"}', $token);
ok($info->{success}, "update with extra fields succeeds");
ok(!exists $info->{shareable}->{injected}, "extra field 'injected' not in update response");
ok(!exists $info->{shareable}->{foo}, "extra field 'foo' not in update response");

# try to override creator via body on create
$info = viewerPostToken("/api/shareable?arkimeRegressionUser=sac-test1", '{"name": "creator-test", "type": "santest", "data": {}, "creator": "sac-test2"}', $token);
my $idCreator = $info->{id};
is($info->{shareable}->{creator}, "sac-test1", "creator cannot be overridden on create");

# try to override creator via body on update
$info = viewerPutToken("/api/shareable/${idCreator}?arkimeRegressionUser=sac-test1", '{"name": "creator-test", "data": {}, "creator": "sac-test2"}', $token);
is($info->{shareable}->{creator}, "sac-test1", "creator cannot be overridden on update");

viewerDeleteToken("/api/shareable/${idSan}?arkimeRegressionUser=sac-test1", $token);
viewerDeleteToken("/api/shareable/${idCreator}?arkimeRegressionUser=sac-test1", $token);

# --- Ownership / permission tests ---

# create shareable owned by sac-test1 with edit access for sac-test2
$info = viewerPostToken("/api/shareable?arkimeRegressionUser=sac-test1", '{"name": "own-test", "type": "owntest", "data": {}, "editUsers": ["sac-test2"]}', $token);
my $idOwn = $info->{id};

# editor can update name/data but creator stays the same
$info = viewerPutToken("/api/shareable/${idOwn}?arkimeRegressionUser=sac-test2", '{"name": "editor-changed", "data": {"edited": true}}', $token2);
ok($info->{success}, "editor can update shareable");
is($info->{shareable}->{name}, "editor-changed", "editor updated name");
is($info->{shareable}->{creator}, "sac-test1", "creator unchanged after editor update");

# editor tries to override creator field
$info = viewerPutToken("/api/shareable/${idOwn}?arkimeRegressionUser=sac-test2", '{"name": "editor-changed", "data": {}, "creator": "sac-test2"}', $token2);
is($info->{shareable}->{creator}, "sac-test1", "editor cannot steal ownership via creator field");

# editor cannot delete
$info = viewerDeleteToken("/api/shareable/${idOwn}?arkimeRegressionUser=sac-test2", $token2);
ok(!$info->{success}, "editor cannot delete shareable");

# editor cannot change permissions (editUsers/viewUsers)
$info = viewerPutToken("/api/shareable/${idOwn}?arkimeRegressionUser=sac-test2", '{"name": "editor-changed", "data": {}, "editUsers": [], "viewUsers": ["sac-test2"]}', $token2);
ok($info->{success}, "editor update with permission changes accepted");
# verify editor removed themselves from editUsers
$info = viewerGet("/api/shareable/${idOwn}?arkimeRegressionUser=sac-test1");
is_deeply($info->{shareable}->{editUsers}, [], "editor can change editUsers (removed self)");

# sac-test2 can no longer edit after removing self from editUsers
$info = viewerPutToken("/api/shareable/${idOwn}?arkimeRegressionUser=sac-test2", '{"name": "nope", "data": {}}', $token2);
ok(!$info->{success}, "editor who removed self can no longer edit");

# creator can still edit and delete
$info = viewerPutToken("/api/shareable/${idOwn}?arkimeRegressionUser=sac-test1", '{"name": "creator-edit", "data": {}}', $token);
ok($info->{success}, "creator can still edit");
$info = viewerDeleteToken("/api/shareable/${idOwn}?arkimeRegressionUser=sac-test1", $token);
ok($info->{success}, "creator can delete");

# cleanup pagination and validation shareables
viewerDeleteToken("/api/shareable/${idPA}?arkimeRegressionUser=sac-test1", $token);
viewerDeleteToken("/api/shareable/${idPB}?arkimeRegressionUser=sac-test1", $token);
viewerDeleteToken("/api/shareable/${idPG}?arkimeRegressionUser=sac-test1", $token);
viewerDeleteToken("/api/shareable/${idDesc}?arkimeRegressionUser=sac-test1", $token);

# verify all clean
$info = viewerGet("/api/shareables?type=pagtest&arkimeRegressionUser=sac-test1");
is($info->{recordsTotal}, 0, "all pagtest shareables cleaned up");
$info = viewerGet("/api/shareables?type=test&arkimeRegressionUser=sac-test1");
is($info->{recordsTotal}, 0, "all test shareables cleaned up");
