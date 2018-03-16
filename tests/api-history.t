use Test::More tests => 37;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $pwd = "*/pcap";

    my $token = getTokenCookie();

# Clear all history
    esDelete("/tests*_history_v1*");
    sleep(1);
    esGet("/_refresh");
    esGet("/_flush");

# Make a request
    countTest(4, "molochRegressionUser=historytest1&date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=domainwise"));
    sleep(1);
    esGet("/_refresh");
    esGet("/_flush");

# See if recorded, should be the only item that is ours
    my $json = viewerGet("/history/list?molochRegressionUser=historytest1");
    is ($json->{recordsFiltered}, 1, "Test1: recordsFiltered");
    my $item = $json->{data}->[0];
    is ($item->{expression}, "(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=domainwise", "Test1: expression");
    is ($item->{uiPage}, "sessions", "Test1: uiPage");
    is ($item->{api}, "/sessions.json", "Test1: api");
    is ($item->{query}, "molochRegressionUser=historytest1&date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=domainwise"), "Test1: query");
    is ($item->{userId}, "historytest1", "Test1: userId");
    is ($item->{recordsReturned}, 4, "Test1: recordsReturned");
    is ($item->{recordsFiltered}, 4, "Test1: recordsFiltered");
    cmp_ok ($item->{recordsTotal}, '>=', 80, "recordsTotal");

# Check Multi
    my $mjson = multiGet("/history/list?molochRegressionUser=historytest1");
    my $index = $json->{data}->[0]->{index};
    delete $json->{data}->[0]->{index};
    delete $mjson->{data}->[0]->{index};
    eq_or_diff($mjson, $json, "multi Test1", { context => 3 });
    $json->{data}->[0]->{index} = $index;

# Make sure another user doesn't see our history
    $json = viewerGet("/history/list?molochRegressionUser=historytest2");
    is ($json->{recordsFiltered}, 0, "Test2: recordsFiltered");

# Check Multi
    $mjson = multiGet("/history/list?molochRegressionUser=historytest2");
    eq_or_diff($mjson, $json, "multi Test2", { context => 3 });

# Make sure can't request someone elses
    $json = viewerGet("/history/list?molochRegressionUser=historytest2&userId=historytest1");
    eq_or_diff($json, from_json('{"success": false, "text": "Need admin privileges"}', {relaxed => 1}), "historytest2 requesting historytest1", { context => 3 });

# Check Multi
    $mjson = viewerGet("/history/list?molochRegressionUser=historytest2&userId=historytest1");
    eq_or_diff($mjson, $json, "multi historytest2 requesting historytest1", { context => 3 });

# An admin user should see everything, find it
    $json = viewerGet("/history/list");
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
    $json = viewerGet("/history/list?userId=historytest1&startTime=$pastHour&stopTime=$current");
    is ($json->{recordsFiltered}, 1, "Test5: recordsFiltered");
    $json = viewerGet("/history/list?userId=historytest1&startTime=18000&stopTime=$pastHour");
    is ($json->{recordsFiltered}, 0, "Test5: recordsFiltered");

# Can't delete items when not admin
    $json = viewerDelete("/history/list/$item->{id}?molochRegressionUser=historytest1");
    eq_or_diff($json, from_json('{"success": false, "text": "Need admin privileges"}', {relaxed => 1}), "Test Delete Not Admin", { context => 3 });

# Delete item no index
    $json = viewerDelete("/history/list/$item->{id}");
    eq_or_diff($json, from_json('{"success": false, "text": "Error deleting history item"}', {relaxed => 1}), "Test Delete No Index", { context => 3 });

# Delete item
    $json = viewerDelete("/history/list/$item->{id}?index=$item->{index}");
    eq_or_diff($json, from_json('{"success": true, "text": "Deleted history item successfully"}', {relaxed => 1}), "Test Delete", { context => 3 });
    esGet("/_refresh");

# Make sure gone
    $json = viewerGet("/history/list?molochRegressionUser=historytest1");
    is ($json->{recordsFiltered}, 0, "Should be no items");

# Check Multi
    $mjson = viewerGet("/history/list?molochRegressionUser=historytest1");
    is ($mjson->{recordsFiltered}, 0, "multi Should be no items");

# Delete Users
    $json = viewerPostToken("/user/delete", "userId=historytest1&password=a&newPassword=b&currentPassword=c&test=1", $token);

    sleep(1);
    esGet("/_refresh");
    esGet("/_flush");

# Check history for delete
    $json = viewerGet("/history/list?molochRegressionUser=anonymous");
    is ($json->{recordsFiltered}, 1, "Delete: recordsFiltered");
    $item = $json->{data}->[0];
    is ($item->{api}, "/user/delete", "Delete: api");
    is ($item->{userId}, "anonymous", "Delete: userId");
    ok (!exists $item->{body}->{password}, "Delete: should have no password item");
    ok (!exists $item->{body}->{newPassword}, "Delete: should have no newPassword item");
    ok (!exists $item->{body}->{currentPassword}, "Delete: should have no currentPassword item");
    is ($item->{body}->{userId}, "historytest1", "Delete: correct userId item");
    is ($item->{body}->{test}, "1", "Delete: correct test item");
