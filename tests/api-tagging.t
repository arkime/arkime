use Test::More tests => 42;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Data::Dumper;
use strict;

my $pwd = getcwd() . "/pcap";

my $json;

# Make sure 3 items with no tags
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap"));
    countTest(0, "date=-1&expression=" . uri_escape("tags==TAGTEST1"));
    countTest(0, "date=-1&expression=" . uri_escape("tags==TAGTEST2"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap && tags==domainwise"));

# adding/removing tags test expression
    viewerPost("/addTags?date=-1&expression=file=$pwd/socks-http-example.pcap", "tags=TAGTEST1");
    esGet("/_refresh");
    $json = countTest(3, "date=-1&fields=ta,tags-term,tacnt&expression=" . uri_escape("tags==TAGTEST1"));
    foreach my $item (@{$json->{data}}) {
        is (scalar @{$item->{ta}}, scalar @{$item->{"tags-term"}}, "add: ta and tags-term match");
        is ($item->{tacnt}, scalar @{$item->{"tags-term"}}, "add: tacnt and array size match");
    }

    viewerPost("/removeTags?date=-1&expression=file=$pwd/socks-http-example.pcap", "tags=TAGTEST1");
    esGet("/_refresh");
    countTest(0, "date=-1&expression=" . uri_escape("tags==TAGTEST1"));
    $json = countTest(3, "date=-1&fields=ta,tags-term,tacnt&expression=" . uri_escape("file=$pwd/socks-http-example.pcap && tags==domainwise"));
    foreach my $item (@{$json->{data}}) {
        is (scalar @{$item->{ta}}, scalar @{$item->{"tags-term"}}, "remove: ta and tags-term match");
        is ($item->{tacnt}, scalar @{$item->{"tags-term"}}, "remove: tacnt and array size match");
    }

# adding/removing tags test ids
    my $idQuery = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap"));
    viewerPost("/addTags?date=-1", "tags=TAGTEST2&ids=" . $idQuery->{data}->[0]->{id});
    esGet("/_refresh");
    countTest(1, "date=-1&expression=" . uri_escape("tags==TAGTEST2"));
    viewerPost("/removeTags?date=-1", "tags=TAGTEST2&ids=" . $idQuery->{data}->[0]->{id});
    esGet("/_refresh");
    countTest(0, "date=-1&expression=" . uri_escape("tags==TAGTEST2"));
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap && tags==domainwise"));

# adding tag to no tag item
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/irc.pcap && tags!=EXISTS!"));
    viewerPost("/addTags?date=-1&expression=file=$pwd/irc.pcap", "tags=TAGTEST3");
    esGet("/_refresh");
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/irc.pcap && tags==TAGTEST3"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/irc.pcap && tags!=EXISTS!"));
    viewerPost("/removeTags?date=-1&expression=file=$pwd/irc.pcap", "tags=TAGTEST3");
    esGet("/_refresh");
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/irc.pcap && tags==TAGTEST3"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/irc.pcap && tags!=EXISTS!"));
