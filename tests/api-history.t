use Test::More tests => 15;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $pwd = getcwd() . "/pcap";

    my $token = getTokenCookie();

# Make a request
    countTest(4, "molochRegressionUser=historytest1&date=-1&expression=" . uri_escape("(file=$pwd/socks-https-example.pcap||file=$pwd/dns-mx.pcap)&&tags=domainwise"));
    esGet("/_refresh");

# See if recorded, should be the only item that is ours
    my $json = viewerGet("/history/list?molochRegressionUser=historytest1");
    is ($json->{recordsFiltered}, 1, "Test1: recordsFiltered");
    my $item = $json->{data}->[0];
    is ($item->{expression}, "(file=/Users/awick/moloch.newui/tests/pcap/socks-https-example.pcap||file=/Users/awick/moloch.newui/tests/pcap/dns-mx.pcap)&&tags=domainwise", "Test1: expression");
    is ($item->{uiPage}, "sessions", "Test1: uiPage");
    is ($item->{pathname}, "/sessions.json", "Test1: pathname");
    is ($item->{query}, "molochRegressionUser=historytest1&date=-1&expression=%28file%3D%2FUsers%2Fawick%2Fmoloch.newui%2Ftests%2Fpcap%2Fsocks-https-example.pcap%7C%7Cfile%3D%2FUsers%2Fawick%2Fmoloch.newui%2Ftests%2Fpcap%2Fdns-mx.pcap%29%26%26tags%3Ddomainwise", "Test1: query");
    is ($item->{userId}, "historytest1", "Test1: userId");

# Make sure another user doesn't see our history
    $json = viewerGet("/history/list?molochRegressionUser=historytest2");
    is ($json->{recordsFiltered}, 0, "Test2: recordsFiltered");

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
    is ($found, 1, "Test4: Found id in all");

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
    my $json = viewerGet("/history/list?molochRegressionUser=historytest1");
    is ($json->{recordsFiltered}, 0, "Should be no items");


# Delete Users
    $json = viewerPostToken("/user/delete", "userId=historytest1", $token);
