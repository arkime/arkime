use Test::More tests => 16;
use JSON;
use Cwd;
use URI::Escape;
use MolochTest;
use Data::Dumper;
use Test::Differences;
use strict;

my $pwd = getcwd() . "/pcap";
my $query = "(file=$pwd/socks-http-example.pcap||file=$pwd/socks-http-pass.pcap||file=$pwd/socks-https-example.pcap||file=$pwd/socks5-http-302.pcap||file=$pwd/socks5-rdp.pcap||file=$pwd/socks5-reverse.pcap||file=$pwd/socks5-smtp-503.pcap)";
my $files = uri_escape($query);


my $json;
my $token = getToken();
is (scalar @{viewerPost("/cronQueries.json", "token=$token")}, 0, "Queries empty");

$json = viewerPost("/addUser", "token=$token&userId=anonymous&userName=anonymous&password=anonymous&enabled=on");

my $json = viewerPost("/updateCronQuery", "key=_create_&since=-1&enabled=true&name=test1&tags=cron&action=forward:test2&query=$files&token=$token");
my $key = $json->{key};
esGet("/_refresh");

sleep 3;
esGet("/_refresh");
esGet("/_flush");

$json = viewerPost("/cronQueries.json", "token=$token");

is ($json->[0]->{count}, 13, "Query count correct");
is ($json->[0]->{creator}, "anonymous", "Query creator correct");
is ($json->[0]->{name}, "test1", "Query name correct");
is ($json->[0]->{tags}, "cron", "Query tags correct");
is ($json->[0]->{action}, "forward:test2", "Query action correct");
is ($json->[0]->{query}, $query, "Query query correct");

is (scalar @{$json}, 1, "Queries after add");

# See if it worked
countTest2(13, "date=-1&expression=" . uri_escape("protocols==tcp"));
countTest2(13, "date=-1&expression=" . uri_escape("tags==cron"));

# Delete Cron
$json = viewerPost("/deleteCronQuery", "key=$key&token=$token");
is (scalar @{viewerPost("/cronQueries.json", "token=$token")}, 0, "Queries empty");

# Delete copied items
$json = viewerPost2("/delete?date=-1&expression=" . uri_escape("protocols=tcp"));
eq_or_diff($json, from_json('{"success": true, "text": "Deleting of 13 sessions complete"}', {relaxed => 1}), "Delete All", { context => 3 });

esGet("/_refresh");
esGet("/_flush");
countTest2(0, "date=-1&expression=" . uri_escape("protocols==tcp"));

esDelete("/tests_users/user/anonymous");
viewerPost("/flushCache");
