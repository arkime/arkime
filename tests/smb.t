use Test::More tests => 38;
use Cwd;
use URI::Escape;
use MolochTest;
use strict;

my $pwd = "*/pcap";
my $files = "(file=$pwd/smb-port80.pcap||file=$pwd/smb-smbclient.pcap)";

countTest(2, "date=-1&expression=" . uri_escape("$files&&protocols==smb"));

# smb.share
    countTest(1, "date=-1&expression=" . uri_escape("$files&&smb.share==\"\\\\\\\\LOCALHOST\\\\MP3\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&smb.share==\"\\\\\\\\localhost\\\\MP3\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&smb.share.cnt==1"));

# smb.fn
    countTest(1, "date=-1&expression=" . uri_escape("$files&&smb.fn==\"\\\\tmp\\\\foo\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&smb.fn==\"\\\\TMP\\\\foo\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&smb.fn.cnt==1"));

# smb.user
    countTest(1, "date=-1&expression=" . uri_escape("$files&&smb.user==user"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&smb.user==User"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&smb.user.cnt==1"));

# smb.domain
    countTest(1, "date=-1&expression=" . uri_escape("$files&&smb.domain==WORKGROUP"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&smb.domain==wORKGROUP"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&smb.domain.cnt==1"));

# smb.os
    countTest(1, "date=-1&expression=" . uri_escape("$files&&smb.os==Unix"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&smb.os==unix"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&smb.os.cnt==1"));

# smb.ver
    countTest(1, "date=-1&expression=" . uri_escape("$files&&smb.ver==\"Samba 3.6.3\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&smb.ver==\"samba 3.6.3\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&smb.ver.cnt==1"));
