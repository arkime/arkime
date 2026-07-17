use Test::More tests => 44;
use Cwd;
use URI::Escape;
use ArkimeTest;
use Test::Differences;
use strict;

# multidlt_synthetic.pcapng is a single pcapng file with 6 interfaces using 3
# different link types (2x EN10MB, 2x RAW, 2x NULL). It is loaded by the global
# "-R pcap" regression load (offlineFilenameRegex now matches .pcapng). These
# tests verify per-interface/per-packet DLT handling end to end: sessions are
# keyed by 5-tuple regardless of which interface/DLT carried each packet, and
# the viewer can read packets back out of the pcapng for every link type.

my $files = "file=*/pcap/multidlt_synthetic.pcapng";

# 8 sessions total in the file
countTest(8, "date=-1&expression=" . uri_escape("$files"));

# 4 HTTP sessions (one per HTTP flow A,C,E,G) classified across EN10MB/RAW/NULL
countTest(4, "date=-1&expression=" . uri_escape("$files&&protocols==http"));

# 4 DNS sessions (flows B,D,F,H) classified across EN10MB/RAW/NULL
countTest(4, "date=-1&expression=" . uri_escape("$files&&protocols==dns"));

# HTTP hosts are parsed regardless of which DLT carried the packets
countTest(1, "date=-1&expression=" . uri_escape("$files&&host.http==a.example.com"));
countTest(1, "date=-1&expression=" . uri_escape("$files&&host.http==c.example.com"));
countTest(1, "date=-1&expression=" . uri_escape("$files&&host.http==e.example.com"));
countTest(1, "date=-1&expression=" . uri_escape("$files&&host.http==g.example.com"));

# DNS hosts likewise
countTest(1, "date=-1&expression=" . uri_escape("$files&&host.dns==b.example.com"));
countTest(1, "date=-1&expression=" . uri_escape("$files&&host.dns==d.example.com"));
countTest(1, "date=-1&expression=" . uri_escape("$files&&host.dns==f.example.com"));
countTest(1, "date=-1&expression=" . uri_escape("$files&&host.dns==h.example.com"));

# MACs are only populated for the Ethernet (EN10MB) interfaces. Flow A's
# handshake spans EN10MB+RAW+NULL, flows C/D are pure EN10MB, and flow B's DNS
# query is on EN10MB(1), so 4 sessions carry the synthetic source MAC; the pure
# RAW/NULL sessions carry none.
countTest(4, "date=-1&expression=" . uri_escape("$files&&mac.src==00:11:22:33:44:55"));

# Flow A: cross-DLT HTTP session (EN10MB iface0 + RAW iface2 + NULL iface4).
# Verify it is a single session and the viewer can read its packets back from
# the pcapng across all three link types, reassembling the HTTP request.
# The /packets HTML-escapes payload bytes, so '/' renders as '&#47;'.
my $json = viewerGet("/api/sessions?date=-1&expression=" . uri_escape("$files&&host.http==a.example.com") . "&fields=id");
is (scalar @{$json->{data}}, 1, "flow A is a single cross-DLT session");
my $aid = $json->{data}->[0]->{id};
my $content = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$aid/packets?line=false&ts=false&base=ascii")->content;
ok ($content =~ /GET &#47; HTTP&#47;1.1/, "flow A: HTTP request read back across DLTs");
ok ($content =~ /Host: a.example.com/, "flow A: HTTP host read back across DLTs");
ok ($content =~ /HTTP&#47;1.1 200 OK/, "flow A: HTTP response read back across DLTs");

# Flow E: single RAW (linktype 101) HTTP session - exercises non-Ethernet
# read-back where there is no link-layer header to strip.
$json = viewerGet("/api/sessions?date=-1&expression=" . uri_escape("$files&&host.http==e.example.com") . "&fields=id");
my $eid = $json->{data}->[0]->{id};
$content = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$eid/packets?line=false&ts=false&base=ascii")->content;
ok ($content =~ /GET &#47; HTTP&#47;1.1/, "flow E: RAW HTTP request read back");
ok ($content =~ /Host: e.example.com/, "flow E: RAW HTTP host read back");

# Flow G: single NULL (linktype 0, BSD loopback) HTTP session.
$json = viewerGet("/api/sessions?date=-1&expression=" . uri_escape("$files&&host.http==g.example.com") . "&fields=id");
my $gid = $json->{data}->[0]->{id};
$content = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$gid/packets?line=false&ts=false&base=ascii")->content;
ok ($content =~ /GET &#47; HTTP&#47;1.1/, "flow G: NULL HTTP request read back");
ok ($content =~ /Host: g.example.com/, "flow G: NULL HTTP host read back");

# The pcapng's files index doc should carry the cached interfaceOffsets array
# (one IDB offset per interface) used to speed up read-back. The synthetic file
# is a fixed SHB (28 bytes) + 6 IDBs (20 bytes each), so the offsets are stable.
$json = esGet("/tests_files/_search?q=name:*multidlt_synthetic.pcapng");
my $file = $json->{hits}->{hits}->[0]->{_source};
ok (defined $file->{interfaceOffsets}, "files doc has interfaceOffsets");
eq_or_diff ($file->{interfaceOffsets}, [28, 48, 68, 88, 108, 128], "interfaceOffsets are the 6 IDB offsets");

# Count pcapng block types in a downloaded buffer. Returns a hashref keyed by
# block_type: 0x0A0D0D0A=SHB, 1=IDB, 6=EPB.
sub countNgBlocks {
    my ($buf) = @_;
    my %counts;
    my $pos = 0;
    my $len = length($buf);
    while ($pos + 8 <= $len) {
        my ($bt, $bl) = unpack("VV", substr($buf, $pos, 8));
        last if ($bl < 12 || $pos + $bl > $len);
        $counts{$bt}++;
        $pos += $bl;
    }
    return \%counts;
}

# Download flow B (cross-DLT DNS: EN10MB iface1 + RAW iface3) as pcapng. The
# exporter must emit one IDB per distinct link type, so this session yields
# exactly 2 IDBs (EN10MB + RAW), one SHB, and EPBs for its packets.
my $pcapng = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/sessions.pcapng?date=-1&expression=" . uri_escape("$files&&host.dns==b.example.com"))->content;
my $blocks = countNgBlocks($pcapng);
is ($blocks->{0x0A0D0D0A}, 1, "flow B pcapng has one SHB");
is ($blocks->{1}, 2, "flow B pcapng has 2 IDBs (EN10MB + RAW)");
ok ($blocks->{6} >= 2, "flow B pcapng has EPB packet blocks");

# Download ALL 8 sessions merged into one pcapng. The 3 distinct link types
# across the file (EN10MB, RAW, NULL) must be merged and de-duplicated into
# exactly 3 IDBs (not one per source interface, not duplicated per session).
$pcapng = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/sessions.pcapng?date=-1&expression=" . uri_escape("$files"))->content;
$blocks = countNgBlocks($pcapng);
is ($blocks->{0x0A0D0D0A}, 1, "merged pcapng has a single SHB");
is ($blocks->{1}, 3, "merged pcapng de-dups to 3 IDBs (EN10MB, RAW, NULL)");
ok ($blocks->{6} >= 8, "merged pcapng has EPB packet blocks for all sessions");

# Download a single session directly via the per-node pcapng route
# (/api/session/:node/:id.pcapng -> getPCAPNGFromNode). This route is distinct
# from the query route above and must not be shadowed by the greedy .pcap route.
# Flow B's DNS query and response travel on two different link types
# (EN10MB iface1 + RAW iface3), so its single-session pcapng has exactly 2 IDBs.
$json = viewerGet("/api/sessions?date=-1&expression=" . uri_escape("$files&&host.dns==b.example.com") . "&fields=id");
my $bid = $json->{data}->[0]->{id};
my $onePcapng = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/session/test/$bid.pcapng")->content;
my $oneBlocks = countNgBlocks($onePcapng);
is ($oneBlocks->{0x0A0D0D0A}, 1, "single-session pcapng (node route) has one SHB");
is ($oneBlocks->{1}, 2, "single-session pcapng (node route) has 2 IDBs (EN10MB + RAW)");
ok ($oneBlocks->{6} >= 2, "single-session pcapng (node route) has EPB packet blocks");

# Payload integrity: the exported EPB packet data must be byte-for-byte the
# captured frame, including its final bytes. Flow A's HTTP request body ends in
# the marker "arkime-multidlt-test\r\n\r\n"; if the exporter drops the trailing
# bytes of each frame the marker's tail is lost, so assert it survives the
# pcapng round-trip intact.
my $aPcapng = $ArkimeTest::userAgent->get("http://$ArkimeTest::host:8123/api/sessions.pcapng?date=-1&expression=" . uri_escape("$files&&host.http==a.example.com"))->content;
ok (index($aPcapng, "arkime-multidlt-test\r\n\r\n") >= 0, "flow A pcapng export preserves full frame bytes (no tail truncation)");
