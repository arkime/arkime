use Test::More tests => 54;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

# clean old users
clearIndex("tests_users");

clearIndex("tests_shareables");

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
$info = viewerPostToken("/api/shareable?arkimeRegressionUser=sac-test1", '{"name": "shareable1", "type": "test", "data": {"key": "value"}}', $token);
my $id1 = $info->{id};
ok($info->{success}, "create shareable success");
ok($info->{shareable}->{id}, "shareable has id");
is($info->{shareable}->{name}, "shareable1", "shareable name is correct");
is($info->{shareable}->{type}, "test", "shareable type is correct");
is($info->{shareable}->{creator}, "sac-test1", "shareable creator is set");
ok($info->{shareable}->{created}, "shareable has created date");
ok($info->{shareable}->{updated}, "shareable has updated date");

# fetch shareables returns newly created shareable
$info = viewerGet("/api/shareables?type=test&arkimeRegressionUser=sac-test1");
is($info->{recordsTotal}, 1, "returns 1 recordsTotal");
is($info->{data}->[0]->{name}, "shareable1", "shareable name in list");
is($info->{data}->[0]->{canEdit}, 1, "creator can edit");
is($info->{data}->[0]->{canDelete}, 1, "creator can delete");

# get specific shareable
$info = viewerGet("/api/shareable/${id1}?arkimeRegressionUser=sac-test1");
ok($info->{success}, "get shareable success");
is($info->{shareable}->{name}, "shareable1", "got shareable with correct name");
is($info->{shareable}->{canEdit}, 1, "creator has canEdit");
is($info->{shareable}->{canDelete}, 1, "creator has canDelete");

# fail delete with invalid id
$info = viewerDeleteToken("/api/shareable/badid?arkimeRegressionUser=sac-test1", $token);
ok(!$info->{success}, "can't delete with bad id");

# still see 1 shareable
$info = viewerGet("/api/shareables?type=test&arkimeRegressionUser=sac-test1");
eq_or_diff($info->{recordsTotal}, 1, "still returns 1 recordsTotal");

# can update shareable
$info = viewerPutToken("/api/shareable/${id1}?arkimeRegressionUser=sac-test1", '{"name": "shareable1updated", "data": {"key": "newvalue"}}', $token);
ok($info->{success}, "update shareable success");
is($info->{shareable}->{name}, "shareable1updated", "shareable name updated");
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
