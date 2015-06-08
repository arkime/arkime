use Test::More tests => 14;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use strict;

my $pwd = getcwd() . "/pcap";

# Make sure 3 items with no tags
    countTest(3, "date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap"));
    countTest(0, "date=-1&expression=" . uri_escape("tags==TAGTEST1"));
    countTest(0, "date=-1&expression=" . uri_escape("tags==TAGTEST2"));

# adding/removing tags test expression
    viewerPost("/addTags?date=-1&expression=file=$pwd/socks-http-example.pcap", "tags=TAGTEST1");
    esGet("/_refresh");
    countTest(3, "date=-1&expression=" . uri_escape("tags==TAGTEST1"));
    viewerPost("/removeTags?date=-1&expression=file=$pwd/socks-http-example.pcap", "tags=TAGTEST1");
    esGet("/_refresh");
    countTest(0, "date=-1&expression=" . uri_escape("tags==TAGTEST1"));

# adding/removing tags test ids
    my $idQuery = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap"));
    viewerPost("/addTags?date=-1", "tags=TAGTEST2&ids=" . $idQuery->{data}->[0]->{id});
    esGet("/_refresh");
    countTest(1, "date=-1&expression=" . uri_escape("tags==TAGTEST2"));
    viewerPost("/removeTags?date=-1", "tags=TAGTEST2&ids=" . $idQuery->{data}->[0]->{id});
    esGet("/_refresh");
    countTest(0, "date=-1&expression=" . uri_escape("tags==TAGTEST2"));
