use Test::More tests => 22;
use Cwd;
use URI::Escape;
use MolochTest;
use strict;

my $pwd = "*/pcap";
my $files = "file=$pwd/irc.pcap";

countTest(1, "date=-1&expression=" . uri_escape("$files&&protocols==irc"));

# irc.nick
    countTest(1, "date=-1&expression=" . uri_escape("$files&&irc.nick==molochtest"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&irc.nick==Molochtest"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&irc.nick==moloch*"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&irc.nick==Moloch*"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&irc.nick.cnt==1"));

# irc.channel
    countTest(1, "date=-1&expression=" . uri_escape("$files&&irc.channel==\"#moloch-fpc\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&irc.channel==\"#Moloch-fpc\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&irc.channel==\"#moloch-*\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&irc.channel==\"#Moloch-*\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&irc.channel.cnt==1"));
