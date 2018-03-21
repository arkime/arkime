use Test::More tests => 24;
use Cwd;
use URI::Escape;
use MolochTest;
use strict;

my $pwd = "*/pcap";
my $files = "file=$pwd/ssh2.pcap";

countTest(1, "date=-1&expression=" . uri_escape("$files&&protocols==ssh"));

# ssh.key
    countTest(1, "date=-1&expression=" . uri_escape("$files&&ssh.key==\"AAAAB3NzaC1yc2EAAAABeHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHg=\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&ssh.key==\"aaaaB3NzaC1yc2EAAAABeHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHg=\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&ssh.key==AAAA*"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&ssh.key==aaaa*"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&ssh.key.cnt==1"));

# ssh.ver
    countTest(1, "date=-1&expression=" . uri_escape("$files&&ssh.ver==ssh-2.0-openssh_5.3"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&ssh.ver==SSH-2.0-openssh_5.3"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&ssh.ver==ssh*"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&ssh.ver==SSH*"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&ssh.ver.cnt==1"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&ssh.ver.cnt==2"));
