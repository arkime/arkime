use Test::More tests => 82;
use Cwd;
use URI::Escape;
use ArkimeTest;
use Test::Differences;
use Data::Dumper;
use JSON;
use strict;


sub get {
my ($param) = @_;

#    diag "/spigraphhierarchy?$param";
    my $json = viewerGet("/api/spigraphhierarchy?$param");
    my $mjson = multiGet("/api/spigraphhierarchy?$param");

    eq_or_diff($mjson, $json, "single doesn't match multi for $param", { context => 3 });

    return $json
}

# Every level of an "other" hierarchy adds up: a value's top children plus its
# Other hold all of its sessions (for single valued fields), Other is a leaf,
# a level keeps at most its size of values, and has at most one Other
sub otherOk {
    my ($node, $sizes, $depth) = @_;
    my ($sum, $values, $others) = (0, 0, 0);
    foreach my $kid (@{$node->{children} || []}) {
        $sum += $kid->{sizeValue} // $kid->{size};
        if ($kid->{other}) {
            $others++;
            return 0 if $kid->{children} || $kid->{name} ne 'Other';
        } else {
            $values++;
            return 0 if $kid->{children} && !otherOk($kid, $sizes, $depth + 1);
        }
    }
    return 0 if $others > 1 || $values > $sizes->[$depth];
    return 0 if defined $node->{sizeValue} && $sum != $node->{sizeValue};
    return 1;
}

# The Other nodes anywhere in a hierarchy
sub countOther {
    my ($node) = @_;
    my $count = $node->{other} ? 1 : 0;
    $count += countOther($_) foreach @{$node->{children} || []};
    return $count;
}

# The sessions the first level holds, Other included
sub topTotal {
    my ($hierarchy) = @_;
    my $total = 0;
    $total += $_->{sizeValue} // $_->{size} foreach @{$hierarchy->{children}};
    return $total;
}

# A column's or a flat list's values as [name, size] pairs
sub pairs {
    my ($values) = @_;
    return [map { [$_->{name}, $_->{size}] } @{$values}];
}

# The sessions a list of values holds
sub sizeTotal {
    my ($values) = @_;
    my $total = 0;
    $total += $_->{size} foreach @{$values};
    return $total;
}

my $pwd = "*/pcap";
my $filestr = "(file=$pwd/socks-http-example.pcap||file=$pwd/socks-http-pass.pcap||file=$pwd/socks-https-example.pcap||file=$pwd/socks5-http-302.pcap||file=$pwd/socks5-rdp.pcap||file=$pwd/socks5-reverse.pcap||file=$pwd/socks5-smtp-503.pcap||file=$pwd/v6-http.pcap)";
my $files = uri_escape($filestr);


my $json = get("");
is($json->{success}, 0, "Missing exp parameter");
is($json->{i18n}, "api.sessions.missingExpParam", "Missing exp parameter i18n");

my $json = get("exp=unknownfield");
eq_or_diff($json, from_json('{"success":false,"text":"Unknown expression unknownfield\n"}'));

my $json = get("date=-1&exp=node&expression=$files");
eq_or_diff($json, from_json('{"tableResults":[{"size":19,"parents":[],"name":"test"}],"success":true,"hierarchicalResults":{"children":[{"srcips":9,"name":"test","size":19,"dstips":9}],"name":"Top Talkers"}}'));

my $json = get("date=-1&exp=node&expression=$files&view=unknown");
eq_or_diff($json, from_json('{"success":false,"text":"Can\'t find view"}'));

my $json = get("date=-1&exp=ip.dst:port&expression=file=$pwd/socks-https-example.pcap");
eq_or_diff($json, from_json('{"tableResults":[{"parents":[],"size":3,"name":"10.180.156.249:1080"}],"success":true,"hierarchicalResults":{"name":"Top Talkers","children":[{"size":3,"dstips":1,"srcips":1,"name":"10.180.156.249:1080"}]}}'));

my $json = get("date=-1&exp=node,ip.src&expression=$files");
eq_or_diff($json, from_json('{"hierarchicalResults":{"name":"Top Talkers","children":[{"dstips":9,"sizeValue":19,"name":"test","srcips":9,"children":[{"size":9,"srcips":1,"name":"10.180.156.185","dstips":1},{"name":"10.0.0.1","dstips":1,"size":2,"srcips":1},{"size":2,"srcips":1,"dstips":2,"name":"fe80::211:25ff:fe82:95b5"},{"dstips":1,"name":"::","srcips":1,"size":1},{"size":1,"srcips":1,"name":"10.0.0.2","dstips":1},{"name":"10.0.0.3","dstips":1,"size":1,"srcips":1},{"dstips":1,"name":"2001:6f8:102d:0:2d0:9ff:fee3:e8de","srcips":1,"size":1},{"srcips":1,"size":1,"dstips":1,"name":"2001:6f8:102d:0:1033:c4c:7e57:b19e"},{"name":"fe80::2d0:9ff:fee3:e8de","dstips":1,"srcips":1,"size":1}]}]},"success":true,"tableResults":[{"size":9,"parents":[{"size":19,"name":"test"}],"name":"10.180.156.185"},{"parents":[{"size":19,"name":"test"}],"name":"10.0.0.1","size":2},{"name":"fe80::211:25ff:fe82:95b5","parents":[{"name":"test","size":19}],"size":2},{"size":1,"parents":[{"size":19,"name":"test"}],"name":"::"},{"name":"10.0.0.2","parents":[{"size":19,"name":"test"}],"size":1},{"parents":[{"name":"test","size":19}],"name":"10.0.0.3","size":1},{"size":1,"name":"2001:6f8:102d:0:2d0:9ff:fee3:e8de","parents":[{"size":19,"name":"test"}]},{"size":1,"parents":[{"name":"test","size":19}],"name":"2001:6f8:102d:0:1033:c4c:7e57:b19e"},{"size":1,"parents":[{"name":"test","size":19}],"name":"fe80::2d0:9ff:fee3:e8de"}]}'));

my $json = get("date=-1&exp=ip.src,node&expression=$files");
eq_or_diff($json, from_json('{"success":true,"tableResults":[{"size":9,"name":"test","parents":[{"size":9,"name":"10.180.156.185"}]},{"size":2,"name":"test","parents":[{"size":2,"name":"10.0.0.1"}]},{"parents":[{"size":2,"name":"fe80::211:25ff:fe82:95b5"}],"name":"test","size":2},{"parents":[{"size":1,"name":"::"}],"size":1,"name":"test"},{"size":1,"name":"test","parents":[{"name":"10.0.0.2","size":1}]},{"size":1,"name":"test","parents":[{"name":"10.0.0.3","size":1}]},{"name":"test","size":1,"parents":[{"name":"2001:6f8:102d:0:2d0:9ff:fee3:e8de","size":1}]},{"parents":[{"size":1,"name":"2001:6f8:102d:0:1033:c4c:7e57:b19e"}],"name":"test","size":1},{"size":1,"name":"test","parents":[{"size":1,"name":"fe80::2d0:9ff:fee3:e8de"}]}],"hierarchicalResults":{"name":"Top Talkers","children":[{"children":[{"srcips":1,"name":"test","size":9,"dstips":1}],"sizeValue":9,"name":"10.180.156.185","dstips":1,"srcips":1},{"name":"10.0.0.1","dstips":1,"children":[{"srcips":1,"name":"test","dstips":1,"size":2}],"sizeValue":2,"srcips":1},{"children":[{"size":2,"dstips":2,"name":"test","srcips":1}],"sizeValue":2,"name":"fe80::211:25ff:fe82:95b5","dstips":2,"srcips":1},{"srcips":1,"sizeValue":1,"children":[{"dstips":1,"size":1,"name":"test","srcips":1}],"dstips":1,"name":"::"},{"srcips":1,"name":"10.0.0.2","dstips":1,"children":[{"name":"test","size":1,"dstips":1,"srcips":1}],"sizeValue":1},{"srcips":1,"children":[{"name":"test","size":1,"dstips":1,"srcips":1}],"sizeValue":1,"name":"10.0.0.3","dstips":1},{"srcips":1,"children":[{"dstips":1,"size":1,"name":"test","srcips":1}],"sizeValue":1,"name":"2001:6f8:102d:0:2d0:9ff:fee3:e8de","dstips":1},{"srcips":1,"sizeValue":1,"children":[{"srcips":1,"name":"test","size":1,"dstips":1}],"dstips":1,"name":"2001:6f8:102d:0:1033:c4c:7e57:b19e"},{"children":[{"srcips":1,"dstips":1,"size":1,"name":"test"}],"sizeValue":1,"name":"fe80::2d0:9ff:fee3:e8de","dstips":1,"srcips":1}]}}'));

my $json = get("date=-1&exp=node,ip.src,ip.dst&expression=$files");
eq_or_diff($json, from_json('{"hierarchicalResults":{"name":"Top Talkers","children":[{"srcips":9,"sizeValue":19,"children":[{"name":"10.180.156.185","dstips":1,"children":[{"name":"10.180.156.249","dstips":1,"size":9,"srcips":1}],"sizeValue":9,"srcips":1},{"children":[{"srcips":1,"size":2,"dstips":1,"name":"10.0.0.2"}],"sizeValue":2,"name":"10.0.0.1","dstips":1,"srcips":1},{"children":[{"dstips":1,"size":1,"name":"ff02::1","srcips":1},{"size":1,"dstips":1,"name":"ff02::1:ff82:95b5","srcips":1}],"sizeValue":2,"name":"fe80::211:25ff:fe82:95b5","dstips":2,"srcips":1},{"sizeValue":1,"children":[{"srcips":1,"size":1,"dstips":1,"name":"ff02::1:ff98:6e1"}],"dstips":1,"name":"::","srcips":1},{"sizeValue":1,"children":[{"name":"10.0.0.1","size":1,"dstips":1,"srcips":1}],"dstips":1,"name":"10.0.0.2","srcips":1},{"sizeValue":1,"children":[{"size":1,"dstips":1,"name":"10.0.0.2","srcips":1}],"dstips":1,"name":"10.0.0.3","srcips":1},{"srcips":1,"sizeValue":1,"children":[{"srcips":1,"dstips":1,"size":1,"name":"2001:6f8:900:7c0::2"}],"dstips":1,"name":"2001:6f8:102d:0:2d0:9ff:fee3:e8de"},{"srcips":1,"name":"2001:6f8:102d:0:1033:c4c:7e57:b19e","dstips":1,"children":[{"name":"ff02::fb","size":1,"dstips":1,"srcips":1}],"sizeValue":1},{"srcips":1,"sizeValue":1,"children":[{"name":"ff02::16","size":1,"dstips":1,"srcips":1}],"dstips":1,"name":"fe80::2d0:9ff:fee3:e8de"}],"dstips":9,"name":"test"}]},"tableResults":[{"parents":[{"name":"test","size":19},{"size":9,"name":"10.180.156.185"}],"name":"10.180.156.249","size":9},{"name":"10.0.0.2","size":2,"parents":[{"name":"test","size":19},{"size":2,"name":"10.0.0.1"}]},{"parents":[{"name":"test","size":19},{"name":"fe80::211:25ff:fe82:95b5","size":2}],"size":1,"name":"ff02::1"},{"parents":[{"size":19,"name":"test"},{"name":"fe80::211:25ff:fe82:95b5","size":2}],"name":"ff02::1:ff82:95b5","size":1},{"size":1,"name":"ff02::1:ff98:6e1","parents":[{"name":"test","size":19},{"size":1,"name":"::"}]},{"name":"10.0.0.1","size":1,"parents":[{"size":19,"name":"test"},{"size":1,"name":"10.0.0.2"}]},{"parents":[{"size":19,"name":"test"},{"size":1,"name":"10.0.0.3"}],"name":"10.0.0.2","size":1},{"size":1,"name":"2001:6f8:900:7c0::2","parents":[{"size":19,"name":"test"},{"size":1,"name":"2001:6f8:102d:0:2d0:9ff:fee3:e8de"}]},{"parents":[{"name":"test","size":19},{"name":"2001:6f8:102d:0:1033:c4c:7e57:b19e","size":1}],"size":1,"name":"ff02::fb"},{"parents":[{"name":"test","size":19},{"size":1,"name":"fe80::2d0:9ff:fee3:e8de"}],"size":1,"name":"ff02::16"}],"success":true}'));

# single field (the Intersection dashboard widget): flat value/count rows with no
# parents, honoring the size (the widget's Top-N). count == doc_count per value.
my $hier = get("date=-1&exp=ip.src&size=3&expression=$files");
is ($hier->{success}, 1, "single-field intersection success");
cmp_ok (scalar(@{$hier->{tableResults}}), '>', 0, "single-field intersection returns rows");
cmp_ok (scalar(@{$hier->{tableResults}}), '<=', 3, "single-field intersection honors size");
is (scalar(@{$hier->{tableResults}->[0]->{parents}}), 0, "single-field intersection rows have no parents");
ok (defined $hier->{tableResults}->[0]->{name} && defined $hier->{tableResults}->[0]->{size}, "single-field intersection row has name + size");

# a fractional size is used as a whole number at every level; it used to hang
# the multi viewer's nested terms merge
my $frac = get("date=-1&exp=ip.src,ip.dst&size=1.5&expression=$files");
eq_or_diff($frac, get("date=-1&exp=ip.src,ip.dst&size=1&expression=$files"), "fractional size matches size 1");

# per-field sizes override the shared one (a split pie: few pies, many slices)
my $sized = get("date=-1&exp=node,ip.src&size=20&sizes=1,2&expression=$files");
is (scalar(@{$sized->{hierarchicalResults}->{children}}), 1, "sizes caps the first level");
is (scalar(@{$sized->{hierarchicalResults}->{children}->[0]->{children}}), 2, "sizes caps the second level");

# other adds an "Other" leaf per level holding what is outside its top values
my $other = get("date=-1&exp=node,ip.src&size=2&other=true&expression=$files");
my $test = $other->{hierarchicalResults}->{children}->[0];
my @kids = @{$test->{children}};
is (scalar(@kids), 3, "two top values plus Other");
ok ($kids[2]->{other} && $kids[2]->{name} eq 'Other', "the last child is Other");
is ($kids[0]->{size} + $kids[1]->{size} + $kids[2]->{size}, $test->{sizeValue}, "Other holds the rest of its parent's sessions");
ok (!grep({ $_->{other} } @{$other->{tableResults}}), "Other stays out of the table rows");

# seven nested fields (a Kibana data table nests that deep)
my $seven = get("date=-1&exp=node,ip.src,ip.dst,port.src,port.dst,protocols,ip.protocol&size=2&expression=$files");
is ($seven->{success}, 1, "seven nested fields");
ok (scalar(@{$seven->{tableResults}}) > 0, "seven nested fields return rows");
is (scalar(@{$seven->{tableResults}->[0]->{parents}}), 6, "each row carries its six ancestors");

# a split pie, as the dashboard asks for one: sizes caps the pies (the split
# field) and their slices, and other adds an Other to every level
my $split = get("date=-1&exp=ip.dst,port.dst&size=20&sizes=2,3&other=true");
my $splitPies = $split->{hierarchicalResults}->{children};
is (scalar(grep { !$_->{other} } @{$splitPies}), 2, "split pie: sizes caps the pies");
is (scalar(grep { $_->{other} } @{$splitPies}), 1, "split pie: an Other for the values past the pies");
ok (otherOk($split->{hierarchicalResults}, [2, 3], 0), "split pie: every level adds up with its Other");
is (topTotal($split->{hierarchicalResults}), topTotal(get("date=-1&exp=ip.dst,port.dst&sizes=1000,1")->{hierarchicalResults}),
    "split pie: the pies plus Other are every session with both fields");
eq_or_diff ($split->{tableResults}, get("date=-1&exp=ip.dst,port.dst&size=20&sizes=2,3")->{tableResults}, "other leaves the table rows alone");

# a nested pie: an Other at every level, not just the first
my $nested = get("date=-1&exp=ip.protocol,ip.dst,port.dst&size=2&other=true");
ok (otherOk($nested->{hierarchicalResults}, [2, 2, 2], 0), "nested pie: every level adds up with its Other");
cmp_ok (scalar(grep { $_->{other} } map { @{$_->{children} || []} } @{$nested->{hierarchicalResults}->{children}}), '>', 0,
    "nested pie: Other below the first level");

# no Other when the top values are every value, or when it isn't asked for
is (countOther(get("date=-1&exp=node&size=10&other=true&expression=$files")->{hierarchicalResults}), 0, "no Other when nothing is left over");
is (countOther(get("date=-1&exp=ip.dst&size=2&other=false")->{hierarchicalResults}), 0, "other=false adds no Other");

# sizes that aren't a positive number, and fields past the sizes, use size
eq_or_diff (get("date=-1&exp=node,ip.src&size=2&sizes=abc,0&expression=$files"), get("date=-1&exp=node,ip.src&size=2&expression=$files"),
    "unusable sizes fall back to size");
is (scalar(@{get("date=-1&exp=node,ip.src&size=2&sizes=1&expression=$files")->{hierarchicalResults}->{children}->[0]->{children}}), 2,
    "a field past the sizes uses size");

# mode=unique: each field's own top values (the dashboard's table, /unique per field),
# rather than their combinations (the intersection)
my $unique = get("date=-1&exp=ip.src,ip.dst,port.dst&size=5&mode=unique");
is ($unique->{success}, 1, "unique success");
is_deeply ([map { $_->{exp} } @{$unique->{columns}}], ['ip.src', 'ip.dst', 'port.dst'], "unique: a column per field, in order");
ok (!exists $unique->{hierarchicalResults} && !exists $unique->{tableResults}, "unique: columns instead of the nested results");
for my $i (0 .. 2) {
    my $exp = $unique->{columns}->[$i]->{exp};
    eq_or_diff (pairs($unique->{columns}->[$i]->{values}), pairs(get("date=-1&exp=$exp&size=5")->{tableResults}),
        "unique: the $exp column is ${exp}'s own top values");
}
my $uniqueSorted = 1;
foreach my $column (@{$unique->{columns}}) {
    my @sizes = map { $_->{size} } @{$column->{values}};
    $uniqueSorted = 0 if join(',', @sizes) ne join(',', sort { $b <=> $a } @sizes);
    $uniqueSorted = 0 if !defined $column->{values}->[0]->{srcips} || !defined $column->{values}->[0]->{dstips};
}
ok ($uniqueSorted, "unique: each column is ranked by count and carries its ip counts");

# a session counts in a column when it has that field, whatever the others
my $uniqueAll = get("date=-1&exp=ip.dst,host.http&size=1000&mode=unique");
is (sizeTotal($uniqueAll->{columns}->[0]->{values}), sizeTotal(get("date=-1&exp=ip.dst&size=1000")->{tableResults}),
    "unique: a column counts every session with its field");
cmp_ok (sizeTotal($uniqueAll->{columns}->[0]->{values}), '>', topTotal(get("date=-1&exp=ip.dst,host.http&sizes=1000,1")->{hierarchicalResults}),
    "unique: unlike the intersection, a session without the other fields still counts");

# sizes per column, Other never added, and the script fields work
my $uniqueSized = get("date=-1&exp=ip.src,ip.dst,port.dst&size=5&sizes=2,3,1&mode=unique");
is_deeply ([map { scalar(@{$_->{values}}) } @{$uniqueSized->{columns}}], [2, 3, 1], "unique: sizes cap each column");
ok (!grep({ $_->{other} } map { @{$_->{values}} } @{get("date=-1&exp=ip.src,ip.dst&size=2&mode=unique&other=true")->{columns}}),
    "unique: no Other");
my $uniqueScript = get("date=-1&exp=ip.dst:port,ip.src&mode=unique&expression=file=$pwd/socks-https-example.pcap");
eq_or_diff (pairs($uniqueScript->{columns}->[0]->{values}), [['10.180.156.249:1080', 3]], "unique: an ip:port column");

# an unknown field is an error, whatever the mode
my $uniqueBad = get("date=-1&exp=ip.src,unknownfield&mode=unique");
eq_or_diff ($uniqueBad, from_json('{"success":false,"text":"Unknown expression unknownfield\n"}'), "unique: unknown field");
