use Test::More tests => 42;
use Cwd;
use URI::Escape;
use MolochTest;
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
    sleep(1);

# Make a request
    esGet("/_flush");
    esGet("/_refresh");
    countTest(4, "molochRegressionUser=historytest1&date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=domainwise"));
    sleep(1);
    esGet("/_flush");
    esGet("/_refresh");
    sleep(1);

# See if recorded, should be the only item that is ours
    my $json = viewerGet("/history/list?molochRegressionUser=historytest1");
    is ($json->{recordsFiltered}, 1, "Test1: recordsFiltered");
    my $item = $json->{data}->[0];
    is ($item->{expression}, "(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=domainwise", "Test1: expression");
    is ($item->{uiPage}, "sessions", "Test1: uiPage");
    is ($item->{api}, "/sessions.json", "Test1: api");
    is ($item->{query}, "molochRegressionUser=historytest1&date=-1&expression=(file=*/pcap/socks-https-example.pcap||file=*/pcap/dns-mx.pcap)&&tags=domainwise", "Test1: query");
    is ($item->{userId}, "historytest1", "Test1: userId");
    is ($item->{recordsReturned}, 4, "Test1: recordsReturned");
    is ($item->{recordsFiltered}, 4, "Test1: recordsFiltered");
    cmp_ok ($item->{recordsTotal}, '>=', 80, "recordsTotal");

# Check Multi
    my $mjson = multiGet("/api/histories?molochRegressionUser=historytest1");
    my $index = $json->{data}->[0]->{index};
    delete $json->{data}->[0]->{index};
    delete $mjson->{data}->[0]->{index};
    delete $mjson->{data}->[0]->{cluster};
    eq_or_diff($mjson, $json, "multi Test1", { context => 3 });
    $json->{data}->[0]->{index} = $index;

# Make sure another user doesn't see our history
    $json = get("/history/list?molochRegressionUser=historytest2");
    is ($json->{recordsFiltered}, 0, "Test2: recordsFiltered");

# Make sure another user doesn't see our history when time given
    $json = get("/history/list?molochRegressionUser=historytest2&startTime=0&stopTime=2147483647");
    is ($json->{recordsFiltered}, 0, "Test2: recordsFiltered");

# Make sure can't request someone elses
    $json = get("/history/list?molochRegressionUser=historytest2&userId=historytest1");

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
    $json = viewerGet("/history/list?userId=historytest1&api=sessions");
    is ($json->{recordsFiltered}, 1, "Test4: recordsFiltered");
    my $item = $json->{data}->[0];
    is ($item->{api}, "/sessions.json", "Test4: api");
    $json = viewerGet("/history/list?userId=historytest1&api=somethingsilly");
    is ($json->{recordsFiltered}, 0, "Test4: recordsFiltered");
    $json = viewerGet("/history/list?userId=somethingsilly&api=sessions");
    is ($json->{recordsFiltered}, 0, "Test4: recordsFiltered");

# Should be able to filter by time range
    my $current = time;
    my $pastHour = $current - 3600;
    $json = viewerGet("/api/histories?userId=historytest1&startTime=$pastHour&stopTime=$current");
    is ($json->{recordsFiltered}, 1, "Test5: recordsFiltered");
    $json = viewerGet("/api/histories?userId=historytest1&startTime=18000&stopTime=$pastHour");
    is ($json->{recordsFiltered}, 0, "Test5: recordsFiltered");

# Can't delete items when not admin
    $json = viewerDeleteToken("/history/list/$item->{id}?molochRegressionUser=historytest1", $otherToken);
    eq_or_diff($json, from_json('{"success": false, "text": "You do not have permission to access this resource"}', {relaxed => 1}), "Test Delete Not Admin", { context => 3 });

# Delete item no index
    $json = viewerDeleteToken("/api/history/$item->{id}", $token);
    eq_or_diff($json, from_json('{"success": false, "text": "Missing history index"}', {relaxed => 1}), "Test Delete No Index", { context => 3 });

# Delete item
    $json = viewerDeleteToken("/api/history/$item->{id}?index=$item->{index}", $token);
    eq_or_diff($json, from_json('{"success": true, "text": "Deleted history item successfully"}', {relaxed => 1}), "Test Delete", { context => 3 });
    esGet("/_refresh");

# Make sure gone
    $json = get("/api/histories?molochRegressionUser=historytest1");
    is ($json->{recordsFiltered}, 0, "Should be no items");

# An admin user should see forced expressions for users
    # create a user with a forced expression
    $json = viewerPostToken("/user/create", '{"userId": "historytest2", "userName": "UserName", "enabled":true, "password":"password","expression":"protocols == udp"}', $token);
    sleep(1);
    esGet("/_refresh");
    esGet("/_flush");

    # issue a request as the user with the forced expression
    countTest(1, "molochRegressionUser=historytest2&date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=domainwise"));
    sleep(1);
    esGet("/_refresh");
    esGet("/_flush");

    # find and delete the user/create history item
    $json = viewerGet("/history/list?molochRegressionUser=anonymous");
    $item = $json->{data}->[0];
    $json = viewerDeleteToken("/history/list/$item->{id}?index=$item->{index}", $token);

    # confirm that the forced expression is visible to admin
    $json = viewerGet("/history/list");
    my $found2 = 0;
    foreach my $item3 (@{$json->{data}}) {
        if ($item3->{forcedExpression} eq "protocols == udp") {
            $found2 = 1;
            last;
        }
    }
    is ($found2, 1, "Admin should see forcedExpression in history");

# A nonadmin user should not see forcedExpression
  $json = viewerGet("/history/list?molochRegressionUser=historytest2");

  my $found3 = 0;
  foreach my $item4 (@{$json->{data}}) {
      if ($item4->{forcedExpression} eq "protocols == udp") {
          $found3 = 1;
          last;
      }
  }
  is ($found3, 0, "Non admin should not see forcedExpression in history");

  # delete the history item with the forced expression
  $item = $json->{data}->[0];
  $json = viewerDeleteToken("/history/list/$item->{id}?index=$item->{index}", $token);

# Delete Users
    $json = viewerPostToken("/user/delete", "userId=historytest1&password=a&newPassword=b&currentPassword=c&test=1", $token);
    sleep(1);
    esGet("/_refresh");
    esGet("/_flush");

    $json = viewerPostToken("/user/delete", "userId=historytest2&password=a&newPassword=b&currentPassword=c&test=1", $token);
    sleep(1);
    esGet("/_refresh");
    esGet("/_flush");

# Check history for delete
    $json = viewerGet("/api/histories?molochRegressionUser=anonymous&sortField=timestamp&desc=true");
    is ($json->{recordsFiltered}, 2, "Delete: recordsFiltered");
    $item = $json->{data}->[0];
    is ($item->{api}, "/user/delete", "Delete: api");
    is ($item->{userId}, "anonymous", "Delete: userId");
    ok (!exists $item->{body}->{password}, "Delete: should have no password item");
    ok (!exists $item->{body}->{newPassword}, "Delete: should have no newPassword item");
    ok (!exists $item->{body}->{currentPassword}, "Delete: should have no currentPassword item");
    is ($item->{body}->{userId}, "historytest2", "Delete: correct userId item");
    is ($item->{body}->{test}, "1", "Delete: correct test item");
