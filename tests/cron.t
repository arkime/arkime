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
my $token = getTokenCookie();

$json = viewerGetToken("/user/cron", $token);
is (scalar keys %{$json}, 0, "Queries empty");

$json = viewerPostToken("/user/create", '{"userId": "anonymous", "userName": "anonymous", "enabled":true, "password":"password"}', $token);

my $json = viewerPostToken("/user/cron/create", "since=-1&name=test1&tags=cron&action=forward:test2&query=$files", $token);
my $key = $json->{key};
sleep 5;
esGet("/_refresh");
esGet("/_flush");

viewerGet("/processCronQueries");

sleep 5;

esGet("/_refresh");
esGet("/_flush");

$json = viewerGetToken("/user/cron", $token);

is ($json->{$key}->{count}, 13, "Query count correct");
is ($json->{$key}->{creator}, "anonymous", "Query creator correct");
is ($json->{$key}->{name}, "test1", "Query name correct");
is ($json->{$key}->{tags}, "cron", "Query tags correct");
is ($json->{$key}->{action}, "forward:test2", "Query action correct");
is ($json->{$key}->{query}, $query, "Query query correct");

is (scalar keys %{$json}, 1, "Queries after add");

# See if it worked
countTest2(13, "date=-1&expression=" . uri_escape("protocols==tcp"));
countTest2(13, "date=-1&expression=" . uri_escape("tags==cron"));

# Delete Cron
$json = viewerPostToken("/user/cron/delete", "key=$key", $token);
$json = viewerGetToken("/user/cron", $token);

is (scalar keys %{$json}, 0, "Queries empty after delete");

# Delete copied items
$json = viewerPost2("/delete?date=-1&expression=" . uri_escape("protocols=tcp"));
eq_or_diff($json, from_json('{"success": true, "text": "Deleting of 13 sessions complete"}', {relaxed => 1}), "Delete All", { context => 3 });

esGet("/_refresh");
esGet("/_flush");
countTest2(0, "date=-1&expression=" . uri_escape("protocols==tcp"));

esDelete("/tests_users/user/anonymous");
viewerPost("/flushCache");
