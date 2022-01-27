use Test::More tests => 38;
use Cwd;
use URI::Escape;
use MolochTest;
use strict;

my $files = "file=*/pcap/modbus.pcap";

countTest(6, "date=-1&expression=" . uri_escape("$files&&protocols==modbus"));

# modbus.unitid
    countTest(1, "date=-1&expression=" . uri_escape("$files&&modbus.unitid==255"));
    countTest(4, "date=-1&expression=" . uri_escape("$files&&modbus.unitid==10"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&modbus.unitid==1"));

# modbus.protocolid
    countTest(6, "date=-1&expression=" . uri_escape("$files&&modbus.protocolid==0"));

# modbus.transactionid
    countTest(2, "date=-1&expression=" . uri_escape("$files&&modbus.transactionid==0"));
    countTest(4, "date=-1&expression=" . uri_escape("$files&&modbus.transactionid==1"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&modbus.transactionid==256"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&modbus.transactionid>1"));

# modbus.transactionid.cnt
    countTest(1, "date=-1&expression=" . uri_escape("$files&&modbus.transactionid.cnt>1"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&modbus.transactionid.cnt==256"));

# modbus.funccode
    countTest(1, "date=-1&expression=" . uri_escape("$files&&modbus.funccode==2"));
    countTest(2, "date=-1&expression=" . uri_escape("$files&&modbus.funccode==1"));
    countTest(3, "date=-1&expression=" . uri_escape("$files&&modbus.funccode==6"));
    countTest(2, "date=-1&expression=" . uri_escape("$files&&modbus.funccode==4"));
    countTest(2, "date=-1&expression=" . uri_escape("$files&&modbus.funccode==8"));

# modbus.exccode
    countTest(2, "date=-1&expression=" . uri_escape("$files&&modbus.exccode>0"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&modbus.exccode==11"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&modbus.exccode==3"));
