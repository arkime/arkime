use Test::More tests => 24;
use Cwd;
use URI::Escape;
use MolochTest;
use strict;

my $files = "file=*/pcap/modbus.pcap";

countTest(7, "date=-1&expression=" . uri_escape("$files&&protocols==modbus"));

# modbus.unitid
    countTest(1, "date=-1&expression=" . uri_escape("$files&&modbus.unitid==255"));
    countTest(4, "date=-1&expression=" . uri_escape("$files&&modbus.unitid==10"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&modbus.unitid==1"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&modbus.unitid==5"));

# modbus.protocolid
    countTest(7, "date=-1&expression=" . uri_escape("$files&&modbus.protocolid==0"));

# modbus.transactionid
    countTest(2, "date=-1&expression=" . uri_escape("$files&&modbus.transactionid==0"));
    countTest(5, "date=-1&expression=" . uri_escape("$files&&modbus.transactionid==1"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&modbus.transactionid==256"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&modbus.transactionid>1"));

# modbus.transactioncnt
    countTest(6, "date=-1&expression=" . uri_escape("$files&&modbus.transactioncnt==1"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&modbus.transactioncnt==256"));
