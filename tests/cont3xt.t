# Test cont3xt.js
use Test::More tests => 139;
use Test::Differences;
use Data::Dumper;
use MolochTest;
use JSON;
use strict;

esPost("/cont3xt_links/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }');
esPost("/cont3xt_overviews/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }');
esPost("/cont3xt_history/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }');
esPost("/cont3xt_views/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }');

my $token = getCont3xtTokenCookie();

# create test cont3xtUser and get their token
viewerPostToken("/api/user", '{"userId": "test", "userName": "test", "enabled":true, "password":"password", "roles":["cont3xtUser"]}', $token);
my $token2 = getTokenCookie('test');

my $json;

### LINK GROUPS
# Make sure delete worked
$json = cont3xtGet('/api/linkGroup');
eq_or_diff($json, from_json('{"success": true, "linkGroups": []}'));

# Bad data
$json = cont3xtPutToken("/api/linkGroup", to_json({
  name => "Links1",
  viewRoles => 1,
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "http://www.foobar.com",
    itypes => ["ip", "hash"]
  }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "viewRoles must be an array of strings"}'));

$json = cont3xtPutToken("/api/linkGroup", to_json({
  name => "Links1",
  viewRoles => [1],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "http://www.foobar.com",
    itypes => ["ip", "hash"]
  }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "viewRoles must be an array of strings"}'));

$json = cont3xtPutToken("/api/linkGroup", to_json({
  name => "Links1",
  viewRoles => ["cont3xtUser"],
  editRoles => 1,
  links => [{
    name => "foo1",
    url => "http://www.foobar.com",
    itypes => ["ip", "hash"]
  }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "editRoles must be an array of strings"}'));

$json = cont3xtPutToken("/api/linkGroup", to_json({
  name => "Links1",
  viewRoles => ["cont3xtUser"],
  editRoles => [1],
  links => [{
    name => "foo1",
    url => "http://www.foobar.com",
    itypes => ["ip", "hash"]
  }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "editRoles must be an array of strings"}'));

$json = cont3xtPutToken("/api/linkGroup", to_json({
  name => "Links1",
  viewRoles => ["cont3xtUser"],
  editRoles => ["superAdmin"],
  links => [1]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Link must be object"}'));

$json = cont3xtPutToken("/api/linkGroup", to_json({
  name => "Links1",
  viewRoles => ["cont3xtUser"],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "http://www.foobar.com",
    itypes => 1
  }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Link missing itypes"}'));

$json = cont3xtPutToken("/api/linkGroup", to_json({
  name => "Links1",
  viewRoles => ["cont3xtUser"],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "http://www.foobar.com",
    itypes => [1]
  }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Link itypes must be strings"}'));

$json = cont3xtPutToken("/api/linkGroup", to_json({
  name => "Links1",
  viewRoles => ["cont3xtUser"],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "http://www.foobar.com",
    itypes => ["ip", "hash"],
    infoField => 1
  }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Link infoField must be a string"}'));

$json = cont3xtPutToken("/api/linkGroup", to_json({
  name => "Links1",
  viewRoles => ["cont3xtUser"],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "http://www.foobar.com",
    itypes => ["ip", "hash"],
    externalDocName => 1
  }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Link externalDocName must be a string"}'));

$json = cont3xtPutToken("/api/linkGroup", to_json({
  name => "Links1",
  viewRoles => ["cont3xtUser"],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "http://www.foobar.com",
    itypes => ["ip", "hash"],
    externalDocUrl => 1
  }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Link externalDocUrl must be a string"}'));

# update link group requires token
$json = cont3xtPut('/api/linkGroup', to_json({
  viewRoles => ["superAdmin"],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "http://www.foo.com",
    itypes => ["ip", "domain"]
  }]
}));
eq_or_diff($json, from_json('{"success": false, "text": "Missing token"}'));

$json = cont3xtPutToken('/api/linkGroup', to_json({
  viewRoles => ["superAdmin"],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "http://www.foo.com",
    itypes => ["ip", "domain"]
  }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Missing name"}'));

$json = cont3xtPutToken('/api/linkGroup', to_json({
  name => "Links1",
  viewRoles => ["superAdmin"],
  editRoles => ["superAdmin"]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Missing list of links"}'));

$json = cont3xtPutToken('/api/linkGroup', to_json({
  name => "Links1",
  viewRoles => ["superAdmin"],
  editRoles => ["superAdmin"],
  links => [{
    url => "http://www.foo.com",
    itypes => ["ip", "domain"]
  }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Link missing name"}'));

$json = cont3xtPutToken('/api/linkGroup', to_json({
  name => "Links1",
  viewRoles => ["superAdmin"],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    itypes => ["ip", "domain"]
  }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Link missing url"}'));

$json = cont3xtPutToken('/api/linkGroup', to_json({
  name => "Links1",
  viewRoles => ["superAdmin"],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "http://www.foo.com"
  }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Link missing itypes"}'));

$json = cont3xtPutToken('/api/linkGroup', to_json({
  name => "Links1",
  viewRoles => ["superAdmin"],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "http://www.foo.com",
    itypes => ["ip", "domain"]
  }]
}), $token);
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

$json = cont3xtGet('/api/linkGroup');
my $id = $json->{linkGroups}->[0]->{_id};
delete $json->{linkGroups}->[0]->{_id};
eq_or_diff($json, from_json('{"linkGroups":[{"creator":"anonymous","_editable":true,"_viewable": true, "viewRoles":["superAdmin"],"links":[{"url":"http://www.foo.com","itypes":["ip", "domain"],"name":"foo1"}],"name":"Links1","editRoles":["superAdmin"]}],"success":true}'));

$json = cont3xtPutToken("/api/linkGroup/$id", to_json({
  name => "Links1",
  viewRoles => ["cont3xtUser"],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "http://www.foobar.com",
    itypes => ["ip", "hash"]
  }]
}), $token);
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

$json = cont3xtGet('/api/linkGroup');
my $id = $json->{linkGroups}->[0]->{_id};
delete $json->{linkGroups}->[0]->{_id};
eq_or_diff($json, from_json('{"linkGroups":[{"creator":"anonymous","_editable":true,"_viewable":true,"viewRoles":["cont3xtUser"],"links":[{"url":"http://www.foobar.com","itypes":["ip", "hash"],"name":"foo1"}],"name":"Links1","editRoles":["superAdmin"]}],"success":true}'));

# can't transfer ownership (not admin or creator)
$json = cont3xtPutToken("/api/linkGroup/$id?molochRegressionUser=test", to_json({
  name => "Links1",
  viewRoles => ["cont3xtUser"],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "http://www.foobar.com",
    itypes => ["ip", "hash"]
  }],
  creator => "test"
}), $token2);
eq_or_diff($json, from_json('{"success": false, "text": "Permission denied"}'));

# can't transfer ownership to invalid user
$json = cont3xtPutToken("/api/linkGroup/$id", to_json({
  name => "Links1",
  viewRoles => ["cont3xtUser"],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "http://www.foobar.com",
    itypes => ["ip", "hash"]
  }],
  creator => "asdf"
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Invalid user: asdf"}'));

# can transfer ownership to valid user
$json = cont3xtPutToken("/api/linkGroup/$id", to_json({
  name => "Links1",
  viewRoles => ["cont3xtUser"],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "http://www.foobar.com",
    itypes => ["ip", "hash"]
  }],
  creator => "test"
}), $token);
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

# delete link group requires token
$json = cont3xtDelete("/api/linkGroup/$id?molochRegressionUser=test", "{}");
eq_or_diff($json, from_json('{"success": false, "text": "Missing token"}'));

$json = cont3xtDeleteToken("/api/linkGroup/$id?molochRegressionUser=test", "{}", $token2);
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

$json = cont3xtDeleteToken("/api/linkGroup/foo", "{}", $token);
eq_or_diff($json, from_json('{"success": false, "text": "LinkGroup not found"}'));

$json = cont3xtGet('/api/linkGroup');
eq_or_diff($json, from_json('{"success": true, "linkGroups": []}'));

### OVERVIEWS
$json = cont3xtGet('/api/overview');
eq_or_diff($json, from_json('{"success": true, "overviews": []}'));

# Bad data
$json = cont3xtPutToken("/api/overview", to_json({
    title => "Overview of %{query}",
    iType => "domain",
    viewRoles => ["cont3xtUser"],
    editRoles => ["superAdmin"],
    fields => [{
        type => "linked",
        from => "Foo",
        field => "foo_field"
    }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Missing name"}'));

$json = cont3xtPutToken("/api/overview", to_json({
    name => "Overview1",
    iType => "domain",
    viewRoles => ["cont3xtUser"],
    editRoles => ["superAdmin"],
    fields => [{
        type => "linked",
        from => "Foo",
        field => "foo_field"
    }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Missing title"}'));

$json = cont3xtPutToken("/api/overview", to_json({
    name => "Overview1",
    title => "Overview of %{query}",
    viewRoles => ["cont3xtUser"],
    editRoles => ["superAdmin"],
    fields => [{
        type => "linked",
        from => "Foo",
        field => "foo_field"
    }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Missing iType"}'));

$json = cont3xtPutToken("/api/overview", to_json({
    name => "Overview1",
    title => "Overview of %{query}",
    iType => "foobar",
    viewRoles => ["cont3xtUser"],
    editRoles => ["superAdmin"],
    fields => [{
        type => "linked",
        from => "Foo",
        field => "foo_field"
    }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Invalid iType"}'));

$json = cont3xtPutToken("/api/overview", to_json({
    name => "Overview1",
    title => "Overview of %{query}",
    iType => "domain",
    viewRoles => ["cont3xtUser"],
    editRoles => ["superAdmin"]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Missing fields array"}'));

$json = cont3xtPutToken("/api/overview", to_json({
    name => "Overview1",
    title => "Overview of %{query}",
    iType => "domain",
    viewRoles => ["cont3xtUser"],
    editRoles => ["superAdmin"],
    fields => 1
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "fields array must be an array"}'));

$json = cont3xtPutToken("/api/overview", to_json({
    name => "Overview1",
    title => "Overview of %{query}",
    iType => "domain",
    viewRoles => ["cont3xtUser"],
    editRoles => ["superAdmin"],
    fields => [1]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Field must be object"}'));

$json = cont3xtPutToken("/api/overview", to_json({
    name => "Overview1",
    title => "Overview of %{query}",
    iType => "domain",
    viewRoles => ["cont3xtUser"],
    editRoles => ["superAdmin"],
    fields => [{
        from => "Foo",
        field => "foo_field"
    }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Field type must be either \"linked\" or \"custom\""}'));

$json = cont3xtPutToken("/api/overview", to_json({
    name => "Overview1",
    title => "Overview of %{query}",
    iType => "domain",
    viewRoles => ["cont3xtUser"],
    editRoles => ["superAdmin"],
    fields => [{
        type => "linked",
        field => "foo_field"
    }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Field missing from"}'));

$json = cont3xtPutToken("/api/overview", to_json({
    name => "Overview1",
    title => "Overview of %{query}",
    iType => "domain",
    viewRoles => ["cont3xtUser"],
    editRoles => ["superAdmin"],
    fields => [{
        type => "linked",
        from => "Foo"
    }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Linked field missing field"}'));

$json = cont3xtPutToken("/api/overview", to_json({
    name => "Overview1",
    title => "Overview of %{query}",
    iType => "domain",
    viewRoles => ["cont3xtUser"],
    editRoles => ["superAdmin"],
    fields => [{
        type => "linked",
        from => "Foo",
        field => "foo_field",
        alias => 1
    }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Linked field alias must be a string or undefined"}'));

$json = cont3xtPutToken("/api/overview", to_json({
    name => "Overview1",
    title => "Overview of %{query}",
    iType => "domain",
    viewRoles => ["cont3xtUser"],
    editRoles => ["superAdmin"],
    fields => [{
        type => "custom",
        from => "Foo"
    }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Custom field must be a string or object"}'));

$json = cont3xtPutToken("/api/overview", to_json({
    name => "Overview1",
    title => "Overview of %{query}",
    iType => "domain",
    viewRoles => ["cont3xtUser"],
    editRoles => ["superAdmin"],
    fields => [{
        type   => "custom",
        from => "Foo",
        custom => 1
    }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Custom field must be a string or object"}'));

$json = cont3xtPutToken("/api/overview", to_json({
    name => "Overview1",
    title => "Overview of %{query}",
    iType => "domain",
    viewRoles => ["cont3xtUser"],
    editRoles => ["superAdmin"],
    fields => [{
        type   => "custom",
        from => "Foo",
        custom => {
            label => "foo name",
            field => "foo.bar"
        },
        field  => "baz"
    }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Custom field must not have field"}'));

$json = cont3xtPutToken("/api/overview", to_json({
    name => "Overview1",
    title => "Overview of %{query}",
    iType => "domain",
    viewRoles => ["cont3xtUser"],
    editRoles => ["superAdmin"],
    fields => [{
        type   => "custom",
        from => "Foo",
        custom => {
            label => "foo name",
            field => "foo.bar"
        },
        alias  => "foo-ier name"
    }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Custom field must not have alias"}'));

$json = cont3xtPutToken("/api/overview", to_json({
    name => "Overview1",
    title => "Overview of %{query}",
    iType => "domain",
    viewRoles => 1,
    editRoles => ["superAdmin"],
    fields => [{
        type => "linked",
        from => "Foo",
        field => "foo_field"
    }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "viewRoles must be an array of strings"}'));

$json = cont3xtPutToken("/api/overview", to_json({
    name => "Overview1",
    title => "Overview of %{query}",
    iType => "domain",
    viewRoles => [1],
    editRoles => ["superAdmin"],
    fields => [{
        type => "linked",
        from => "Foo",
        field => "foo_field"
    }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "viewRoles must be an array of strings"}'));

$json = cont3xtPutToken("/api/overview", to_json({
    name => "Overview1",
    title => "Overview of %{query}",
    iType => "domain",
    viewRoles => ["cont3xtUser"],
    editRoles => 1,
    fields => [{
        type => "linked",
        from => "Foo",
        field => "foo_field"
    }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "editRoles must be an array of strings"}'));

$json = cont3xtPutToken("/api/overview", to_json({
    name => "Overview1",
    title => "Overview of %{query}",
    iType => "domain",
    viewRoles => ["cont3xtUser"],
    editRoles => [1],
    fields => [{
        type => "linked",
        from => "Foo",
        field => "foo_field"
    }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "editRoles must be an array of strings"}'));

# update overview requires token
$json = cont3xtPut('/api/overview', to_json({
    name => "Overview1",
    title => "Overview of %{query}",
    iType => "domain",
    viewRoles => ["cont3xtUser"],
    editRoles => ["superAdmin"],
    fields => [{
        type => "linked",
        from  => "Foo",
        field => "foo_field"
    }]
}));
eq_or_diff($json, from_json('{"success": false, "text": "Missing token"}'));

$json = cont3xtPutToken('/api/overview', to_json({
    name => "Overview1",
    title => "Overview of %{query}",
    iType => "domain",
    viewRoles => ["superAdmin"],
    editRoles => ["superAdmin"],
    fields => [{
        type => "linked",
        from  => "Foo",
        field => "foo_field"
    }, {
        type => "custom",
        from => "Foo",
        custom => {
            field  => "foo.bar",
            label  => "Foo Bar",
            type   => "array"
        }
    }]
}), $token);
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

$json = cont3xtGet('/api/overview');
my $id = $json->{overviews}->[0]->{_id};
delete $json->{overviews}->[0]->{_id};
eq_or_diff($json, from_json('{"overviews":[{"creator":"anonymous","_editable":true,"_viewable": true, "viewRoles":["superAdmin"],"fields":[{"type":"linked","from":"Foo","field":"foo_field"},{"type":"custom","from":"Foo","custom":{"field":"foo.bar","label":"Foo Bar","type":"array"}}],"name":"Overview1","title":"Overview of %{query}","iType":"domain","editRoles":["superAdmin"]}],"success":true}'));

$json = cont3xtPutToken("/api/overview/$id", to_json({
    name => "Overview1 v2",
    title => "Overview v2 of %{query}",
    iType => "ip",
    viewRoles => ["cont3xtUser"],
    editRoles => ["superAdmin"],
    fields => [{
        type => "linked",
        from  => "Foo",
        field => "bar_field"
    }, {
        type   => "custom",
        from  => "Foo",
        custom => "foo.bar"
    }]
}), $token);
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

$json = cont3xtGet('/api/overview');
my $id = $json->{overviews}->[0]->{_id};
delete $json->{overviews}->[0]->{_id};
eq_or_diff($json, from_json('{"overviews":[{"creator":"anonymous","_editable":true,"_viewable": true, "viewRoles":["cont3xtUser"],"fields":[{"type":"linked","from":"Foo","field":"bar_field"},{"type":"custom","from":"Foo","custom":"foo.bar"}],"name":"Overview1 v2","title":"Overview v2 of %{query}","iType":"ip","editRoles":["superAdmin"]}],"success":true}'));

# delete overview requires token
$json = cont3xtDelete("/api/overview/$id", "{}");
eq_or_diff($json, from_json('{"success": false, "text": "Missing token"}'));

$json = cont3xtDeleteToken("/api/overview/$id", "{}", $token);
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

$json = cont3xtDeleteToken("/api/overview/foo", "{}", $token);
eq_or_diff($json, from_json('{"success": false, "text": "Overview not found"}'));

$json = cont3xtGet('/api/overview');
eq_or_diff($json, from_json('{"success": true, "overviews": []}'));

### ROLES
$json = cont3xtGet('/api/roles');
eq_or_diff($json, from_json('{"success": false, "text": "Missing token"}'));

$json = cont3xtGetToken('/api/roles', $token);
eq_or_diff($json, from_json('{"success": true, "roles": ["arkimeAdmin","arkimeUser","cont3xtAdmin","cont3xtUser","parliamentAdmin","parliamentUser","superAdmin","usersAdmin","wiseAdmin","wiseUser"]}'));

### INTEGRATION
$json = cont3xtGet('/api/integration');
is ($json->{success}, 1);
cmp_ok (keys %{$json->{integrations}}, ">", 10);

$json = cont3xtPost('/api/integration/search', to_json({
  query => "example.com"
}));

is($json->[0]->{purpose}, "init");
is($json->[0]->{sent}, 0);
is($json->[0]->{text}, "more to follow");
is(scalar @{$json->[0]->{indicators}}, 1);
is($json->[0]->{indicators}->[0]->{itype}, "domain");
is($json->[0]->{indicators}->[0]->{query}, "example.com");
cmp_ok (scalar @{$json}, ">", 8);

$json = cont3xtPost('/api/integration/search', to_json({
  query => "example.com",
  doIntegrations => ["dns"]
}));

is($json->[0]->{purpose}, "init");
is($json->[0]->{sent}, 0);
is($json->[0]->{text}, "more to follow");
is(scalar @{$json->[0]->{indicators}}, 1);
is($json->[0]->{indicators}->[0]->{itype}, "domain");
is($json->[0]->{indicators}->[0]->{query}, "example.com");
is($json->[1]->{purpose}, "finish");
is($json->[1]->{resultCount}, 0);
is (scalar @{$json}, 2);

$json = cont3xtPost('/api/integration/search', to_json({
  query => "example.com, 1.1.1.1",
  tags => ["goodtag"],
  doIntegrations => ["DNS"]
}));

is($json->[0]->{purpose}, "init"); # initial integration chunk
is($json->[0]->{sent}, 0);
is($json->[0]->{text}, "more to follow");
is(scalar @{$json->[0]->{indicators}}, 2);
is($json->[0]->{indicators}->[0]->{itype}, "domain");
is($json->[0]->{indicators}->[0]->{query}, "example.com");
is($json->[0]->{indicators}->[1]->{itype}, "ip");
is($json->[0]->{indicators}->[1]->{query}, "1.1.1.1");

is($json->[1]->{purpose}, "enhance");
is($json->[1]->{indicator}->{itype}, "ip");
is($json->[2]->{purpose}, "link");
is($json->[2]->{indicator}->{itype}, "ip");
is($json->[2]->{parentIndicator}->{query}, "example.com");
is($json->[2]->{parentIndicator}->{itype}, "domain");

is($json->[5]->{purpose}, "data");
is($json->[5]->{indicator}->{query}, "example.com");
is($json->[5]->{indicator}->{itype}, "domain");

is($json->[6]->{purpose}, "finish"); # last integration chunk
is($json->[6]->{resultCount}, 0);
is (scalar @{$json}, 7);

$json = cont3xtPost('/api/integration/search', to_json({
  query => "example.com",
  tags => "badtag",
  doIntegrations => ["DNS"]
}));
eq_or_diff($json, from_json('{"purpose": "error", "text": "tags must be an array when present"}'));

$json = cont3xtPost('/api/integration/search', to_json({
  query => "example.com",
  tags => [1],
  doIntegrations => ["DNS"]
}));
eq_or_diff($json, from_json('{"purpose": "error", "text": "every tag must be a string"}'));

$json = cont3xtPost('/api/integration/search', to_json({
  query => "example.com",
  doIntegrations => 1
}));
eq_or_diff($json, from_json('{"purpose": "error", "text": "doIntegrations must be an array when present"}'));

$json = cont3xtPost('/api/integration/search', to_json({
  query => "example.com",
  doIntegrations => [1]
}));
eq_or_diff($json, from_json('{"purpose": "error", "text": "every doIntegration must be a string"}'));

$json = cont3xtPost('/api/integration/search', to_json({
  query => "example.com",
  viewId => 1
}));
eq_or_diff($json, from_json('{"purpose": "error", "text": "viewId must be a string when present"}'));

$json = cont3xtPost('/api/integration/search', to_json({
    query => ","
}));
eq_or_diff($json, from_json('{"purpose": "error", "text": "query must contain at least one non-whitespace indicator"}'));

esGet("/_flush");
esGet("/_refresh");

### HISTORY
$json = cont3xtGet('/api/audits?searchTerm=goodtag');
is($json->{success}, 1);
is (scalar @{$json->{audits}}, 1);

$json = cont3xtGet('/api/audits?searchTerm=foo');
is($json->{success}, 1);
is (scalar @{$json->{audits}}, 0);

$json = cont3xtGet('/api/audits');
is($json->{success}, 1);
is (scalar @{$json->{audits}}, 3);
$id = $json->{audits}->[0]->{_id};

$json = cont3xtDelete("/api/audit/$id", '{}');
eq_or_diff($json, from_json('{"success": false, "text": "Missing token"}'));

$json = cont3xtDeleteToken("/api/audit/$id", '{}', $token);
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

$json = cont3xtDeleteToken('/api/audit/foo', '{}', $token);
eq_or_diff($json, from_json('{"success": false, "text": "History log not found"}'));

$json = cont3xtGet('/api/audits');
is($json->{success}, 1);
is (scalar @{$json->{audits}}, 2);

### VIEWS
# Bad
$json = cont3xtPostToken('/api/view', to_json({
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Missing name"}'));

$json = cont3xtPostToken('/api/view', to_json({
  name => "view1", viewRoles => 1
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "viewRoles must be array"}'));

$json = cont3xtPostToken('/api/view', to_json({
  name => "view1", viewRoles => [1]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "viewRoles must contain strings"}'));

$json = cont3xtPostToken('/api/view', to_json({
  name => "view1", editRoles => 1
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "editRoles must be array"}'));

$json = cont3xtPostToken('/api/view', to_json({
  name => "view1", editRoles => [1]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "editRoles must contain strings"}'));

$json = cont3xtPostToken('/api/view', to_json({
  name => "view1", integrations => 1
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "integrations must be array"}'));

$json = cont3xtPostToken('/api/view', to_json({
  name => "view1", integrations => [1]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "integrations must contain strings"}'));

# Good
$json = cont3xtPostToken('/api/view', to_json({
  name => "view1"
}), $token);
delete $json->{view}->{_id};
eq_or_diff($json, from_json('{"view":{"_viewable":true,"name":"view1","_editable":true,"creator":"anonymous"},"success":true,"text":"Success"}'));

$json = cont3xtPostToken('/api/view', to_json({
  name => "view2"
}), $token);
$id = $json->{view}->{_id};
delete $json->{view}->{_id};
eq_or_diff($json, from_json('{"success":true,"view":{"creator":"anonymous","_editable":true,"name":"view2","_viewable":true},"text":"Success"}'));

$json = cont3xtPutToken("/api/view/$id", to_json({
  name => "view2changed"
}), $token);
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

$json = cont3xtPutToken("/api/view/foo", to_json({
  name => "view2changed"
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "View not found"}'));

$json = cont3xtGet('/api/views');
is($json->{success}, 1);
is($json->{views}->[1]->{name}, "view2changed");
is (scalar @{$json->{views}}, 2);

$json = cont3xtDelete("/api/view/$id", '{}');
eq_or_diff($json, from_json('{"success": false, "text": "Missing token"}'));

$json = cont3xtDeleteToken("/api/view/$id", '{}', $token);
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

$json = cont3xtDeleteToken('/api/view/foo', '{}', $token);
eq_or_diff($json, from_json('{"success": false, "text": "View not found"}'));

$json = cont3xtGet('/api/views');
is($json->{success}, 1);
is (scalar @{$json->{views}}, 1);

### Settings
$json = cont3xtGet('/api/integration/settings');
ok($json->{success});

$json = cont3xtPut('/api/integration/settings', '{}');
eq_or_diff($json, from_json('{"success": false, "text": "Missing token"}'));

$json = cont3xtPutToken('/api/integration/settings', 'hi', $token);
is ($json, "SyntaxError: Unexpected token h in JSON at position 0");

$json = cont3xtPutToken('/api/integration/settings', '{"__proto__": {"foo": 1}}', $token);
is ($json, "SyntaxError: Object contains forbidden prototype property");

### Classify
$json = cont3xtPost('/regressionTests/classify', '["aol.com", "1.2.3.4", "a----b.com", "https://a----b.com", "703-867-5309", "text", "foo@example.com", "d07708229fb0d2d513c82f36e5cdc68f", "25425d55a6af7586bf68c3989f0d4d89ffbb1641"]');
eq_or_diff($json, from_json('["domain", "ip", "domain", "url", "phone", "text", "email", "hash", "hash"]'));

$json = cont3xtPost('/regressionTests/classify', '["415-123-4567", "415.123.4567","4151234567","+01133143122222", "+011.33.143122222", "+011-33-143122222", "+33143122222"]');
eq_or_diff($json, from_json('["phone", "phone", "phone", "phone", "phone", "phone", "phone"]'));
