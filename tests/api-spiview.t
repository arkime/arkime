use Test::More tests => 3;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $pwd = getcwd() . "/pcap";
my $files = "(file=$pwd/socks-http-example.pcap||file=$pwd/socks-http-pass.pcap||file=$pwd/socks-https-example.pcap||file=$pwd/socks5-http-302.pcap||file=$pwd/socks5-rdp.pcap||file=$pwd/socks5-reverse.pcap||file=$pwd/socks5-smtp-503.pcap)";


    my $json = viewerGet("/spiview.json?date=-1&spi=a1&expression=" . uri_escape("$files&&ip.protocol==tcp"));
    delete $json->{health};
    eq_or_diff($json, from_json('{ "iTotalRecords": 141, "spi": { "a1": { "_type": "terms", "missing": 0, "total": 13, "other": 0, "terms": [ { "term": 179608761, "count": 9 }, { "term": 167772161, "count": 2 }, { "term": 167772163, "count": 1 }, { "term": 167772162, "count": 1 } ] } }, "iTotalDisplayRecords": 13, "bsqErr": null }', {relaxed => 1}), "a1 ip.protocol==tcp", { context => 3 });

    my $json = viewerGet("/spiview.json?date=-1&&spi=a1&expression=" . uri_escape("$files&&ip.protocol==blah"));
    eq_or_diff($json, from_json('{ "spi": {}, "bsqErr": "Unknown protocol string blah" }', {relaxed => 1}), "a1 ip.protocol==blah", { context => 3 });

    my $json = viewerGet("/spiview.json?date=-1&&spi=a1&expression=" . uri_escape("$files&&ip.protocol==[tcp,blah2]"));
    eq_or_diff($json, from_json('{ "spi": {}, "bsqErr": "Unknown protocol string blah2" }', {relaxed => 1}), "a1 ip.protocol==[tcp,blah2]", { context => 3 });
