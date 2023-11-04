use Test::More tests => 47;
use Cwd;
use URI::Escape;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $pwd = "*/pcap";


sub get {
my ($url) = @_;

    my $json = viewerGet($url);
    my $mjson = multiGet($url);

    eq_or_diff($mjson, $json, "single doesn't match multi for $url", { context => 3 });

    return $json
}

    my $token = getTokenCookie();
    my $otherToken = getTokenCookie('historytest1');

# Clear all history
    esDelete("/tests*_history_v1*");
    sleep(1);
    esGet("/_flush");
    esGet("/_refresh");

# Make a request
    countTest(4, "arkimeRegressionUser=historytest1&date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=domainwise"));
    sleep(2);
    esGet("/_flush");
    esGet("/_refresh");

# See if recorded, should be the only item that is ours
    my $json = viewerGet("/api/histories?arkimeRegressionUser=historytest1");
    is ($json->{recordsFiltered}, 1, "Test1: recordsFiltered");
    my $item = $json->{data}->[0];
    is ($item->{expression}, "(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=domainwise", "Test1: expression");
    is ($item->{uiPage}, "sessions", "Test1: uiPage");
    is ($item->{api}, "/sessions.json", "Test1: api");
    is ($item->{query}, "arkimeRegressionUser=historytest1&date=-1&expression=(file=*/pcap/socks-https-example.pcap||file=*/pcap/dns-mx.pcap)&&tags=domainwise", "Test1: query");
    is ($item->{userId}, "historytest1", "Test1: userId");
    is ($item->{recordsReturned}, 4, "Test1: recordsReturned");
    is ($item->{recordsFiltered}, 4, "Test1: recordsFiltered");
    cmp_ok ($item->{recordsTotal}, '>=', 80, "recordsTotal");

# Check Multi
    my $mjson = multiGet("/api/histories?arkimeRegressionUser=historytest1");
    my $index = $json->{data}->[0]->{index};
    delete $json->{data}->[0]->{index};
    delete $mjson->{data}->[0]->{index};
    delete $mjson->{data}->[0]->{cluster};
    eq_or_diff($mjson, $json, "multi Test1", { context => 3 });
    $json->{data}->[0]->{index} = $index;

# Make sure another user doesn't see our history
    $json = get("/api/histories?arkimeRegressionUser=historytest2");
    is ($json->{recordsFiltered}, 0, "Test2: recordsFiltered");

# Make sure another user doesn't see our history when time given
    $json = get("/api/histories?arkimeRegressionUser=historytest2&startTime=0&stopTime=2147483647");
    is ($json->{recordsFiltered}, 0, "Test2: recordsFiltered");

# Make sure can't request someone elses
    $json = get("/api/histories?arkimeRegressionUser=historytest2&userId=historytest1");
    eq_or_diff($json, from_json('{"success": false, "text": "Need admin privileges"}'));

# Make sure can't request ours with wildcard
    $json = get("/api/histories?arkimeRegressionUser=historytest2&userId=historytest2*");
    eq_or_diff($json, from_json('{"success": false, "text": "Need admin privileges"}'));

# An admin user should see everything, find it
    $json = viewerGet("/api/histories");
    cmp_ok ($json->{recordsFiltered}, ">=", 1, "Test3: recordsFiltered");

    my $found = 0;
    foreach my $item2 (@{$json->{data}}) {
        if ($item2->{id} eq $item->{id}) {
            $found = 1;
            last;
        }
    }
    is ($found, 1, "Test3: Found id in all");

# Should be able to pass in userId and api params
    $json = viewerGet("/api/histories?userId=historytest1&api=sessions");
    is ($json->{recordsFiltered}, 1, "Test4: recordsFiltered");
    my $item = $json->{data}->[0];
    is ($item->{api}, "/sessions.json", "Test4: api");
    $json = viewerGet("/api/histories?userId=historytest1&api=somethingsilly");
    is ($json->{recordsFiltered}, 0, "Test4: recordsFiltered");
    $json = viewerGet("/api/histories?userId=somethingsilly&api=sessions");
    is ($json->{recordsFiltered}, 0, "Test4: recordsFiltered");

    $json = viewerGet("/api/histories?userId=historytest1*&api=sessions");
    is ($json->{recordsFiltered}, 1, "Test4b: recordsFiltered");

    $json = viewerGet("/api/histories?userId=h*&api=sessions");
    is ($json->{recordsFiltered}, 1, "Test4c: recordsFiltered");

    $json = viewerGet("/api/histories?userId=*i*&api=sessions");
    is ($json->{recordsFiltered}, 1, "Test4d: recordsFiltered");

    $json = viewerGet("/api/histories?userId=*j*&api=sessions");
    is ($json->{recordsFiltered}, 0, "Test4e: recordsFiltered");

# Should be able to filter by time range
    my $current = time;
    my $pastHour = $current - 3600;
    $json = viewerGet("/api/histories?userId=historytest1&startTime=$pastHour&stopTime=$current");
    is ($json->{recordsFiltered}, 1, "Test5: recordsFiltered");
    $json = viewerGet("/api/histories?userId=historytest1&startTime=18000&stopTime=$pastHour");
    is ($json->{recordsFiltered}, 0, "Test5: recordsFiltered");

# Can't delete items when not admin
    $json = viewerDeleteToken("/api/history/$item->{id}?arkimeRegressionUser=historytest1", $otherToken);
    eq_or_diff($json, from_json('{"success": false, "text": "You do not have permission to access this resource"}', {relaxed => 1}), "Test Delete Not Admin", { context => 3 });

# Delete item no index
    $json = viewerDeleteToken("/api/history/$item->{id}", $token);
    eq_or_diff($json, from_json('{"success": false, "text": "Missing history index"}', {relaxed => 1}), "Test Delete No Index", { context => 3 });

# Delete item
    $json = viewerDeleteToken("/api/history/$item->{id}?index=$item->{index}", $token);
    eq_or_diff($json, from_json('{"success": true, "text": "Deleted history item successfully"}', {relaxed => 1}), "Test Delete", { context => 3 });
    esGet("/_refresh");

# Make sure gone
    $json = get("/api/histories?arkimeRegressionUser=historytest1");
    is ($json->{recordsFiltered}, 0, "Should be no items");

# An admin user should see forced expressions for users
    # create a user with a forced expression
    $json = viewerPostToken("/api/user", '{"userId": "sac-historytest3", "userName": "UserName", "enabled":true, "password":"password","expression":"protocols == udp", "roles": ["arkimeUser"]}', $token);
    sleep(1);
    esGet("/_refresh");
    esGet("/_flush");

    # issue a request as the user with the forced expression
    countTest(1, "arkimeRegressionUser=sac-historytest3&date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=domainwise"));
    sleep(2);
    esGet("/_refresh");
    esGet("/_flush");

    # find and delete the user/create history item
    $json = viewerGet("/api/histories?arkimeRegressionUser=anonymous");
    $item = $json->{data}->[0];
    $json = viewerDeleteToken("/api/history/$item->{id}?index=$item->{index}", $token);

    # confirm that the forced expression is visible to admin
    $json = viewerGet("/api/histories");
    my $found2 = 0;
    foreach my $item3 (@{$json->{data}}) {
        if ($item3->{forcedExpression} eq "(protocols == udp)") {
            $found2 = 1;
            last;
        }
    }
    is ($found2, 1, "Admin should see forcedExpression in history");

# A nonadmin user should not see forcedExpression
  $json = viewerGet("/api/histories?arkimeRegressionUser=sac-historytest3");

  my $found3 = 0;
  foreach my $item4 (@{$json->{data}}) {
      if ($item4->{forcedExpression} eq "(protocols == udp)") {
          $found3 = 1;
          last;
      }
  }
  is ($found3, 0, "Non admin should not see forcedExpression in history");

  # delete the history item with the forced expression
  $item = $json->{data}->[0];
  $json = viewerDeleteToken("/api/history/$item->{id}?index=$item->{index}", $token);

# Delete Users
    $json = viewerDeleteToken("/api/user/historytest1", $token);
    sleep(1);
    esGet("/_refresh");
    esGet("/_flush");

    $json = viewerDeleteToken("/api/user/sac-historytest3", $token);
    sleep(1);
    esGet("/_refresh");
    esGet("/_flush");

# Check history for delete
    $json = viewerGet("/api/histories?arkimeRegressionUser=anonymous&sortField=timestamp&desc=true");
    is ($json->{recordsFiltered}, 2, "Delete: recordsFiltered");
    $item = $json->{data}->[0];
    is ($item->{api}, "/api/user/sac-historytest3", "Delete: api");
    is ($item->{userId}, "anonymous", "Delete: userId");
    ok (!exists $item->{body}->{password}, "Delete: should have no password item");
    ok (!exists $item->{body}->{newPassword}, "Delete: should have no newPassword item");
    ok (!exists $item->{body}->{currentPassword}, "Delete: should have no currentPassword item");
