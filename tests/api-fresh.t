# Tests on a fresh install
use Test::More tests => 41;
use Cwd;
use URI::Escape;
use MolochTest;
use Test::Differences;
use JSON;
use Data::Dumper;
use strict;

# Clear out node2
    system("../db/db.pl --prefix tests2 $MolochTest::elasticsearch initnoprompt 2>&1 1>/dev/null");
    sleep(1);
    esCopy("tests_fields", "tests2_fields");
    esGet("/_flush");

# Make sure no items
    viewerPost2("/regressionTests/flushCache");
    countTest2(0, "date=-1");

# Now test the APIs, basically making sure viewer dosn't crash
my $json;

    $json = viewerGet2("/eshealth.json");
    is ($json->{number_of_data_nodes}, 1, "Correct number_of_data_nodes");

    $json = viewerGet2("/esstats.json");
    is ($json->{recordsTotal}, 1, "Correct number of stats");

    $json = viewerGet2("/stats.json");
    is ($json->{recordsTotal}, 0, "Correct stats.json recordsTotal");

    $json = viewerGet2("/dstats.json");
    is (scalar %{$json}, 0, "Empty dstats");

    $json = viewerGet2("/file/list");
    is ($json->{recordsTotal}, 0, "Correct stats.json recordsTotal");

    $json = viewerGet2("/sessions.json?map=true");
    is ($json->{recordsTotal}, 0, "Correct sessions.json recordsTotal");
    is ($json->{graph}->{interval}, 60, "Correct sessions.json graph interval");
    is (scalar @{$json->{graph}->{sessionsHisto}}, 0, "Correct sessions.json graph sessionsHisto");
    is (scalar @{$json->{graph}->{"source.packetsHisto"}}, 0, "Correct sessions.json graph srcPacketsHisto");
    is (scalar @{$json->{graph}->{"destination.packetsHisto"}}, 0, "Correct sessions.json graph dstPacketsHisto");
    is (scalar @{$json->{graph}->{"client.bytesHisto"}}, 0, "Correct sessions.json graph client.bytesHisto");
    is (scalar @{$json->{graph}->{"server.bytesHisto"}}, 0, "Correct sessions.json graph dstDataBytesHisto");
    is (scalar keys %{$json->{map}}, 0, "Correct sessions.json map");

    $json = viewerGet2("/spigraph.json?map=true");
    is ($json->{recordsTotal}, 0, "Correct spigraph.json recordsTotal");
    is ($json->{graph}->{interval}, 60, "Correct spigraph.json graph interval");
    is (scalar @{$json->{graph}->{sessionsHisto}}, 0, "Correct spigraph.json graph sessionsHisto");
    is (scalar @{$json->{graph}->{"source.packetsHisto"}}, 0, "Correct spigraph.json graph srcPacketsHisto");
    is (scalar @{$json->{graph}->{"destination.packetsHisto"}}, 0, "Correct spigraph.json graph dstPacketsHisto");
    is (scalar @{$json->{graph}->{"client.bytesHisto"}}, 0, "Correct spigraph.json graph client.bytesHisto");
    is (scalar @{$json->{graph}->{"server.bytesHisto"}}, 0, "Correct spigraph.json graph dstDataBytesHisto");
    is (scalar keys %{$json->{map}}, 0, "Correct spigraph.json map");

    $json = viewerGet2("/spiview.json?map=true");
    is (scalar keys %{$json->{spi}}, 0, "Empty spiview.json spi");
    is ($json->{recordsTotal}, 0, "Correct spiview.json recordsTotal");
    is (!exists $json->{graph}, 1, "Shouldn't have spiview.json graph");
    is (!exists $json->{map}, 1, "Shouldn't have spiview.json map");

    $json = viewerGet2("/spiview.json?spi=ta&facets=1&map=true");
    is (scalar keys %{$json->{spi}}, 1, "one spiview.json spi");
    is (scalar keys %{$json->{spi}->{ta}}, 2, "Two spiview.json ta elements");
    is ($json->{recordsTotal}, 0, "Correct spiview.json recordsTotal");
    is (scalar @{$json->{graph}->{sessionsHisto}}, 0, "Correct spiview.json graph sessionsHisto");
    is (scalar @{$json->{graph}->{"source.packetsHisto"}}, 0, "Correct spiview.json graph srcPacketsHisto");
    is (scalar @{$json->{graph}->{"destination.packetsHisto"}}, 0, "Correct spiview.json graph dstPacketsHisto");
    is (scalar @{$json->{graph}->{"client.bytesHisto"}}, 0, "Correct spiview.json graph client.bytesHisto");
    is (scalar @{$json->{graph}->{"server.bytesHisto"}}, 0, "Correct spiview.json graph dstDataBytesHisto");
    is (scalar keys %{$json->{map}}, 3, "Correct spiview.json map");

    $json = viewerGet2("/connections.json");
    is ($json->{recordsFiltered}, 0, "Correct connections.json recordsFiltered");
    is (!exists $json->{graph}, 1, "Shouldn't have connections.json graph");
    is (!exists $json->{map}, 1, "Shouldn't have connections.json map");

    my $txt = $MolochTest::userAgent->get("http://$MolochTest::host:8124/unique.txt?field=tags")->content;
    is ($txt, "", "Empty unique.txt");

    $json = viewerGet2("/remoteclusters");
    eq_or_diff($json, from_json('{"test2": {"name": "Test2", "url": "http://localhost:8124" }}'));
