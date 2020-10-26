use Test::More tests => 19;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $json;

sub get {
my ($url) = @_;

    my $json = viewerGet($url);
    my $mjson = multiGet($url);

    for (my $i=0; $i < scalar(@{$mjson->{data}}); $i++) { delete $mjson->{data}->[$i]->{escluster}; }
    eq_or_diff($mjson, $json, "single doesn't match multi for $url", { context => 3 });

    return $json
}


# Basic list
$json = get("/file/list");

cmp_ok ($json->{recordsTotal}, ">=", 108);
cmp_ok ($json->{recordsFiltered}, ">=", 108);
delete $json->{data}->[0]->{first};
cmp_ok ($json->{data}->[0]->{num}, "<", $json->{data}->[1]->{num});

# name sort
$json = get("/file/list?sortField=name");

cmp_ok ($json->{data}->[0]->{name}, "lt", $json->{data}->[1]->{name});

# reverse name sort
$json = get("/file/list?sortField=name&desc=true");

cmp_ok ($json->{data}->[0]->{name}, "gt", $json->{data}->[1]->{name});

# filter
$json = get("/file/list?sortField=name&desc=true&filter=v6-http");

cmp_ok ($json->{recordsTotal}, ">=", 108);
cmp_ok ($json->{recordsFiltered}, "==", 1);
delete $json->{data}->[0]->{id};
delete $json->{data}->[0]->{num};
delete $json->{data}->[0]->{first};
eq_or_diff($json->{data}->[0], from_json('{"locked":1,"filesize":9159,"node":"test","name":"' . getcwd() . '/pcap/v6-http.pcap"}'));

# filter 2
$json = get("/file/list?sortField=name&desc=true&filter=/v6");

cmp_ok ($json->{recordsTotal}, ">=", 108);
cmp_ok ($json->{recordsFiltered}, "==", 2);
delete $json->{data}->[0]->{id};
delete $json->{data}->[0]->{num};
delete $json->{data}->[0]->{first};
delete $json->{data}->[1]->{id};
delete $json->{data}->[1]->{num};
delete $json->{data}->[1]->{first};
eq_or_diff($json->{data}, from_json('[{"locked":1,"filesize":28251,"node":"test","name":"' . getcwd() . '/pcap/v6.pcap"},' .
                                     '{"locked":1,"filesize":9159,"node":"test","name":"' . getcwd() . '/pcap/v6-http.pcap"}]'));

# filter emptry
$json = get("/file/list?sortField=name&desc=true&filter=sillyname");

cmp_ok ($json->{recordsTotal}, ">=", 108);
cmp_ok ($json->{recordsFiltered}, "==", 0);
