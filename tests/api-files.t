use Test::More tests => 19;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $json;
my $mjson;

# Basic list
$json = viewerGet("/file/list");
$mjson = multiGet("/file/list");
eq_or_diff($mjson, $json, "single doesn't match multi", { context => 3 });

cmp_ok ($json->{recordsTotal}, ">=", 108);
cmp_ok ($json->{recordsFiltered}, ">=", 108);
delete $json->{data}->[0]->{first};
cmp_ok ($json->{data}->[0]->{num}, "<", $json->{data}->[1]->{num});

# name sort
$json = viewerGet("/file/list?sortField=name");
$mjson = multiGet("/file/list?sortField=name");
eq_or_diff($mjson, $json, "single doesn't match multi", { context => 3 });

cmp_ok ($json->{data}->[0]->{name}, "lt", $json->{data}->[1]->{name});

# reverse name sort
$json = viewerGet("/file/list?sortField=name&desc=true");
$mjson = multiGet("/file/list?sortField=name&desc=true");
eq_or_diff($mjson, $json, "single doesn't match multi", { context => 3 });

cmp_ok ($json->{data}->[0]->{name}, "gt", $json->{data}->[1]->{name});

# filter
$json = viewerGet("/file/list?sortField=name&desc=true&filter=v6-http");
$mjson = multiGet("/file/list?sortField=name&desc=true&filter=v6-http");
eq_or_diff($mjson, $json, "single doesn't match multi", { context => 3 });

cmp_ok ($json->{recordsTotal}, ">=", 108);
cmp_ok ($json->{recordsFiltered}, "==", 1);
delete $json->{data}->[0]->{id};
delete $json->{data}->[0]->{num};
delete $json->{data}->[0]->{first};
eq_or_diff($json->{data}->[0], from_json('{"locked":1,"filesize":9159,"node":"test","name":"' . getcwd() . '/pcap/v6-http.pcap"}'));

# filter 2
$json = viewerGet("/file/list?sortField=name&desc=true&filter=/v6");
$mjson = multiGet("/file/list?sortField=name&desc=true&filter=/v6");
eq_or_diff($mjson, $json, "single doesn't match multi", { context => 3 });

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
$json = viewerGet("/file/list?sortField=name&desc=true&filter=sillyname");
$mjson = multiGet("/file/list?sortField=name&desc=true&filter=sillyname");
eq_or_diff($mjson, $json, "single doesn't match multi", { context => 3 });

cmp_ok ($json->{recordsTotal}, ">=", 108);
cmp_ok ($json->{recordsFiltered}, "==", 0);
