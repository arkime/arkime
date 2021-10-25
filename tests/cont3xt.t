# Test cont3xt.js
use Test::More tests => 12;
use Test::Differences;
use Data::Dumper;
use MolochTest;
use JSON;
use strict;

esPost("/cont3xt_links/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }');

my $json;

$json = cont3xtGet('/api/linkGroup/getViewable');
eq_or_diff($json, from_json('{"success": true, "linkGroups": []}'));

$json = cont3xtGet('/api/linkGroup/getEditable');
eq_or_diff($json, from_json('{"success": true, "linkGroups": []}'));

$json = cont3xtPut('/api/linkGroup/create', to_json({
  name => "Links1",
  viewRoles => ["superAdmin"],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "http://www.foo.com",
    itype => "ip"
  }]
}));
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

$json = cont3xtGet('/api/linkGroup/getViewable');
my $id = $json->{linkGroups}->[0]->{_id};
delete $json->{linkGroups}->[0]->{_id};
eq_or_diff($json, from_json('{"linkGroups":[{"creator":"anonymous","_editable":true,"viewRoles":["superAdmin"],"links":[{"url":"http://www.foo.com","itype":"ip","name":"foo1"}],"name":"Links1","editRoles":["superAdmin"]}],"success":true}'));

$json = cont3xtGet('/api/linkGroup/getEditable');
delete $json->{linkGroups}->[0]->{_id};
eq_or_diff($json, from_json('{"linkGroups":[{"creator":"anonymous","_viewable":true,"viewRoles":["superAdmin"],"links":[{"url":"http://www.foo.com","itype":"ip","name":"foo1"}],"name":"Links1","editRoles":["superAdmin"]}],"success":true}'));

$json = cont3xtPut("/api/linkGroup/update/$id", to_json({
  name => "Links1",
  viewRoles => ["cont3xtUser"],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "http://www.foobar.com",
    itype => "ip"
  }]
}));
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

$json = cont3xtGet('/api/linkGroup/getViewable');
my $id = $json->{linkGroups}->[0]->{_id};
delete $json->{linkGroups}->[0]->{_id};
eq_or_diff($json, from_json('{"linkGroups":[{"creator":"anonymous","_editable":true,"viewRoles":["cont3xtUser"],"links":[{"url":"http://www.foobar.com","itype":"ip","name":"foo1"}],"name":"Links1","editRoles":["superAdmin"]}],"success":true}'));

$json = cont3xtGet('/api/linkGroup/getEditable');
delete $json->{linkGroups}->[0]->{_id};
eq_or_diff($json, from_json('{"linkGroups":[{"creator":"anonymous","_viewable":true,"viewRoles":["cont3xtUser"],"links":[{"url":"http://www.foobar.com","itype":"ip","name":"foo1"}],"name":"Links1","editRoles":["superAdmin"]}],"success":true}'));

$json = cont3xtPut("/api/linkGroup/delete/$id", "{}");
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

$json = cont3xtGet('/api/linkGroup/getViewable');
eq_or_diff($json, from_json('{"success": true, "linkGroups": []}'));

$json = cont3xtGet('/api/linkGroup/getEditable');
eq_or_diff($json, from_json('{"success": true, "linkGroups": []}'));

$json = cont3xtGet('/api/roles');
eq_or_diff($json, from_json('{"success": true, "roles": ["arkimeAdmin","arkimeUser","cont3xtAdmin","cont3xtUser","parliamentAdmin","parliamentUser","superAdmin","usersAdmin","wiseAdmin","wiseUser"]}'));
