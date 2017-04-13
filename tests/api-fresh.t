# Tests on a fresh install
use Test::More tests => 39;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Data::Dumper;
use strict;

# Clear out node2
    system("../db/db.pl --prefix tests2 $MolochTest::elasticsearch initnoprompt 2>&1 1>/dev/null");
    esCopy("tests_fields", "tests2_fields", "field");


# Make sure no items
    viewerPost2("/flushCache");
    countTest2(0, "date=-1");

# Now test the APIs, basically making sure viewer dosn't crash
my $json;

    $json = viewerGet2("/eshealth.json");
    is ($json->{number_of_data_nodes}, 1, "Correct number_of_data_nodes");

    $json = viewerGet2("/esstats.json");
    is ($json->{health}->{number_of_data_nodes}, 1, "Correct health number_of_data_nodes");

    $json = viewerGet2("/stats.json");
    is ($json->{recordsTotal}, 0, "Correct stats.json recordsTotal");

    $json = viewerGet2("/dstats.json");
    is (scalar %{$json}, 0, "Empty dstats");

    $json = viewerGet2("/file/list");
    is ($json->{recordsTotal}, 0, "Correct stats.json recordsTotal");

    $json = viewerGet2("/sessions.json");
    is ($json->{recordsTotal}, 0, "Correct sessions.json recordsTotal");
    is ($json->{health}->{number_of_data_nodes}, 1, "Correct sessions.json health number_of_data_nodes");
    is ($json->{graph}->{interval}, 60, "Correct sessions.json graph interval");
    is (scalar @{$json->{graph}->{lpHisto}}, 0, "Correct sessions.json graph lpHisto");
    is (scalar @{$json->{graph}->{paHisto}}, 0, "Correct sessions.json graph paHisto");
    is (scalar @{$json->{graph}->{lpHisto}}, 0, "Correct sessions.json graph lpHisto");
    is (scalar keys %{$json->{map}}, 0, "Correct sessions.json map");

    $json = viewerGet2("/spigraph.json");
    is ($json->{recordsTotal}, 0, "Correct spigraph.json recordsTotal");
    is ($json->{health}->{number_of_data_nodes}, 1, "Correct spigraph.json health number_of_data_nodes");
    is ($json->{graph}->{interval}, 60, "Correct spigraph.json graph interval");
    is (scalar @{$json->{graph}->{lpHisto}}, 0, "Correct spigraph.json graph lpHisto");
    is (scalar @{$json->{graph}->{paHisto}}, 0, "Correct spigraph.json graph paHisto");
    is (scalar @{$json->{graph}->{lpHisto}}, 0, "Correct spigraph.json graph lpHisto");
    is (scalar keys %{$json->{map}}, 0, "Correct spigraph.json map");

    $json = viewerGet2("/spiview.json");
    is (scalar keys %{$json->{spi}}, 0, "Empty spiview.json spi");
    is ($json->{recordsTotal}, 0, "Correct spiview.json recordsTotal");
    is (!exists $json->{graph}, 1, "Shouldn't have spiview.json graph");
    is (!exists $json->{map}, 1, "Shouldn't have spiview.json map");
    is (!exists $json->{health}, 1, "Shouldn't have spiview.json health");

    $json = viewerGet2("/spiview.json?spi=ta&facets=1");
    is (scalar keys %{$json->{spi}}, 1, "one spiview.json spi");
    is (scalar keys %{$json->{spi}->{ta}}, 2, "Two spiview.json ta elements");
    is ($json->{recordsTotal}, 0, "Correct spiview.json recordsTotal");
    is ($json->{health}->{number_of_data_nodes}, 1, "Correct spiview.json health number_of_data_nodes");
    is (scalar @{$json->{graph}->{lpHisto}}, 0, "Correct spiview.json graph lpHisto");
    is (scalar @{$json->{graph}->{paHisto}}, 0, "Correct spiview.json graph paHisto");
    is (scalar @{$json->{graph}->{lpHisto}}, 0, "Correct spiview.json graph lpHisto");
    is (scalar keys %{$json->{map}}, 2, "Correct spiview.json map");

    $json = viewerGet2("/connections.json");
    is ($json->{recordsFiltered}, 0, "Correct connections.json recordsFiltered");
    is ($json->{health}->{number_of_data_nodes}, 1, "Correct connections.json health number_of_data_nodes");
    is (!exists $json->{graph}, 1, "Shouldn't have connections.json graph");
    is (!exists $json->{map}, 1, "Shouldn't have connections.json map");

    $json = viewerGet2("/uniqueValue.json?type=tags");
    is (scalar @{$json}, 0, "Empty uniqueValue");
