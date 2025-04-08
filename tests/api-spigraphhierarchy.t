use Test::More tests => 16;
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

my $pwd = "*/pcap";
my $filestr = "(file=$pwd/socks-http-example.pcap||file=$pwd/socks-http-pass.pcap||file=$pwd/socks-https-example.pcap||file=$pwd/socks5-http-302.pcap||file=$pwd/socks5-rdp.pcap||file=$pwd/socks5-reverse.pcap||file=$pwd/socks5-smtp-503.pcap||file=$pwd/v6-http.pcap)";
my $files = uri_escape($filestr);


my $json = get("");
eq_or_diff($json, from_json('{"success":false,"text":"Missing exp parameter"}'));

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
