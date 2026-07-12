# Tests on a fresh install
use Test::More tests => 46;
use Cwd;
use URI::Escape;
use ArkimeTest;
use Test::Differences;
use JSON;
use Data::Dumper;
use strict;

    esCopy("tests_fields", "tests2_fields");
    esGet("/_flush");

# Make sure no items
    viewerPost2("/regressionTests/flushCache");
    countTest2(0, "date=-1");

# Now test the APIs, basically making sure viewer doesn't crash
my $json;

    $json = viewerGet2("/eshealth.json");
    is ($json->{number_of_data_nodes}, 1, "Correct number_of_data_nodes");

    $json = viewerGet2("/esstats.json");
    is ($json->{recordsTotal}, 1, "Correct number of stats");

    $json = viewerGet2("/stats.json");
    is ($json->{recordsTotal}, 0, "Correct files recordsTotal");

    $json = viewerGet2("/dstats.json");
    is (scalar %{$json}, 0, "Empty dstats");

    $json = viewerGet2("/api/files");
    is ($json->{recordsTotal}, 0, "Correct files recordsTotal");

    $json = viewerGet2("/sessions.json?date=-1");
    is ($json->{recordsTotal}, 0, "Correct sessions.json recordsTotal");
    is ($json->{graph}->{interval}, 60, "Correct sessions.json graph interval");
    is (scalar @{$json->{graph}->{sessionsHisto}}, 0, "Correct sessions.json graph sessionsHisto");
    is (scalar @{$json->{graph}->{"source.packetsHisto"}}, 0, "Correct sessions.json graph srcPacketsHisto");
    is (scalar @{$json->{graph}->{"destination.packetsHisto"}}, 0, "Correct sessions.json graph dstPacketsHisto");
    is (scalar @{$json->{graph}->{"client.bytesHisto"}}, 0, "Correct sessions.json graph client.bytesHisto");
    is (scalar @{$json->{graph}->{"server.bytesHisto"}}, 0, "Correct sessions.json graph dstDataBytesHisto");
    is (scalar keys %{$json->{map}}, 0, "Correct sessions.json map");

    $json = viewerGet2("/spigraph.json?map=true&date=-1");
    is ($json->{recordsTotal}, 0, "Correct spigraph.json recordsTotal");
    is ($json->{graph}->{interval}, 3600, "Correct spigraph.json graph interval");
    is (scalar @{$json->{graph}->{sessionsHisto}}, 0, "Correct spigraph.json graph sessionsHisto");
    is (scalar @{$json->{graph}->{"source.packetsHisto"}}, 0, "Correct spigraph.json graph srcPacketsHisto");
    is (scalar @{$json->{graph}->{"destination.packetsHisto"}}, 0, "Correct spigraph.json graph dstPacketsHisto");
    is (scalar @{$json->{graph}->{"client.bytesHisto"}}, 0, "Correct spigraph.json graph client.bytesHisto");
    is (scalar @{$json->{graph}->{"server.bytesHisto"}}, 0, "Correct spigraph.json graph dstDataBytesHisto");
    is (scalar keys %{$json->{map}}, 0, "Correct spigraph.json map");

    $json = viewerGet2("/spiview.json?map=true&date=-1");
    is (scalar keys %{$json->{spi}}, 0, "Empty spiview.json spi");
    is ($json->{recordsTotal}, 0, "Correct spiview.json recordsTotal");
    is (!exists $json->{graph}, 1, "Shouldn't have spiview.json graph");
    is (!exists $json->{map}, 1, "Shouldn't have spiview.json map");

    $json = viewerGet2("/spiview.json?spi=ta&facets=1&map=true&date=-1");
    is (scalar keys %{$json->{spi}}, 1, "one spiview.json spi");
    is (scalar keys %{$json->{spi}->{ta}}, 2, "Two spiview.json ta elements");
    is ($json->{recordsTotal}, 0, "Correct spiview.json recordsTotal");
    is (scalar @{$json->{graph}->{sessionsHisto}}, 0, "Correct spiview.json graph sessionsHisto");
    is (scalar @{$json->{graph}->{"source.packetsHisto"}}, 0, "Correct spiview.json graph srcPacketsHisto");
    is (scalar @{$json->{graph}->{"destination.packetsHisto"}}, 0, "Correct spiview.json graph dstPacketsHisto");
    is (scalar @{$json->{graph}->{"client.bytesHisto"}}, 0, "Correct spiview.json graph client.bytesHisto");
    is (scalar @{$json->{graph}->{"server.bytesHisto"}}, 0, "Correct spiview.json graph dstDataBytesHisto");
    is (scalar keys %{$json->{map}}, 3, "Correct spiview.json map");

    $json = viewerGet2("/connections.json?date=-1");
    is ($json->{recordsFiltered}, 0, "Correct connections.json recordsFiltered");
    is (!exists $json->{graph}, 1, "Shouldn't have connections.json graph");
    is (!exists $json->{map}, 1, "Shouldn't have connections.json map");

    my $txt = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8124/unique.txt?field=tags")->content;
    is ($txt, "", "Empty unique.txt");

    $json = viewerGet2("/api/remoteclusters");
    eq_or_diff($json, from_json('{"test2": {"name": "Test2", "url": "http://localhost:8124" }}'));

    $json = multiGet("/api/clusters");
    eq_or_diff($json, from_json('{"inactive": [], "active": ["test", "test2"]}'));

# payload8 hex and utf8 field definitions are all served to the UI (issue #3201)
    $json = viewerGet2("/api/fields");
    eq_or_diff($json->{"payload8.src.hex"}, from_json('{"friendlyName":"Payload Src Hex","type":"lotermfield","exp":"payload8.src.hex","help":"First 8 bytes of source payload in hex","dbField":"srcPayload8","group":"general","dbField2":"srcPayload8","aliases":["payload.src"]}'), "payload8.src.hex field");
    eq_or_diff($json->{"payload8.src.utf8"}, from_json('{"friendlyName":"Payload Src UTF8","type":"termfield","exp":"payload8.src.utf8","help":"First 8 bytes of source payload in utf8","dbField":"srcPayload8","group":"general","dbField2":"srcPayload8","transform":"utf8ToHex","noFacet":"true"}'), "payload8.src.utf8 field");
    eq_or_diff($json->{"payload8.dst.hex"}, from_json('{"friendlyName":"Payload Dst Hex","type":"lotermfield","exp":"payload8.dst.hex","help":"First 8 bytes of destination payload in hex","dbField":"dstPayload8","group":"general","dbField2":"dstPayload8","aliases":["payload.dst"]}'), "payload8.dst.hex field");
    eq_or_diff($json->{"payload8.dst.utf8"}, from_json('{"friendlyName":"Payload Dst UTF8","type":"termfield","exp":"payload8.dst.utf8","help":"First 8 bytes of destination payload in utf8","dbField":"dstPayload8","group":"general","dbField2":"dstPayload8","transform":"utf8ToHex","noFacet":"true"}'), "payload8.dst.utf8 field");
