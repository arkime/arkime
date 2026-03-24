use Test::More tests => 36;
use Cwd;
use URI::Escape;
use ArkimeTest;
use strict;

my $files = "file=*/pcap/adb.pcap";

# Test protocol detection
countTest(1, "date=-1&expression=" . uri_escape("$files&&protocols==adb"));

# adb.command
    countTest(1, "date=-1&expression=" . uri_escape("$files&&adb.command==cnxn"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&adb.command==open"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&adb.command==okay"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&adb.command==wrte"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&adb.command==invalid"));

# adb.systemtype
    countTest(1, "date=-1&expression=" . uri_escape("$files&&adb.systemtype==device"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&adb.systemtype==host"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&adb.systemtype==bootloader"));

# adb.service
    countTest(1, "date=-1&expression=" . uri_escape("$files&&adb.service==shell"));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&adb.service==invalid"));

# adb.version
    countTest(1, "date=-1&expression=" . uri_escape("$files&&adb.version==0x*"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&adb.version.cnt>=1"));

# adb.maxpayload
    countTest(1, "date=-1&expression=" . uri_escape("$files&&adb.maxpayload.cnt>=1"));

# adb.streamid
    countTest(1, "date=-1&expression=" . uri_escape("$files&&adb.streamid.cnt>=1"));

# Port based detection
    countTest(1, "date=-1&expression=" . uri_escape("$files&&port.dst==5555"));

# Ensure no false positives on non-ADB traffic
my $otherfiles = "file=*/pcap/ssh2.pcap";
    countTest(0, "date=-1&expression=" . uri_escape("$otherfiles&&protocols==adb"));
    countTest(1, "date=-1&expression=" . uri_escape("$otherfiles&&protocols==ssh"));
