# Test cont3xt.js
use Test::More tests => 218;
use Test::Differences;
use Data::Dumper;
use ArkimeTest;
use JSON;
use strict;

viewerGet("/regressionTests/deleteAllUsers");
cont3xtPost("/regressionTests/clearAll");

my $token = getCont3xtTokenCookie();

# create sac-test cont3xtUser and get their token
viewerPostToken("/api/user", '{"userId": "sac-test", "userName": "test", "enabled":true, "password":"password", "roles":["cont3xtUser"]}', $token);
my $token2 = getTokenCookie('sac-test');

# create users for TOTP tests
viewerPostToken("/api/user", '{"userId": "sac-nonadmin", "userName": "Non-Admin User", "enabled":true, "webEnabled":true, "password":"nonadminpass", "roles": ["cont3xtUser"]}', $token);
my $nonAdminToken = getCont3xtTokenCookie('sac-nonadmin');
addUser("-n testuser sac-totpuser sac-totpuser sac-totpuser --roles cont3xtAdmin,cont3xtUser");
my $totpToken = getCont3xtTokenCookie('sac-totpuser');
addUser("-n testuser sac-cont3xtadmin sac-cont3xtadmin sac-cont3xtadmin --roles cont3xtAdmin,cont3xtUser,usersAdmin");
my $adminToken = getCont3xtTokenCookie('sac-cont3xtadmin');

my $json;

# Check appversion
$json = cont3xtGet("/api/appversion");
is($json->{app}, "cont3xt", "cont3xt appversion app field");

################################################################################
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

# javascript: url should be rejected
$json = cont3xtPutToken('/api/linkGroup', to_json({
  name => "Links1",
  viewRoles => ["superAdmin"],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "javascript:alert(1)",
    itypes => ["ip", "domain"]
  }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Link url must start with http:// or https://"}'));

# separator url should be allowed
$json = cont3xtPutToken('/api/linkGroup', to_json({
  name => "Links1",
  viewRoles => ["superAdmin"],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "----------",
    itypes => ["ip", "domain"]
  }]
}), $token);
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

# clean up separator linkGroup
$json = cont3xtGet('/api/linkGroup');
my $sepId = $json->{linkGroups}->[0]->{_id};
$json = cont3xtDeleteToken("/api/linkGroup/$sepId", "{}", $token);

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
  }],
  creator => "anonymous"
}), $token);
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

$json = cont3xtGet('/api/linkGroup');
my $id = $json->{linkGroups}->[0]->{_id};
delete $json->{linkGroups}->[0]->{_id};
eq_or_diff($json, from_json('{"linkGroups":[{"creator":"anonymous","_editable":true,"_viewable":true,"viewRoles":["cont3xtUser"],"links":[{"url":"http://www.foobar.com","itypes":["ip", "hash"],"name":"foo1"}],"name":"Links1","editRoles":["superAdmin"]}],"success":true}'));

# can't transfer ownership (not admin or creator)
$json = cont3xtPutToken("/api/linkGroup/$id?arkimeRegressionUser=sac-test", to_json({
  name => "Links1",
  viewRoles => ["cont3xtUser"],
  editRoles => ["superAdmin"],
  links => [{
    name => "foo1",
    url => "http://www.foobar.com",
    itypes => ["ip", "hash"]
  }],
  creator => "sac-test"
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
eq_or_diff($json, from_json('{"success": false, "text": "User not found"}'));

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
  creator => "sac-test"
}), $token);
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

# delete link group requires token
$json = cont3xtDelete("/api/linkGroup/$id?arkimeRegressionUser=sac-test", "{}");
eq_or_diff($json, from_json('{"success": false, "text": "Missing token"}'));

$json = cont3xtDeleteToken("/api/linkGroup/$id?arkimeRegressionUser=sac-test", "{}", $token2);
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

$json = cont3xtDeleteToken("/api/linkGroup/foo", "{}", $token);
eq_or_diff($json, from_json('{"success": false, "text": "Unknown resource"}'));

$json = cont3xtGet('/api/linkGroup');
eq_or_diff($json, from_json('{"success": true, "linkGroups": []}'));

################################################################################
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

# deeply nested custom field should be rejected to prevent stack overflow DoS
my $deep = { field => "leaf", type => "string" };
for (my $i = 0; $i < 100; $i++) {
  $deep = { type => "table", fields => [$deep] };
}
$json = cont3xtPutToken("/api/overview", to_json({
    name => "DeepOverview",
    title => "Deep",
    iType => "domain",
    viewRoles => ["cont3xtUser"],
    editRoles => ["superAdmin"],
    fields => [{
        type => "custom",
        from => "Foo",
        custom => $deep
    }]
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "Custom field nested too deep"}'));

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

# can't transfer ownership (not admin or creator)
$json = cont3xtPutToken("/api/overview/$id?arkimeRegressionUser=sac-test", to_json({
    creator => "sac-test"
}), $token2);
eq_or_diff($json, from_json('{"success": false, "text": "Permission denied"}'));

# can't transfer ownership to invalid user
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
    }],
    creator => "asdf"
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "User not found"}'));

# can transfer ownership to valid user
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
    }],
    creator => "sac-test"
}), $token);
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

# delete overview requires token
$json = cont3xtDelete("/api/overview/$id", "{}");
eq_or_diff($json, from_json('{"success": false, "text": "Missing token"}'));

$json = cont3xtDeleteToken("/api/overview/$id?arkimeRegressionUser=sac-test", "{}", $token2);
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

$json = cont3xtDeleteToken("/api/overview/foo", "{}", $token);
eq_or_diff($json, from_json('{"success": false, "text": "Unknown resource"}'));

$json = cont3xtGet('/api/overview');
eq_or_diff($json, from_json('{"success": true, "overviews": []}'));

################################################################################
### ROLES
$json = cont3xtGet('/api/roles');
eq_or_diff($json, from_json('{"success": false, "text": "Missing token"}'));

$json = cont3xtGetToken('/api/roles', $token);
eq_or_diff($json, from_json('{"success": true, "roles": ["arkimeAdmin","arkimeUser","cont3xtAdmin","cont3xtUser","parliamentAdmin","parliamentUser","superAdmin","usersAdmin","wiseAdmin","wiseUser"]}'));

################################################################################
### INTEGRATION

# Full test - disable for now since it goes against real services
#$json = cont3xtGet('/api/integration');
#is ($json->{success}, 1);
#cmp_ok (keys %{$json->{integrations}}, ">", 10);

#$json = cont3xtPost('/api/integration/search', to_json({
#  query => "example.com"
#}));

#is($json->[0]->{purpose}, "init");
#is($json->[0]->{sent}, 0);
#is($json->[0]->{text}, "more to follow");
#is(scalar @{$json->[0]->{indicators}}, 1);
#is($json->[0]->{indicators}->[0]->{itype}, "domain");
#is($json->[0]->{indicators}->[0]->{query}, "example.com");
#cmp_ok (scalar @{$json}, ">", 8);

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

#diag Dumper($json);

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

my $size = scalar @{$json};

is($json->[$size - 2]->{purpose}, "data");
is($json->[$size - 2]->{indicator}->{query}, "example.com");
is($json->[$size - 2]->{indicator}->{itype}, "domain");
is($json->[$size - 2]->{data}->{_cont3xt}->{count}, 7);

is($json->[$size - 1]->{purpose}, "finish"); # last integration chunk
is($json->[$size - 1]->{resultCount}, 7);
is (scalar @{$json}, $size);

$json = cont3xtPost('/api/integration/search', to_json({
  query => "http://example.com/blah.html",
  doIntegrations => ["dns"]
}));
eq_or_diff($json->[1]->{indicator}, from_json('{"query": "example.com", "itype": "domain"}'));
eq_or_diff($json->[1]->{parentIndicator}, from_json('{"query": "http://example.com/blah.html", "itype": "url"}'));

$json = cont3xtPost('/api/integration/search', to_json({
  query => "http://8.8.8.8/blah.html",
  doIntegrations => ["dns"]
}));
eq_or_diff($json->[1]->{indicator}, from_json('{"query": "8.8.8.8", "itype": "ip"}'));
eq_or_diff($json->[1]->{parentIndicator}, from_json('{"query": "http://8.8.8.8/blah.html", "itype": "url"}'));


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
################################################################################
### SINGLE INTEGRATION
#/api/integration/:itype/:integration/search

# maxmind tests
$json = cont3xtPost('/api/integration/ip/foo/search', to_json({
  query => "8.8.8.8"
}));
eq_or_diff($json, from_json('{"purpose": "error", "text": "integration ip foo not found"}'));

$json = cont3xtPost('/api/integration/ip/bar/search', to_json({
  query => "8.8.8.8"
}));
eq_or_diff($json, from_json('{"purpose": "error", "text": "integration ip bar not found"}'));

# wrong itype for query value
$json = cont3xtPost('/api/integration/domain/Maxmind/search', to_json({
  query => "8.8.8.8"
}));
eq_or_diff($json, from_json('{"purpose": "error", "text": "query does not match itype domain"}'));

$json = cont3xtPost('/api/integration/ip/Maxmind/search', to_json({
  query => "8.8.8.8"
}));
is($json->{data}->{asn}->{autonomous_system_number}, 15169);
is($json->{data}->{country}->{country}->{names}->{en}, "United States");

# elasticsearch tests
$json = cont3xtPost('/api/integration/ip/elasticsearch:test/search', to_json({
  query => "10.0.0.1"
}));
is($json->{data}->{_cont3xt}->{count}, 11);
is($json->{data}->{hits}->[0]->{source}->{ip}, "10.0.0.1");
is($json->{data}->{hits}->[1]->{source}->{ip}, "10.0.0.1");

$json = cont3xtPost('/api/integration/ip/elasticsearch:test/search', to_json({
  query => "badip"
}));
is($json->{purpose}, "error");

# json tests
$json = cont3xtPost('/api/integration/ip/json:ipwise/search', to_json({
  query => "10.20.30.50"
}));
is($json->{data}->{_cont3xt}->{count}, 1);

$json = cont3xtPost('/api/integration/ip/json:ipwise/search', to_json({
  query => "2001:16d8:ffce:0010:aca8:353c:291d:a9b3"
}));
is($json->{data}->{_cont3xt}->{count}, 1);

# csv tests
$json = cont3xtPost('/api/integration/domain/csv:whois/search', to_json({
  query => "whois.apnic.net"
}));
is($json->{data}->{_cont3xt}->{count}, 51);

# csv tests
$json = cont3xtPost('/api/integration/ip/csv:rir/search', to_json({
  query => "8.8.8.8"
}));
is($json->{data}->{_cont3xt}->{count}, 1);

# wise tests
$json = cont3xtPost('/api/integration/ip/wise:test/search', to_json({
  query => "10.0.0.2"
}));
is($json->{data}->{_cont3xt}->{count}, 1);
is($json->{data}->{results}->[0]->{value}, "ipwisecsv");
is($json->{data}->{results}->[0]->{key}, "tags");

################################################################################
### HISTORY
$json = cont3xtGet('/api/audits?searchTerm=goodtag');
is($json->{success}, 1);
is (scalar @{$json->{audits}}, 1);

$json = cont3xtGet('/api/audits?searchTerm=foo');
is($json->{success}, 1);
is (scalar @{$json->{audits}}, 0);

$json = cont3xtGet('/api/audits');
is($json->{success}, 1);
is (scalar @{$json->{audits}}, 4);
$id = $json->{audits}->[0]->{_id};

$json = cont3xtDelete("/api/audit/$id", '{}');
eq_or_diff($json, from_json('{"success": false, "text": "Missing token"}'));

$json = cont3xtDeleteToken("/api/audit/$id", '{}', $token);
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

$json = cont3xtDeleteToken('/api/audit/foo', '{}', $token);
eq_or_diff($json, from_json('{"success": false, "text": "History log not found"}'));

$json = cont3xtGet('/api/audits');
is($json->{success}, 1);
is (scalar @{$json->{audits}}, 3);

# use actual issuedAt from results to build reliable date ranges
my @timestamps = sort map { $_->{issuedAt} } @{$json->{audits}};
my $minTime = $timestamps[0];
my $maxTime = $timestamps[-1];

# date range covering all audits
$json = cont3xtGet("/api/audits?startMs=" . ($minTime - 1) . "&stopMs=" . ($maxTime + 1));
is($json->{success}, 1);
is (scalar @{$json->{audits}}, 3, "all audits in range");

# date range before all audits
$json = cont3xtGet("/api/audits?startMs=" . ($minTime - 2000) . "&stopMs=" . ($minTime - 1000));
is($json->{success}, 1);
is (scalar @{$json->{audits}}, 0, "no audits before range");

# date range after all audits
$json = cont3xtGet("/api/audits?startMs=" . ($maxTime + 1000) . "&stopMs=" . ($maxTime + 2000));
is($json->{success}, 1);
is (scalar @{$json->{audits}}, 0, "no audits after range");

# sorting by issuedAt ascending
$json = cont3xtGet('/api/audits?sortBy=issuedAt&sortOrder=asc');
is($json->{success}, 1);
ok($json->{audits}->[0]->{issuedAt} <= $json->{audits}->[-1]->{issuedAt}, "sorted ascending by issuedAt");

# sorting by issuedAt descending
$json = cont3xtGet('/api/audits?sortBy=issuedAt&sortOrder=desc');
is($json->{success}, 1);
ok($json->{audits}->[0]->{issuedAt} >= $json->{audits}->[-1]->{issuedAt}, "sorted descending by issuedAt");

# pagination
$json = cont3xtGet('/api/audits?page=1&itemsPerPage=2');
is($json->{success}, 1);
is (scalar @{$json->{audits}}, 2, "page 1 has 2 items");
is ($json->{total}, 3, "total is still 3");

$json = cont3xtGet('/api/audits?page=2&itemsPerPage=2');
is($json->{success}, 1);
is (scalar @{$json->{audits}}, 1, "page 2 has 1 item");

# combined date range + search
$json = cont3xtGet("/api/audits?startMs=" . ($minTime - 1) . "&stopMs=" . ($maxTime + 1) . "&searchTerm=goodtag");
is($json->{success}, 1);
is (scalar @{$json->{audits}}, 1, "date range + searchTerm combined");
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
eq_or_diff($json, from_json('{"success": false, "text": "Unknown resource"}'));

$json = cont3xtGet('/api/views');
is($json->{success}, 1);
is($json->{views}->[1]->{name}, "view2changed");
is (scalar @{$json->{views}}, 2);

# can't transfer ownership (not admin or creator)
$json = cont3xtPutToken("/api/view/$id?arkimeRegressionUser=sac-test", to_json({
    creator => "sac-test"
}), $token2);
eq_or_diff($json, from_json('{"success": false, "text": "Permission denied"}'));

# can't transfer ownership to invalid user
$json = cont3xtPutToken("/api/view/$id", to_json({
    name => "view2changed",
    creator => "asdf"
}), $token);
eq_or_diff($json, from_json('{"success": false, "text": "User not found"}'));

# can transfer ownership to valid user
$json = cont3xtPutToken("/api/view/$id", to_json({
    name => "view2changed",
    creator => "sac-test"
}), $token);
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));


$json = cont3xtDelete("/api/view/$id", '{}');
eq_or_diff($json, from_json('{"success": false, "text": "Missing token"}'));

$json = cont3xtDeleteToken("/api/view/$id?arkimeRegressionUser=sac-test", '{}', $token2);
eq_or_diff($json, from_json('{"success": true, "text": "Success"}'));

$json = cont3xtDeleteToken('/api/view/foo', '{}', $token);
eq_or_diff($json, from_json('{"success": false, "text": "Unknown resource"}'));

$json = cont3xtGet('/api/views');
is($json->{success}, 1);
is (scalar @{$json->{views}}, 1);

################################################################################
### Settings
$json = cont3xtGet('/api/integration/settings');
ok($json->{success});
ok($json->{settings}->{"elasticsearch:test"}->{locked});
ok(!$json->{settings}->{Twilio}->{locked});

$json = cont3xtPut('/api/integration/settings', '{}');
eq_or_diff($json, from_json('{"success": false, "text": "Missing token"}'));

$json = cont3xtPutToken('/api/integration/settings', 'hi', $token);
ok($json =~ /SyntaxError: Unexpected token/);

$json = cont3xtPutToken('/api/integration/settings', '{"__proto__": {"foo": 1}}', $token);
is ($json, "SyntaxError: Object contains forbidden prototype property");

################################################################################
### General Settings (not integration settings)
$json = cont3xtGet('/api/settings');
is($json->{success}, 1, "get settings success");
ok(exists $json->{settings}, "settings field exists");
ok(exists $json->{linkGroup}, "linkGroup field exists");
ok(exists $json->{selectedOverviews}, "selectedOverviews field exists");

# PUT settings without token
$json = cont3xtPut('/api/settings', '{}');
eq_or_diff($json, from_json('{"success": false, "text": "Missing token"}'), "put settings missing token");

# PUT settings with nothing to change
$json = cont3xtPutToken('/api/settings', '{}', $token);
eq_or_diff($json, from_json('{"success": false, "text": "Nothing sent to change"}'), "put settings nothing to change");

# PUT settings with actual settings
$json = cont3xtPutToken('/api/settings', '{"settings": {"foo": "bar"}}', $token);
is($json->{success}, 1, "put settings success");

################################################################################
### Integration Stats
$json = cont3xtGet('/api/integration/stats');
is($json->{success}, 1, "integration stats success");
ok(exists $json->{stats}, "stats field exists");

################################################################################
### Health Check
$json = cont3xtGet('/api/health');
is($json->{success}, 1, "health check success");

################################################################################
### Classify
$json = cont3xtPost('/regressionTests/classify', '["aol.com", "1.2.3.4", "a----b.com", "https://a----b.com", "703-867-5309", "text", "foo@example.com", "d07708229fb0d2d513c82f36e5cdc68f", "25425d55a6af7586bf68c3989f0d4d89ffbb1641"]');
eq_or_diff($json, from_json('[{"itype":"domain"}, {"itype":"ip"}, {"itype":"domain"}, {"itype":"url"}, {"itype":"phone"}, {"itype":"text"}, {"itype":"email"}, {"itype":"hash"}, {"itype":"hash"}]'));

$json = cont3xtPost('/regressionTests/classify', '["415-123-4567", "415.123.4567","4151234567","+01133143122222", "+011.33.143122222", "+011-33-143122222", "+33143122222"]');
eq_or_diff($json, from_json('[{"itype":"phone"}, {"itype":"phone"}, {"itype":"phone"}, {"itype":"phone"}, {"itype":"phone"}, {"itype":"phone"}, {"itype":"phone"}]'));

$json = cont3xtPost('/regressionTests/classify', '["xn--yho-ela6g.com"]');
eq_or_diff($json, from_json('[{"itype":"domain", "decoded":"yáhoó.com"}]'));

################################################################################
### TOTP Tests
# Non-admin users should be denied TOTP access
$json = cont3xtGetToken("/api/user/totp/status?arkimeRegressionUser=sac-nonadmin", $nonAdminToken);
is($json->{success}, 0, "Non-admin cannot access TOTP status");

$json = cont3xtPostToken("/api/user/totp/setup?arkimeRegressionUser=sac-nonadmin", '{}', $nonAdminToken);
is($json->{success}, 0, "Non-admin cannot setup TOTP");

# Get TOTP status - should be not enabled
$json = cont3xtGetToken("/api/user/totp/status?arkimeRegressionUser=sac-totpuser", $totpToken);
ok($json->{success}, "TOTP status returns success");
is($json->{enabled}, 0, "TOTP not enabled initially");

# Setup TOTP - get QR code
$json = cont3xtPostToken("/api/user/totp/setup?arkimeRegressionUser=sac-totpuser", '{}', $totpToken);
ok($json->{success}, "TOTP setup returns success");
ok(defined $json->{secret}, "TOTP setup returns secret");
ok(defined $json->{qrCodeDataUrl}, "TOTP setup returns qrCodeDataUrl");
like($json->{qrCodeDataUrl}, qr/^data:image\/png;base64,/, "TOTP qrCodeDataUrl is data URL");
my $totpSecret = $json->{secret};

# Confirm TOTP with invalid code
$json = cont3xtPostToken("/api/user/totp/confirm?arkimeRegressionUser=sac-totpuser", '{"code": "000000"}', $totpToken);
is($json->{success}, 0, "TOTP confirm fails with invalid code");

# Generate valid TOTP code
my $validCode = generate_totp($totpSecret);

# Confirm TOTP with valid code
$json = cont3xtPostToken("/api/user/totp/confirm?arkimeRegressionUser=sac-totpuser", '{"code": "' . $validCode . '"}', $totpToken);
ok($json->{success}, "TOTP confirm succeeds with valid code");

# Get TOTP status - should be enabled now
$json = cont3xtGetToken("/api/user/totp/status?arkimeRegressionUser=sac-totpuser", $totpToken);
ok($json->{success}, "TOTP status returns success after enrollment");
is($json->{enabled}, 1, "TOTP enabled after confirm");

# Disable TOTP with invalid code
$json = cont3xtPostToken("/api/user/totp/disable?arkimeRegressionUser=sac-totpuser", '{"code": "000000"}', $totpToken);
is($json->{success}, 0, "TOTP disable fails with invalid code");

# Disable TOTP with valid code
$validCode = generate_totp($totpSecret);
$json = cont3xtPostToken("/api/user/totp/disable?arkimeRegressionUser=sac-totpuser", '{"code": "' . $validCode . '"}', $totpToken);
ok($json->{success}, "TOTP disable succeeds with valid code");

# Get TOTP status - should be not enabled again
$json = cont3xtGetToken("/api/user/totp/status?arkimeRegressionUser=sac-totpuser", $totpToken);
is($json->{enabled}, 0, "TOTP not enabled after disable");

# Try to disable TOTP when not enabled
$json = cont3xtPostToken("/api/user/totp/disable?arkimeRegressionUser=sac-totpuser", '{"code": "123456"}', $totpToken);
is($json->{success}, 0, "TOTP disable fails when not enabled");
like($json->{text}, qr/not enabled/i, "TOTP disable returns not enabled message");

# TOTP admin disable tests - Re-enroll the test user
$json = cont3xtPostToken("/api/user/totp/setup?arkimeRegressionUser=sac-totpuser", '{}', $totpToken);
ok($json->{success}, "TOTP re-setup for admin tests");
$totpSecret = $json->{secret};
$validCode = generate_totp($totpSecret);
$json = cont3xtPostToken("/api/user/totp/confirm?arkimeRegressionUser=sac-totpuser", '{"code": "' . $validCode . '"}', $totpToken);
ok($json->{success}, "TOTP re-confirm for admin tests");

# Admin enroll TOTP
$json = cont3xtPostToken("/api/user/totp/setup?arkimeRegressionUser=sac-cont3xtadmin", '{}', $adminToken);
ok($json->{success}, "Admin TOTP setup");
my $adminTotpSecret = $json->{secret};
my $adminValidCode = generate_totp($adminTotpSecret);
$json = cont3xtPostToken("/api/user/totp/confirm?arkimeRegressionUser=sac-cont3xtadmin", '{"code": "' . $adminValidCode . '"}', $adminToken);
ok($json->{success}, "Admin TOTP confirm");

# Admin trying to disable own TOTP without code - should fail
$json = cont3xtPostToken("/api/user/totp/disable?arkimeRegressionUser=sac-cont3xtadmin", '{}', $adminToken);
is($json->{success}, 0, "Admin cannot disable own TOTP without code");
like($json->{text}, qr/code required/i, "Admin disable own returns code required");

# Admin trying to disable own TOTP with wrong code - should fail
$json = cont3xtPostToken("/api/user/totp/disable?arkimeRegressionUser=sac-cont3xtadmin", '{"code": "000000"}', $adminToken);
is($json->{success}, 0, "Admin cannot disable own TOTP with wrong code");

# Admin can disable another user's TOTP without code
$json = cont3xtPostToken("/api/user/totp/disable?arkimeRegressionUser=sac-cont3xtadmin&userId=sac-totpuser", '{}', $adminToken);
ok($json->{success}, "Admin can disable other user TOTP without code");

# Verify other user's TOTP is disabled
$json = cont3xtGetToken("/api/user/totp/status?arkimeRegressionUser=sac-totpuser", $totpToken);
is($json->{enabled}, 0, "Other user TOTP disabled by admin");

# Clean up - disable admin TOTP with valid code
$adminValidCode = generate_totp($adminTotpSecret);
$json = cont3xtPostToken("/api/user/totp/disable?arkimeRegressionUser=sac-cont3xtadmin", '{"code": "' . $adminValidCode . '"}', $adminToken);
ok($json->{success}, "Admin disable own TOTP with valid code");
