use Test::More tests => 4;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $pwd = getcwd() . "/pcap";

# bigendian pcap file tests
    my $json = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/bigendian.pcap"));
    is ($json->{iTotalDisplayRecords}, 1, "bigendian iTotalDisplayRecords");
    my $response = $MolochTest::userAgent->get("http://$MolochTest::host:8123/test/raw/" . $json->{aaData}->[0]->{id} . "?type=src");
    is (unpack("H*", $response->content), "4fa11b290002538d08090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f30313233343536374fa11b2d0008129108090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f3031323334353637", "Correct bigendian tcpdump data");

# csv
    my $csv = $MolochTest::userAgent->get("http://$MolochTest::host:8123/sessions.csv?date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap"))->content;
    $csv =~ s/\r//g;
    eq_or_diff ($csv, 'Protocol, First Packet, Last Packet, Source IP, Source Port, Source Geo, Destination IP, Destination Port, Destination Geo, Packets, Bytes, Data Bytes, Node
tcp, 1386004309, 1386004309, 10.180.156.185, 53533, USA, 10.180.156.249, 1080, USA, 14, 2698, 1754, test
tcp, 1386004312, 1386004312, 10.180.156.185, 53534, USA, 10.180.156.249, 1080, USA, 15, 2780, 1770, test
tcp, 1386004317, 1386004317, 10.180.156.185, 53535, USA, 10.180.156.249, 1080, USA, 17, 2905, 1763, test
', "CSV Expression");
   
    my $idQuery = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("file=$pwd/socks-http-example.pcap"));
    $csv = $MolochTest::userAgent->get("http://$MolochTest::host:8123/sessions.csv?date=-1&ids=" . $idQuery->{aaData}->[0]->{id})->content;
    $csv =~ s/\r//g;
    eq_or_diff ($csv, 
'Protocol, First Packet, Last Packet, Source IP, Source Port, Source Geo, Destination IP, Destination Port, Destination Geo, Packets, Bytes, Data Bytes, Node
tcp, 1386004309, 1386004309, 10.180.156.185, 53533, USA, 10.180.156.249, 1080, USA, 14, 2698, 1754, test
', "CSV Ids");
