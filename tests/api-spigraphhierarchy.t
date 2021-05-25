use Test::More tests => 14;
use Cwd;
use URI::Escape;
use MolochTest;
use Test::Differences;
use Data::Dumper;
use JSON;
use strict;


sub get {
my ($param) = @_;

#    diag "/spigraphhierarchy?$param";
    my $json = viewerGet("/spigraphhierarchy?$param");
    my $mjson = multiGet("/spigraphhierarchy?$param");

    eq_or_diff($mjson, $json, "single doesn't match multi for $param", { context => 3 });

    return $json
}

my $pwd = "*/pcap";
my $filestr = "(file=$pwd/socks-http-example.pcap||file=$pwd/socks-http-pass.pcap||file=$pwd/socks-https-example.pcap||file=$pwd/socks5-http-302.pcap||file=$pwd/socks5-rdp.pcap||file=$pwd/socks5-reverse.pcap||file=$pwd/socks5-smtp-503.pcap||file=$pwd/v6-http.pcap)";
my $files = uri_escape($filestr);


my $json = get("");
eq_or_diff($json, from_json('{"success":false,"text":"Missing exp parameter"}'));

my $json = get("exp=unknownfield");
eq_or_diff($json, from_json('{"success":false,"text":"Unknown expression unknownfield\n"}'));

my $json = get("date=-1&exp=node&expression=$files");
eq_or_diff($json, from_json('{"tableResults":[{"name":"test","parents":[],"size":19}],"success":true,"hierarchicalResults":{"children":[{"size":19,"name":"test"}],"name":"Top Talkers"}}'));

my $json = get("date=-1&exp=ip.dst:port&expression=file=$pwd/socks-https-example.pcap");
eq_or_diff($json, from_json('{"hierarchicalResults":{"name":"Top Talkers","children":[{"size":3,"name":"10.180.156.249:1080"}]},"success":true,"tableResults":[{"size":3,"parents":[],"name":"10.180.156.249:1080"}]}'));

my $json = get("date=-1&exp=node,ip.src&expression=$files");
eq_or_diff($json, from_json('{"success":true,"tableResults":[{"name":"10.180.156.185","parents":[{"size":19,"name":"test"}],"size":9},{"size":2,"parents":[{"size":19,"name":"test"}],"name":"10.0.0.1"},{"size":2,"parents":[{"size":19,"name":"test"}],"name":"fe80::211:25ff:fe82:95b5"},{"size":1,"parents":[{"size":19,"name":"test"}],"name":"::"},{"size":1,"parents":[{"size":19,"name":"test"}],"name":"10.0.0.2"},{"parents":[{"size":19,"name":"test"}],"name":"10.0.0.3","size":1},{"size":1,"name":"2001:6f8:102d:0:2d0:9ff:fee3:e8de","parents":[{"size":19,"name":"test"}]},{"name":"2001:6f8:102d:0:1033:c4c:7e57:b19e","parents":[{"size":19,"name":"test"}],"size":1},{"name":"fe80::2d0:9ff:fee3:e8de","parents":[{"size":19,"name":"test"}],"size":1}],"hierarchicalResults":{"name":"Top Talkers","children":[{"sizeValue":19,"name":"test","children":[{"name":"10.180.156.185","size":9},{"name":"10.0.0.1","size":2},{"size":2,"name":"fe80::211:25ff:fe82:95b5"},{"size":1,"name":"::"},{"name":"10.0.0.2","size":1},{"size":1,"name":"10.0.0.3"},{"size":1,"name":"2001:6f8:102d:0:2d0:9ff:fee3:e8de"},{"name":"2001:6f8:102d:0:1033:c4c:7e57:b19e","size":1},{"size":1,"name":"fe80::2d0:9ff:fee3:e8de"}]}]}}'));

my $json = get("date=-1&exp=ip.src,node&expression=$files");
eq_or_diff($json, from_json('{"success":true,"tableResults":[{"parents":[{"name":"10.180.156.185","size":9}],"name":"test","size":9},{"size":2,"name":"test","parents":[{"name":"10.0.0.1","size":2}]},{"parents":[{"name":"fe80::211:25ff:fe82:95b5","size":2}],"name":"test","size":2},{"size":1,"name":"test","parents":[{"size":1,"name":"::"}]},{"name":"test","size":1,"parents":[{"name":"10.0.0.2","size":1}]},{"size":1,"name":"test","parents":[{"size":1,"name":"10.0.0.3"}]},{"name":"test","size":1,"parents":[{"size":1,"name":"2001:6f8:102d:0:2d0:9ff:fee3:e8de"}]},{"parents":[{"name":"2001:6f8:102d:0:1033:c4c:7e57:b19e","size":1}],"name":"test","size":1},{"size":1,"name":"test","parents":[{"size":1,"name":"fe80::2d0:9ff:fee3:e8de"}]}],"hierarchicalResults":{"name":"Top Talkers","children":[{"children":[{"name":"test","size":9}],"name":"10.180.156.185","sizeValue":9},{"name":"10.0.0.1","sizeValue":2,"children":[{"name":"test","size":2}]},{"sizeValue":2,"name":"fe80::211:25ff:fe82:95b5","children":[{"size":2,"name":"test"}]},{"children":[{"size":1,"name":"test"}],"sizeValue":1,"name":"::"},{"children":[{"name":"test","size":1}],"name":"10.0.0.2","sizeValue":1},{"children":[{"name":"test","size":1}],"sizeValue":1,"name":"10.0.0.3"},{"children":[{"name":"test","size":1}],"name":"2001:6f8:102d:0:2d0:9ff:fee3:e8de","sizeValue":1},{"children":[{"size":1,"name":"test"}],"sizeValue":1,"name":"2001:6f8:102d:0:1033:c4c:7e57:b19e"},{"children":[{"name":"test","size":1}],"sizeValue":1,"name":"fe80::2d0:9ff:fee3:e8de"}]}}'));

my $json = get("date=-1&exp=node,ip.src,ip.dst&expression=$files");
eq_or_diff($json, from_json('{"hierarchicalResults":{"children":[{"name":"test","sizeValue":19,"children":[{"children":[{"name":"10.180.156.249","size":9}],"sizeValue":9,"name":"10.180.156.185"},{"children":[{"name":"10.0.0.2","size":2}],"sizeValue":2,"name":"10.0.0.1"},{"sizeValue":2,"name":"fe80::211:25ff:fe82:95b5","children":[{"name":"ff02::1","size":1},{"name":"ff02::1:ff82:95b5","size":1}]},{"children":[{"name":"ff02::1:ff98:6e1","size":1}],"sizeValue":1,"name":"::"},{"children":[{"size":1,"name":"10.0.0.1"}],"sizeValue":1,"name":"10.0.0.2"},{"children":[{"size":1,"name":"10.0.0.2"}],"sizeValue":1,"name":"10.0.0.3"},{"children":[{"size":1,"name":"2001:6f8:900:7c0::2"}],"name":"2001:6f8:102d:0:2d0:9ff:fee3:e8de","sizeValue":1},{"sizeValue":1,"name":"2001:6f8:102d:0:1033:c4c:7e57:b19e","children":[{"size":1,"name":"ff02::fb"}]},{"children":[{"name":"ff02::16","size":1}],"name":"fe80::2d0:9ff:fee3:e8de","sizeValue":1}]}],"name":"Top Talkers"},"success":true,"tableResults":[{"parents":[{"name":"test","size":19},{"name":"10.180.156.185","size":9}],"name":"10.180.156.249","size":9},{"parents":[{"name":"test","size":19},{"name":"10.0.0.1","size":2}],"name":"10.0.0.2","size":2},{"size":1,"name":"ff02::1","parents":[{"name":"test","size":19},{"size":2,"name":"fe80::211:25ff:fe82:95b5"}]},{"size":1,"name":"ff02::1:ff82:95b5","parents":[{"name":"test","size":19},{"name":"fe80::211:25ff:fe82:95b5","size":2}]},{"name":"ff02::1:ff98:6e1","parents":[{"name":"test","size":19},{"size":1,"name":"::"}],"size":1},{"name":"10.0.0.1","parents":[{"name":"test","size":19},{"name":"10.0.0.2","size":1}],"size":1},{"size":1,"parents":[{"name":"test","size":19},{"size":1,"name":"10.0.0.3"}],"name":"10.0.0.2"},{"parents":[{"name":"test","size":19},{"size":1,"name":"2001:6f8:102d:0:2d0:9ff:fee3:e8de"}],"name":"2001:6f8:900:7c0::2","size":1},{"size":1,"name":"ff02::fb","parents":[{"name":"test","size":19},{"name":"2001:6f8:102d:0:1033:c4c:7e57:b19e","size":1}]},{"name":"ff02::16","parents":[{"name":"test","size":19},{"size":1,"name":"fe80::2d0:9ff:fee3:e8de"}],"size":1}]}'));
