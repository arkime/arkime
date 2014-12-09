# Tests on a fresh install
use Test::More tests => 21;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Data::Dumper;
use strict;

# Clear out node2
    system("../db/db.pl --prefix tests2 localhost:9200 initnoprompt 2>&1 1>/dev/null");
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
    is ($json->{iTotalRecords}, 0, "Correct stats.json iTotalRecords");

    $json = viewerGet2("/dstats.json");
    is (scalar @{$json}, 0, "Empty dstats");

    $json = viewerGet2("/files.json");
    is ($json->{iTotalRecords}, 0, "Correct stats.json iTotalRecords");

    $json = viewerGet2("/sessions.json");
    is ($json->{iTotalRecords}, 0, "Correct sessions.json iTotalRecords");
    is ($json->{health}->{number_of_data_nodes}, 1, "Correct sessions.json health number_of_data_nodes");
    is ($json->{graph}->{interval}, 60, "Correct sessions.json graph interval");

    $json = viewerGet2("/spigraph.json");
    is ($json->{iTotalRecords}, 0, "Correct spigraph.json iTotalRecords");
    is ($json->{health}->{number_of_data_nodes}, 1, "Correct spigraph.json health number_of_data_nodes");
    is ($json->{graph}->{interval}, 60, "Correct spigraph.json graph interval");

    $json = viewerGet2("/spiview.json");
    is (scalar keys %{$json->{spi}}, 0, "Empty spiview.json spi");
    is ($json->{iTotalRecords}, 0, "Correct spiview.json iTotalRecords");

    $json = viewerGet2("/spiview.json?spi=ta");
    is (scalar keys %{$json->{spi}}, 0, "Empty spiview.json spi");
    is ($json->{iTotalRecords}, 0, "Correct spiview.json iTotalRecords");
    is ($json->{health}->{number_of_data_nodes}, 1, "Correct spiview.json health number_of_data_nodes");

    $json = viewerGet2("/connections.json");
    is ($json->{iTotalDisplayRecords}, 0, "Correct connections.json iTotalDisplayRecords");
    is ($json->{health}->{number_of_data_nodes}, 1, "Correct connections.json health number_of_data_nodes");

    $json = viewerGet2("/uniqueValue.json?type=tags");
    is (scalar @{$json}, 0, "Empty uniqueValue");
