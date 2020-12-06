use Test::More tests => 72;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Data::Dumper;
use strict;

my $pwd = "*/pcap";

my $json;

# Make sure 3 items with no tags
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap"));
    countTest(0, "date=-1&expression=" . uri_escape("tags==TAGTEST1"));
    countTest(0, "date=-1&expression=" . uri_escape("tags==TAGTEST2"));
    countTest(0, "date=-1&expression=" . uri_escape("tags==TAGTEST3"));

    countTest(0, "date=-1&expression=" . uri_escape("tags==MTAGTEST1"));
    countTest(0, "date=-1&expression=" . uri_escape("tags==MTAGTEST2"));
    countTest(0, "date=-1&expression=" . uri_escape("tags==MTAGTEST3"));

    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap && tags==domainwise"));

# adding/removing tags test expression
    viewerPost("/addTags?date=-1&expression=file=$pwd/socks-http-example.pcap", "tags=TAGTEST1");
    esGet("/_refresh");
    $json = countTest(3, "date=-1&fields=tags,tagsCnt&expression=" . uri_escape("tags==TAGTEST1"));
    foreach my $item (@{$json->{data}}) {
        is ($item->{tagsCnt}, scalar @{$item->{"tags"}}, "add: tagsCnt and array size match");
    }

    viewerPost("/removeTags?date=-1&expression=file=$pwd/socks-http-example.pcap", "tags=TAGTEST1");
    esGet("/_refresh");
    countTest(0, "date=-1&expression=" . uri_escape("tags==TAGTEST1"));
    $json = countTest(3, "date=-1&fields=tags,tagsCnt&expression=" . uri_escape("file=$pwd/socks-http-example.pcap && tags==domainwise"));
    foreach my $item (@{$json->{data}}) {
        is ($item->{tagsCnt}, scalar @{$item->{"tags"}}, "remove: tagsCnt and array size match");
    }

# adding/removing tags test expression for MultiViewer
    multiPost("/addTags?date=-1&expression=file=$pwd/socks-http-example.pcap", "tags=MTAGTEST1");
    esGet("/_refresh");
    $json = countTest(3, "date=-1&fields=tags,tagsCnt&expression=" . uri_escape("tags==MTAGTEST1"));
    foreach my $item (@{$json->{data}}) {
        is ($item->{tagsCnt}, scalar @{$item->{"tags"}}, "add: tagsCnt and array size match");
    }

    multiPost("/removeTags?date=-1&expression=file=$pwd/socks-http-example.pcap", "tags=MTAGTEST1");
    esGet("/_refresh");
    countTest(0, "date=-1&expression=" . uri_escape("tags==MTAGTEST1"));
    $json = countTest(3, "date=-1&fields=tags,tagsCnt&expression=" . uri_escape("file=$pwd/socks-http-example.pcap && tags==domainwise"));
    foreach my $item (@{$json->{data}}) {
        is ($item->{tagsCnt}, scalar @{$item->{"tags"}}, "remove: tagsCnt and array size match");
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

# adding/removing tags test ids for MultiViewer
    my $idQuery = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap"));
    multiPost("/addTags?date=-1", "tags=MTAGTEST2&ids=" . $idQuery->{data}->[0]->{id});
    esGet("/_refresh");
    countTest(1, "date=-1&expression=" . uri_escape("tags==MTAGTEST2"));
    multiPost("/removeTags?date=-1", "tags=MTAGTEST2&ids=" . $idQuery->{data}->[0]->{id});
    esGet("/_refresh");
    countTest(0, "date=-1&expression=" . uri_escape("tags==MTAGTEST2"));
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

# adding tag to no tag item for MultiViewer
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/irc.pcap && tags!=EXISTS!"));
    multiPost("/addTags?date=-1&expression=file=$pwd/irc.pcap", "tags=MTAGTEST3");
    esGet("/_refresh");
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/irc.pcap && tags==MTAGTEST3"));
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/irc.pcap && tags!=EXISTS!"));
    multiPost("/removeTags?date=-1&expression=file=$pwd/irc.pcap", "tags=MTAGTEST3");
    esGet("/_refresh");
    countTest(0, "date=-1&expression=" . uri_escape("file=$pwd/irc.pcap && tags==MTAGTEST3"));
    countTest(1, "date=-1&expression=" . uri_escape("file=$pwd/irc.pcap && tags!=EXISTS!"));
