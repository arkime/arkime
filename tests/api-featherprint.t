use Test::More tests => 44;
use ArkimeTest;
use JSON;
use Data::Dumper;
use strict;

# Featherprint is viewer-resident. The monitor runs only on the cron-leader;
# lookup and read APIs work on any viewer node. All feather_synthetic.pcap
# devices live in 10.177.0.0/16, which no other test pcap uses.
#
# Change events (changeMac/changeName/changeDevice/newService) fire across
# processing windows, and the whole pcap ingests inside one window -- so this
# test injects fresh session docs after the first drain and drains again to
# exercise the cross-window paths.

my $token = getTokenCookie();

# ---------------------------------------------------------------------------
# Drain the monitor over everything ingested so far.
my $proc = viewerGet("/regressionTests/processFeatherprints");
ok(defined $proc->{lpValue}, "processFeatherprints ran and returned a cursor");

# ---------------------------------------------------------------------------
# Classifications: every rule family gets exercised.
my %expect = (
  "10.177.1.50" => "apple_tv",         # nameContains via mDNS SRV target + A
  "10.177.1.51" => "chromecast",       # hasService _googlecast._tcp
  "10.177.1.52" => "printer",          # hasService _ipp._tcp
  "10.177.1.53" => "homepod",          # nameContains beats airplay_device
  "10.177.1.54" => "sonos",            # mDNS service + SSDP RINCON
  "10.177.1.55" => "nas",              # nameMatches synology
  "10.177.1.56" => "spotify_endpoint", # _services._dns-sd enumeration rdata
  "10.177.1.71" => "roku",             # ssdpUsnMatches roku:ecp
  "10.177.1.72" => "smart_tv",         # ssdpServerMatches LGTV
  "10.177.1.73" => "tradfri_gateway",  # ssdpServerMatches IKEA
  "10.177.0.12" => "raspberry_pi",     # macPrefixIn OUI beats linux vendor class
  "10.177.0.13" => "android",          # dhcpVendorClassMatches android-dhcp
  "10.177.0.14" => "windows_pc",       # dhcpVendorClassMatches MSFT
  "10.177.0.62" => "windows_pc",       # nameMatches desktop-xxxxxxx via NBNS
);
for my $ip (sort keys %expect) {
    my $r = viewerGet("/api/featherprint/ip/$ip");
    is($r->{device}->{classification}, $expect{$ip}, "$ip classified as $expect{$ip}");
}

# ---------------------------------------------------------------------------
# Device record details.
my $atv = viewerGet("/api/featherprint/ip/10.177.1.50")->{device};
ok((grep { $_->{type} eq '_airplay._tcp' } @{$atv->{services}}),
   "appletv has _airplay._tcp service");
ok((grep { $_->{type} eq '_airplay._tcp' && $_->{port} == 7000 } @{$atv->{services}}),
   "airplay service carries the SRV port");
ok((grep { $_->{name} eq 'appletv.local' } @{$atv->{names}}),
   "appletv hostname bound from mDNS");

my $win = viewerGet("/api/featherprint/ip/10.177.0.14")->{device};
is($win->{dhcp}->{vendorClass}, "MSFT 5.0",          "dhcp vendor class recorded");
ok(defined $win->{dhcp}->{paramReqList},             "dhcp param request list recorded");
is($win->{mac}->{value}, "aa:bb:cc:00:00:04",        "mac bound via arp");

my $roku = viewerGet("/api/featherprint/ip/10.177.1.71")->{device};
ok((grep { /Roku/ } @{$roku->{ssdp}->{server}}),     "ssdp server header recorded");

my $pi = viewerGet("/api/featherprint/ip/10.177.0.12")->{device};
is($pi->{mac}->{value}, "b8:27:eb:00:00:12",         "raspberry pi mac recorded");

# ---------------------------------------------------------------------------
# History + alerts from the first drain.
my $h10 = viewerGet("/api/featherprint/history/10.177.0.10?limit=50");
ok((grep { $_->{kind} eq 'newIp' } @{$h10->{history}}),  "newIp history recorded");
ok((grep { $_->{kind} eq 'newMac' } @{$h10->{history}}), "newMac history recorded");

my $al = viewerGet("/api/featherprint/alerts?limit=10000");
ok($al->{success} && ref($al->{alerts}) eq 'ARRAY',      "alerts list returns array");
ok((grep { $_->{kind} eq 'changeIp' } @{$al->{alerts}}), "changeIp alert fired for moved mac");
ok((grep { $_->{kind} eq 'newIp' } @{$al->{alerts}}),    "newIp alerts fired");

# Ack flow: ack one open alert, then find it via the server-side acked filter.
my ($openAlert) = grep { !$_->{acked} } @{$al->{alerts}};
my $ack = viewerPostToken("/api/featherprint/ack/$openAlert->{_id}", "", $token);
ok($ack->{success},                                      "alert ack succeeded");
my $ackedList = viewerGet("/api/featherprint/alerts?acked=true&limit=10000");
ok((grep { $_->{_id} eq $openAlert->{_id} } @{$ackedList->{alerts}}),
   "acked alert visible via acked=true filter");

# ---------------------------------------------------------------------------
# Search + validation.
my $list = viewerGet("/api/featherprint/search?limit=500");
ok($list->{success} && ref($list->{devices}) eq 'ARRAY', "search returns devices array");
my $bymac = viewerGet("/api/featherprint/search?mac=b8:27:eb&limit=10");
ok((grep { $_->{ip} eq '10.177.0.12' } @{$bymac->{devices}}), "search by mac substring finds pi");

my $bad = viewerGet("/api/featherprint/ip/notanip");
ok(!$bad->{success},                                     "invalid ip rejected");
my $missing = viewerGet("/api/featherprint/ip/203.0.113.99");
ok(!$missing->{success},                                 "unknown ip returns not found");

# ---------------------------------------------------------------------------
# On-demand transient lookup (start=1 covers the pcap's packet times).
my $lookup = viewerGet("/api/featherprint/lookup?ip=10.177.1.51&start=1");
is($lookup->{device}->{classification}, "chromecast",    "on-demand lookup classifies chromecast");

my $st = viewerGet("/api/featherprint/state");
ok($st->{success} && $st->{state}->{lpValue} > 0,        "monitor state has a cursor");

# ---------------------------------------------------------------------------
# Second window: inject fresh sessions and drain again so cross-window change
# events fire.
my $now = time() * 1000;
my $idx = "tests_sessions3-26m04";

sessionsPost($idx, "feathertestchangemac000000", qq({
  "\@timestamp": $now, "firstPacket": $now, "lastPacket": $now, "node": "test",
  "source": {"ip": "10.177.0.10"}, "destination": {"ip": "10.177.0.1"},
  "arp": {"ip": ["10.177.0.10"], "mac": ["ff:ee:dd:00:00:0a"]},
  "protocol": ["arp"], "protocolCnt": 1
}));
sessionsPost($idx, "feathertestchangename00000", qq({
  "\@timestamp": $now, "firstPacket": $now, "lastPacket": $now, "node": "test",
  "source": {"ip": "0.0.0.0"}, "destination": {"ip": "255.255.255.255"},
  "dhcp": {"host": ["bobs-third-laptop"], "mac": ["aa:bb:cc:00:00:01"], "requestIp": ["10.177.0.11"]},
  "protocol": ["udp", "dhcp"], "protocolCnt": 2
}));
sessionsPost($idx, "feathertestnewservice00000", qq({
  "\@timestamp": $now, "firstPacket": $now, "lastPacket": $now, "node": "test",
  "source": {"ip": "10.177.1.50"}, "destination": {"ip": "224.0.0.251"},
  "dns": [{"answersCnt": 1, "answers": [{"type": "PTR", "name": "_touch-able._tcp.local", "ptr": "appletv._touch-able._tcp.local", "class": "IN", "ttl": 4500}]}],
  "protocol": ["udp", "mdns"], "protocolCnt": 2
}));
sessionsPost($idx, "feathertestchangedevice000", qq({
  "\@timestamp": $now, "firstPacket": $now, "lastPacket": $now, "node": "test",
  "source": {"ip": "10.177.1.52"}, "destination": {"ip": "224.0.0.251"},
  "dns": [{"answersCnt": 1, "answers": [{"type": "PTR", "name": "_googlecast._tcp.local", "ptr": "hpprinter._googlecast._tcp.local", "class": "IN", "ttl": 4500}]}],
  "protocol": ["udp", "mdns"], "protocolCnt": 2
}));
sessionsPost($idx, "featherteststickyname00000", qq({
  "\@timestamp": $now, "firstPacket": $now, "lastPacket": $now, "node": "test",
  "source": {"ip": "0.0.0.0"}, "destination": {"ip": "255.255.255.255"},
  "dhcp": {"host": ["android-13"], "classId": ["MSFT 5.0"], "mac": ["aa:bb:cc:00:00:03"], "requestIp": ["10.177.0.13"]},
  "protocol": ["udp", "dhcp"], "protocolCnt": 2
}));

sleep(2); # ensure the drain's end-of-window passes the injected timestamps
my $proc2 = viewerGet("/regressionTests/processFeatherprints");
ok($proc2->{lpValue} >= $proc->{lpValue},                "second drain advanced the cursor");

$h10 = viewerGet("/api/featherprint/history/10.177.0.10?limit=50");
ok((grep { $_->{kind} eq 'changeMac' } @{$h10->{history}}), "changeMac recorded across windows");
is(viewerGet("/api/featherprint/ip/10.177.0.10")->{device}->{mac}->{value},
   "ff:ee:dd:00:00:0a",                                  "device record follows the new mac");

my $h11 = viewerGet("/api/featherprint/history/10.177.0.11?limit=50");
ok((grep { $_->{kind} eq 'changeName' } @{$h11->{history}}), "changeName recorded for dhcp rename");

my $h50 = viewerGet("/api/featherprint/history/10.177.1.50?limit=50");
ok((grep { $_->{kind} eq 'newService' && $_->{after}->{type} eq '_touch-able._tcp' } @{$h50->{history}}),
   "newService recorded for later announcement");

# The printer starts casting: chromecast's rule ordering beats printer.
my $h52 = viewerGet("/api/featherprint/history/10.177.1.52?limit=50");
ok((grep { $_->{kind} eq 'changeDevice' } @{$h52->{history}}), "changeDevice recorded on reclassification");
is(viewerGet("/api/featherprint/ip/10.177.1.52")->{device}->{classification},
   "chromecast",                                         "reclassified device record updated");

# A later MSFT vendor class does NOT reclassify android-13: the android rule
# matches its sticky discovered name first (rule ordering is deliberate).
is(viewerGet("/api/featherprint/ip/10.177.0.13")->{device}->{classification},
   "android",                                            "sticky name outranks later vendor class");
