#!/usr/bin/perl
# SPDX-License-Identifier: Apache-2.0
#
# netflow2arkime.pl - Import NetFlow data into Arkime sessions
#
# This script reads NetFlow data and creates Arkime session records in Elasticsearch.
#
# Features:
# - Receives binary NetFlow v5, v9, and IPFIX via UDP (requires Net::Flow)
# - Reads nfdump binary files (nfcapd.*) via nfdump subprocess
# - Reads CSV/text output from NetFlow collectors
# - Combines bidirectional flows into single session records
# - Sends data to Elasticsearch in Arkime sessions3 format
# - Tags records as netflow-originated for identification
# - Supports de-duplication based on 5-tuple + timestamp
#
# Usage:
#   Binary mode (UDP listener):
#     ./netflow2arkime.pl --listen 2055
#
#   nfdump binary files:
#     ./netflow2arkime.pl nfcapd.202402041200 nfcapd.202402041205
#     ./netflow2arkime.pl /var/flows/nfcapd.*
#
#   Text mode (CSV input):
#     ./netflow2arkime.pl --format nfdump < netflow.csv
#     nfdump -r nfcapd.file -o csv | ./netflow2arkime.pl --format nfdump
#
# Dependencies:
#   Core: LWP::UserAgent, JSON, Digest::SHA
#   Binary UDP mode: Net::Flow (cpan install Net::Flow)
#   nfdump file mode: nfdump command in PATH
#

use strict;
use warnings;
use Getopt::Long;
use LWP::UserAgent;
use JSON;
use POSIX qw(strftime);
use Digest::SHA qw(sha1_base64);
use Socket qw(inet_aton inet_ntoa inet_pton AF_INET AF_INET6 pack_sockaddr_in unpack_sockaddr_in);
use Time::Local;
use IO::Select;
use MIME::Base64;

################################################################################
# Configuration defaults
################################################################################
my $ELASTICSEARCH = $ENV{ELASTICSEARCH} || "http://127.0.0.1:9200";
my $PREFIX = $ENV{ARKIME_PREFIX} || "";
my $NODE = $ENV{ARKIME_NODE} || "netflow";
my $TAG = "netflow";
my $DEBUG = 0;
my $DRY_RUN = 0;
my $BATCH_SIZE = 1000;
my $HELP = 0;
my $FORMAT = "";         # nfdump, simple, or empty for auto-detect
my $LISTEN_PORT = 0;     # UDP port for binary mode
my $LISTEN_ADDR = "0.0.0.0";
my $COMBINE_BIDIR = 1;   # Combine bidirectional flows
my $COMBINE_TIMEOUT = 30; # Seconds to wait for reverse flow
my $DEDUP = 0;           # De-duplicate flows
my $EXPORTER = "";       # Exporter identifier
my $NFDUMP_PATH = "nfdump";  # Path to nfdump binary
my $NFDUMP_FILTER = "";  # Optional nfdump filter expression
my @FILES;               # Input files
my $ESUSER = $ENV{ARKIME_default__elasticsearchBasicAuth} || $ENV{ARKIME__elasticsearchBasicAuth} || "";
my $ESAPIKEY = $ENV{ARKIME_default__elasticsearchAPIKey} || $ENV{ARKIME__elasticsearchAPIKey} || "";
my $INSECURE = 0;        # Disable SSL certificate verification

my %seen_flows;          # For de-duplication
my @batch;               # Batch of documents to send
my $total_flows = 0;
my $total_sent = 0;
my $total_skipped = 0;

# For binary NetFlow
my %templates;           # NetFlow v9/IPFIX templates per exporter

################################################################################
# Parse command line options
################################################################################
Getopt::Long::Configure("pass_through");
GetOptions(
    "elasticsearch=s" => \$ELASTICSEARCH,
    "prefix=s"        => \$PREFIX,
    "node=s"          => \$NODE,
    "tag=s"           => \$TAG,
    "format=s"        => \$FORMAT,
    "listen=i"        => \$LISTEN_PORT,
    "bind=s"          => \$LISTEN_ADDR,
    "batch=i"         => \$BATCH_SIZE,
    "exporter=s"      => \$EXPORTER,
    "combine!"        => \$COMBINE_BIDIR,
    "combine-timeout=i" => \$COMBINE_TIMEOUT,
    "dedup!"          => \$DEDUP,
    "nfdump=s"        => \$NFDUMP_PATH,
    "filter=s"        => \$NFDUMP_FILTER,
    "esuser=s"        => \$ESUSER,
    "esapikey=s"      => \$ESAPIKEY,
    "insecure"        => \$INSECURE,
    "debug+"          => \$DEBUG,
    "dry-run"         => \$DRY_RUN,
    "help"            => \$HELP,
) or usage();

# Handle esuser password prompt if needed
if ($ESUSER ne "" && $ESUSER !~ /:/) {
    print STDERR "Enter password for $ESUSER: ";
    system("stty -echo 2>/dev/null");
    my $pass = <STDIN>;
    system("stty echo 2>/dev/null");
    print STDERR "\n";
    chomp($pass);
    $ESUSER .= ":$pass";
}

usage() if $HELP;

# Remaining arguments are input files
@FILES = @ARGV;

# Validate mode
if (!$LISTEN_PORT && !$FORMAT && !@FILES) {
    # Check if stdin has data
    if (-t STDIN) {
        print STDERR "Error: Must specify one of:\n";
        print STDERR "  --listen PORT     for binary UDP mode\n";
        print STDERR "  --format FORMAT   for text mode from stdin\n";
        print STDERR "  <files>           for nfdump binary file mode\n\n";
        usage();
    }
    # Default to nfdump format for piped input
    $FORMAT = "nfdump";
}

################################################################################
# Usage
################################################################################
sub usage {
    print <<EOF;
Usage: $0 [options] [files...]

Import NetFlow data into Arkime sessions.

Modes:
  1. Binary UDP listener:
     $0 --listen 2055 [options]

  2. nfdump binary files (nfcapd.*):
     $0 [options] nfcapd.202402041200 nfcapd.202402041205
     $0 [options] /var/flows/nfcapd.*

  3. Text/CSV from stdin:
     nfdump -r nfcapd.file -o csv | $0 --format nfdump [options]
     $0 --format nfdump [options] < flows.csv

Options:
  --listen PORT         Listen on UDP port for binary NetFlow/IPFIX
  --bind ADDRESS        Bind address for UDP listener (default: 0.0.0.0)
  --format FORMAT       Input format for text mode: nfdump, simple
  --nfdump PATH         Path to nfdump binary (default: nfdump)
  --filter EXPR         Filter expression passed to nfdump (e.g., "proto tcp")
  
  --elasticsearch URL   Elasticsearch URL (default: http://127.0.0.1:9200)
  --esuser USER[:PASS]  ES username and password (prompts if password omitted)
  --esapikey KEY        ES API key (same as elasticsearchAPIKey in config)
  --insecure            Disable SSL certificate verification
  --prefix PREFIX       Arkime index prefix (default: none)
  --node NAME           Node name for sessions (default: netflow)
  --tag TAG             Tag to add to sessions (default: netflow)
  --batch SIZE          Batch size for bulk inserts (default: 1000)
  --exporter NAME       Exporter identifier (for multi-exporter setups)
  
  --combine             Combine bidirectional flows (default: on)
  --no-combine          Don't combine bidirectional flows
  --combine-timeout SEC Seconds to wait for reverse flow (default: 30)
  --dedup               De-duplicate flows based on 5-tuple+time
  
  --debug               Enable debug output (use twice for more)
  --dry-run             Parse but don't send to Elasticsearch
  --help                Show this help

Binary UDP Mode (--listen):
  Supports NetFlow v5, NetFlow v9, and IPFIX (RFC 5101)
  Requires: Net::Flow module (cpan install Net::Flow)
  Templates are learned automatically from incoming packets.

nfdump File Mode (file arguments):
  Reads nfcapd.* files using nfdump subprocess.
  Requires: nfdump installed and in PATH
  Supports glob patterns: /var/flows/nfcapd.*

Text/CSV Mode (--format):
  nfdump   - nfdump -o csv output (ts,te,td,sa,da,sp,dp,pr,flg,...)
  simple   - Simple CSV: start_time,src_ip,src_port,dst_ip,dst_port,proto,pkts,bytes

Environment Variables:
  ELASTICSEARCH                         - Elasticsearch URL
  ARKIME_PREFIX                         - Index prefix
  ARKIME_NODE                           - Node name
  ARKIME__elasticsearchBasicAuth        - ES user:password
  ARKIME__elasticsearchAPIKey           - ES API key

Examples:
  # Listen for NetFlow on UDP 2055
  $0 --listen 2055 --node router1

  # Process nfdump binary files
  $0 --node datacenter /var/flows/nfcapd.202402*

  # Process with filter
  $0 --filter "proto tcp and port 443" nfcapd.*

  # Import from nfdump pipe
  nfdump -r /var/flows/nfcapd.202402041200 -o csv | $0 --format nfdump

  # Dry run to test parsing
  $0 --dry-run --debug nfcapd.202402041200

EOF
    exit(0);
}

################################################################################
# HTTP client setup
################################################################################
my $ua = LWP::UserAgent->new(timeout => 120);

# Configure SSL options
if ($INSECURE) {
    $ua->ssl_opts(
        SSL_verify_mode => 0,
        verify_hostname => 0,
    );
}

# Configure authentication
if ($ESAPIKEY ne "") {
    $ua->default_header('Authorization' => "ApiKey $ESAPIKEY");
} elsif ($ESUSER ne "") {
    my $auth = encode_base64($ESUSER, "");
    $ua->default_header('Authorization' => "Basic $auth");
}

################################################################################
# ES helper functions
################################################################################
sub esPost {
    my ($url, $content) = @_;
    return if $DRY_RUN;
    
    my $response = $ua->post(
        "$ELASTICSEARCH$url",
        Content => $content,
        "Content-Type" => "application/json;charset=UTF-8"
    );
    
    if ($DEBUG > 1) {
        print STDERR "POST $url\n";
        print STDERR "Response: " . $response->content . "\n";
    }
    
    unless ($response->is_success) {
        warn "ES error: " . $response->status_line . "\n";
        warn $response->content . "\n" if $DEBUG;
        return undef;
    }
    
    return decode_json($response->content);
}

sub esBulk {
    my ($docs) = @_;
    return 0 if $DRY_RUN || !@$docs;
    
    my $bulk_data = "";
    for my $doc (@$docs) {
        my $index = $doc->{_index};
        my $source = $doc->{_source};
        $bulk_data .= encode_json({index => {_index => $index}}) . "\n";
        $bulk_data .= encode_json($source) . "\n";
    }
    
    my $response = $ua->post(
        "$ELASTICSEARCH/_bulk",
        Content => $bulk_data,
        "Content-Type" => "application/x-ndjson"
    );
    
    if ($DEBUG) {
        print STDERR "BULK insert " . scalar(@$docs) . " documents\n";
    }
    
    unless ($response->is_success) {
        warn "ES bulk error: " . $response->status_line . "\n";
        return 0;
    }
    
    my $result = decode_json($response->content);
    if ($result->{errors}) {
        warn "ES bulk had errors\n";
        if ($DEBUG) {
            for my $item (@{$result->{items}}) {
                if ($item->{index}{error}) {
                    warn "  Error: " . encode_json($item->{index}{error}) . "\n";
                }
            }
        }
    }
    
    return scalar(@$docs);
}

################################################################################
# Community ID calculation (simplified version 1)
# https://github.com/corelight/community-id-spec
################################################################################
sub calculate_community_id {
    my ($proto, $src_ip, $src_port, $dst_ip, $dst_port) = @_;
    
    # Only calculate community ID for TCP (6) and UDP (17)
    return undef unless ($proto == 6 || $proto == 17);
    
    # Determine if IPv6
    my $is_ipv6 = ($src_ip =~ /:/ || $dst_ip =~ /:/);
    
    # For community ID, we need to normalize direction
    my ($ip1, $port1, $ip2, $port2);
    my ($ip1_bytes, $ip2_bytes);
    
    if ($is_ipv6) {
        my $src_bytes = inet_pton(AF_INET6, $src_ip) || pack("a16", 0);
        my $dst_bytes = inet_pton(AF_INET6, $dst_ip) || pack("a16", 0);
        
        if ($src_bytes lt $dst_bytes || ($src_bytes eq $dst_bytes && $src_port < $dst_port)) {
            ($ip1_bytes, $port1, $ip2_bytes, $port2) = ($src_bytes, $src_port, $dst_bytes, $dst_port);
        } else {
            ($ip1_bytes, $port1, $ip2_bytes, $port2) = ($dst_bytes, $dst_port, $src_bytes, $src_port);
        }
    } else {
        my $src_num = unpack("N", inet_aton($src_ip) || pack("N", 0));
        my $dst_num = unpack("N", inet_aton($dst_ip) || pack("N", 0));
        
        if ($src_num < $dst_num || ($src_num == $dst_num && $src_port < $dst_port)) {
            ($ip1, $port1, $ip2, $port2) = ($src_ip, $src_port, $dst_ip, $dst_port);
        } else {
            ($ip1, $port1, $ip2, $port2) = ($dst_ip, $dst_port, $src_ip, $src_port);
        }
        $ip1_bytes = inet_aton($ip1) || pack("N", 0);
        $ip2_bytes = inet_aton($ip2) || pack("N", 0);
    }
    
    # Build the hash input
    my $seed = pack("n", 0);  # seed = 0
    my $proto_byte = pack("C", $proto);
    my $pad = pack("C", 0);
    my $port1_bytes = pack("n", $port1);
    my $port2_bytes = pack("n", $port2);
    
    my $data = $seed . $ip1_bytes . $ip2_bytes . $proto_byte . $pad . $port1_bytes . $port2_bytes;
    my $hash = sha1_base64($data);
    
    return "1:$hash=";
}

################################################################################
# Protocol number to name
################################################################################
sub protocol_name {
    my ($proto) = @_;
    return "icmp"   if $proto == 1;
    return "tcp"    if $proto == 6;
    return "udp"    if $proto == 17;
    return "gre"    if $proto == 47;
    return "esp"    if $proto == 50;
    return "icmp6"  if $proto == 58;
    return "sctp"   if $proto == 132;
    return "proto$proto";
}

################################################################################
# Generate Arkime session document
################################################################################
sub create_session {
    my ($flow) = @_;
    
    my $firstPacket = $flow->{start_time} * 1000;  # Convert to milliseconds
    my $lastPacket = $flow->{end_time} * 1000;
    my $length = $lastPacket - $firstPacket;
    $length = 1 if $length < 1;
    
    my @tags = ($TAG);
    push @tags, "exporter-$EXPORTER" if $EXPORTER;
    push @tags, "exporter-$flow->{exporter}" if $flow->{exporter} && !$EXPORTER;
    
    my $community_id = calculate_community_id(
        $flow->{protocol},
        $flow->{src_ip},
        $flow->{src_port},
        $flow->{dst_ip},
        $flow->{dst_port}
    );
    
    my $session = {
        "firstPacket"      => int($firstPacket),
        "lastPacket"       => int($lastPacket),
        "length"           => int($length),
        "source.ip"        => $flow->{src_ip},
        "source.port"      => int($flow->{src_port}),
        "destination.ip"   => $flow->{dst_ip},
        "destination.port" => int($flow->{dst_port}),
        "ipProtocol"       => int($flow->{protocol}),
        "node"             => $flow->{node} || $NODE,
        "segmentCnt"       => 1,
        "srcPackets"       => int($flow->{src_packets} || 0),
        "srcBytes"         => int($flow->{src_bytes} || 0),
        "dstPackets"       => int($flow->{dst_packets} || 0),
        "dstBytes"         => int($flow->{dst_bytes} || 0),
        "network.packets"  => int(($flow->{src_packets} || 0) + ($flow->{dst_packets} || 0)),
        "network.bytes"    => int(($flow->{src_bytes} || 0) + ($flow->{dst_bytes} || 0)),
        "tags"             => \@tags,
        "tagsCnt"          => scalar(@tags),
        "protocol"         => [protocol_name($flow->{protocol})],
        "protocolCnt"      => 1,
        "fileId"           => [],
        "packetPos"        => [],
        "packetLen"        => [],
    };
    
    # Add community ID only for TCP/UDP
    if (defined $community_id) {
        $session->{communityId} = $community_id;
    }
    
    # Add TCP flags if present
    if ($flow->{protocol} == 6 && defined $flow->{tcp_flags}) {
        $session->{tcpflags} = parse_tcp_flags($flow->{tcp_flags});
    }
    
    return $session;
}

################################################################################
# Parse TCP flags string to Arkime format
################################################################################
sub parse_tcp_flags {
    my ($flags_str) = @_;
    
    my $flags = {
        "syn"     => 0,
        "syn-ack" => 0,
        "ack"     => 0,
        "psh"     => 0,
        "fin"     => 0,
        "rst"     => 0,
        "urg"     => 0,
        "srcZero" => 0,
        "dstZero" => 0,
    };
    
    # Handle various flag formats
    if (ref($flags_str) eq '') {
        if ($flags_str =~ /^[\.FSRPAUECN]+$/i) {
            # nfdump style: .AP.SF
            $flags->{syn} = 1 if $flags_str =~ /S/i;
            $flags->{ack} = 1 if $flags_str =~ /A/i;
            $flags->{psh} = 1 if $flags_str =~ /P/i;
            $flags->{fin} = 1 if $flags_str =~ /F/i;
            $flags->{rst} = 1 if $flags_str =~ /R/i;
            $flags->{urg} = 1 if $flags_str =~ /U/i;
        } elsif ($flags_str =~ /^\d+$/) {
            # Numeric flags
            my $f = int($flags_str);
            $flags->{fin} = 1 if $f & 0x01;
            $flags->{syn} = 1 if $f & 0x02;
            $flags->{rst} = 1 if $f & 0x04;
            $flags->{psh} = 1 if $f & 0x08;
            $flags->{ack} = 1 if $f & 0x10;
            $flags->{urg} = 1 if $f & 0x20;
        }
    }
    
    return $flags;
}

################################################################################
# Parse timestamp to epoch seconds
################################################################################
sub parse_timestamp {
    my ($ts) = @_;
    
    # Already epoch?
    return $ts if $ts =~ /^\d+(\.\d+)?$/;
    
    # ISO format: 2024-02-04T15:30:00
    if ($ts =~ /^(\d{4})-(\d{2})-(\d{2})[T ](\d{2}):(\d{2}):(\d{2})/) {
        return timegm($6, $5, $4, $3, $2-1, $1);
    }
    
    # nfdump format: 2024-02-04 15:30:00.123
    if ($ts =~ /^(\d{4})-(\d{2})-(\d{2}) (\d{2}):(\d{2}):(\d{2})/) {
        return timegm($6, $5, $4, $3, $2-1, $1);
    }
    
    warn "Unknown timestamp format: $ts\n" if $DEBUG;
    return time();
}

################################################################################
# Convert protocol string to number
################################################################################
sub protocol_to_num {
    my ($proto) = @_;
    return int($proto) if $proto =~ /^\d+$/;
    return 1   if $proto =~ /^icmp$/i;
    return 6   if $proto =~ /^tcp$/i;
    return 17  if $proto =~ /^udp$/i;
    return 47  if $proto =~ /^gre$/i;
    return 50  if $proto =~ /^esp$/i;
    return 58  if $proto =~ /^icmp6$/i;
    return 132 if $proto =~ /^sctp$/i;
    return 0;
}

################################################################################
# Parse nfdump CSV format
# ts,te,td,sa,da,sp,dp,pr,flg,fwd,stos,ipkt,ibyt,opkt,obyt,...
################################################################################
sub parse_nfdump {
    my ($line) = @_;
    
    my @fields = split(/,/, $line);
    return undef if @fields < 13;
    
    my ($ts, $te, $td, $sa, $da, $sp, $dp, $pr, $flg, $fwd, $stos, $ipkt, $ibyt) = @fields;
    
    # Skip header line
    return undef if $ts eq 'ts' || $ts =~ /^#/;
    
    my $start = parse_timestamp($ts);
    my $end = parse_timestamp($te);
    $end = $start + 1 if $end <= $start;
    
    return {
        start_time  => $start,
        end_time    => $end,
        src_ip      => $sa,
        dst_ip      => $da,
        src_port    => int($sp || 0),
        dst_port    => int($dp || 0),
        protocol    => protocol_to_num($pr),
        tcp_flags   => $flg,
        src_packets => int($ipkt || 0),
        src_bytes   => int($ibyt || 0),
        dst_packets => 0,
        dst_bytes   => 0,
    };
}

################################################################################
# Parse simple CSV format
# start_time,src_ip,src_port,dst_ip,dst_port,protocol,packets,bytes
################################################################################
sub parse_simple {
    my ($line) = @_;
    
    my @fields = split(/,/, $line);
    return undef if @fields < 8;
    
    my ($ts, $sa, $sp, $da, $dp, $pr, $pkts, $bytes) = @fields;
    
    # Skip header line
    return undef if $ts =~ /^(start|time|#)/i;
    
    my $start = parse_timestamp($ts);
    
    return {
        start_time  => $start,
        end_time    => $start + 1,
        src_ip      => $sa,
        dst_ip      => $da,
        src_port    => int($sp || 0),
        dst_port    => int($dp || 0),
        protocol    => protocol_to_num($pr),
        src_packets => int($pkts || 0),
        src_bytes   => int($bytes || 0),
        dst_packets => 0,
        dst_bytes   => 0,
    };
}

################################################################################
# Generate flow key for combining/dedup
################################################################################
sub flow_key {
    my ($flow, $include_time) = @_;
    
    my @parts = sort ($flow->{src_ip}, $flow->{dst_ip});
    my @ports = sort {$a <=> $b} ($flow->{src_port}, $flow->{dst_port});
    
    my $key = join("|", @parts, @ports, $flow->{protocol});
    
    if ($include_time) {
        # Round to nearest minute for dedup
        my $rounded_time = int($flow->{start_time} / 60) * 60;
        $key .= "|$rounded_time";
    }
    
    return $key;
}

################################################################################
# Check if flow is reverse direction of another
################################################################################
sub is_reverse_flow {
    my ($flow1, $flow2) = @_;
    
    return (
        $flow1->{src_ip} eq $flow2->{dst_ip} &&
        $flow1->{dst_ip} eq $flow2->{src_ip} &&
        $flow1->{src_port} == $flow2->{dst_port} &&
        $flow1->{dst_port} == $flow2->{src_port} &&
        $flow1->{protocol} == $flow2->{protocol}
    );
}

################################################################################
# Combine bidirectional flows
################################################################################
my %pending_flows;
my %pending_times;

sub combine_or_queue {
    my ($flow) = @_;
    
    my $key = flow_key($flow, 0);
    
    if (exists $pending_flows{$key}) {
        my $other = $pending_flows{$key};
        
        # Check if this is the reverse direction
        if (is_reverse_flow($flow, $other)) {
            # Combine: other is src->dst, flow is dst->src
            $other->{dst_packets} = $flow->{src_packets};
            $other->{dst_bytes} = $flow->{src_bytes};
            $other->{end_time} = $flow->{end_time} if $flow->{end_time} > $other->{end_time};
            
            delete $pending_flows{$key};
            delete $pending_times{$key};
            return $other;
        }
    }
    
    # Queue this flow and wait for reverse
    $pending_flows{$key} = $flow;
    $pending_times{$key} = time();
    return undef;
}

sub flush_expired_pending {
    my $now = time();
    my @expired;
    
    for my $key (keys %pending_times) {
        if ($now - $pending_times{$key} > $COMBINE_TIMEOUT) {
            push @expired, $pending_flows{$key};
            delete $pending_flows{$key};
            delete $pending_times{$key};
        }
    }
    
    return @expired;
}

sub flush_all_pending {
    my @flows;
    for my $flow (values %pending_flows) {
        push @flows, $flow;
    }
    %pending_flows = ();
    %pending_times = ();
    return @flows;
}

################################################################################
# Calculate index name from timestamp
################################################################################
sub get_index_name {
    my ($timestamp_ms) = @_;
    
    my @t = gmtime($timestamp_ms / 1000);
    my $date = sprintf("%02d%02d%02d", $t[5] % 100, $t[4] + 1, $t[3]);
    
    return "${PREFIX}sessions3-$date";
}

################################################################################
# Add flow to batch and send if full
################################################################################
sub queue_flow {
    my ($flow) = @_;
    
    my $session = create_session($flow);
    my $index = get_index_name($session->{firstPacket});
    
    if ($DEBUG > 1) {
        print STDERR "Flow: $flow->{src_ip}:$flow->{src_port} -> $flow->{dst_ip}:$flow->{dst_port} " .
                     "proto=$flow->{protocol} pkts=$session->{'network.packets'} bytes=$session->{'network.bytes'}\n";
    }
    
    push @batch, {
        _index  => $index,
        _source => $session,
    };
    
    if (@batch >= $BATCH_SIZE) {
        my $sent = esBulk(\@batch);
        $total_sent += $sent if $sent;
        @batch = ();
    }
}

################################################################################
# Process a single flow record
################################################################################
sub process_flow {
    my ($flow) = @_;
    
    return unless $flow;
    
    $total_flows++;
    
    # De-duplication check
    if ($DEDUP) {
        my $dedup_key = flow_key($flow, 1);
        if (exists $seen_flows{$dedup_key}) {
            $total_skipped++;
            print STDERR "Skipping duplicate: $dedup_key\n" if $DEBUG > 1;
            return;
        }
        $seen_flows{$dedup_key} = 1;
    }
    
    # Combine bidirectional flows
    if ($COMBINE_BIDIR) {
        my $combined = combine_or_queue($flow);
        queue_flow($combined) if $combined;
        
        # Flush expired pending flows
        for my $expired (flush_expired_pending()) {
            queue_flow($expired);
        }
    } else {
        queue_flow($flow);
    }
    
    # Print progress
    if ($DEBUG && $total_flows % 10000 == 0) {
        print STDERR "Processed $total_flows flows, sent $total_sent...\n";
    }
}

################################################################################
# Process text input from filehandle
################################################################################
sub process_text_input {
    my ($fh, $parser) = @_;
    
    while (my $line = <$fh>) {
        chomp $line;
        next if $line =~ /^\s*$/;
        next if $line =~ /^#/;
        
        my $flow = $parser->($line);
        process_flow($flow);
    }
}

################################################################################
# Binary NetFlow/IPFIX processing (UDP)
################################################################################
sub process_binary_netflow {
    # Try to load Net::Flow
    eval { require Net::Flow; };
    if ($@) {
        die "Error: Net::Flow module required for binary UDP mode.\n" .
            "Install with: cpan install Net::Flow\n\n$@\n";
    }
    
    require IO::Socket::INET;
    
    # Create UDP socket
    my $sock = IO::Socket::INET->new(
        LocalAddr => $LISTEN_ADDR,
        LocalPort => $LISTEN_PORT,
        Proto     => 'udp',
    ) or die "Cannot create UDP socket on $LISTEN_ADDR:$LISTEN_PORT: $!\n";
    
    print STDERR "Listening for NetFlow/IPFIX on $LISTEN_ADDR:$LISTEN_PORT\n";
    
    my $select = IO::Select->new($sock);
    
    while (1) {
        # Wait for data with timeout (to flush expired flows)
        my @ready = $select->can_read(5);
        
        # Flush expired pending flows periodically
        if ($COMBINE_BIDIR) {
            for my $expired (flush_expired_pending()) {
                queue_flow($expired);
            }
        }
        
        # Flush batch periodically even if not full
        if (@batch > 0) {
            my $sent = esBulk(\@batch);
            $total_sent += $sent if $sent;
            @batch = ();
        }
        
        next unless @ready;
        
        my $packet;
        my $sender = $sock->recv($packet, 0xFFFF);
        next unless defined $sender;
        
        my ($sender_port, $sender_addr) = unpack_sockaddr_in($sender);
        my $sender_ip = inet_ntoa($sender_addr);
        
        if ($DEBUG) {
            print STDERR "Received " . length($packet) . " bytes from $sender_ip:$sender_port\n";
        }
        
        # Determine stream ID for template storage
        my ($version, $obs_domain, $source_id) = unpack('nx10N2', $packet);
        my $stream_id;
        if ($version == 9) {
            $stream_id = "$sender_ip:$sender_port:$source_id";
        } elsif ($version == 10) {  # IPFIX
            $stream_id = "$sender_ip:$sender_port:$obs_domain";
        } else {
            $stream_id = "$sender_ip:$sender_port";
        }
        
        $templates{$stream_id} ||= [];
        
        # Decode the NetFlow/IPFIX packet
        my ($HeaderHashRef, $TemplateArrayRef, $FlowArrayRef, $ErrorsArrayRef) =
            Net::Flow::decode(\$packet, $templates{$stream_id});
        
        # Update templates
        $templates{$stream_id} = $TemplateArrayRef;
        
        # Log errors
        if (@$ErrorsArrayRef) {
            warn "NetFlow decode errors from $sender_ip: " . join(", ", @$ErrorsArrayRef) . "\n" if $DEBUG;
        }
        
        # Process flow records
        for my $FlowRef (@$FlowArrayRef) {
            my $flow = decode_binary_flow($FlowRef, $HeaderHashRef, $sender_ip);
            process_flow($flow) if $flow;
        }
    }
}

################################################################################
# Decode a binary flow record from Net::Flow
# NetFlow/IPFIX Information Element IDs:
#   1  = octetDeltaCount
#   2  = packetDeltaCount
#   4  = protocolIdentifier
#   6  = tcpControlBits
#   7  = sourceTransportPort
#   8  = sourceIPv4Address
#   10 = ingressInterface
#   11 = destinationTransportPort
#   12 = destinationIPv4Address
#   21 = flowEndSysUpTime
#   22 = flowStartSysUpTime
#  150 = flowStartSeconds
#  151 = flowEndSeconds
#  152 = flowStartMilliseconds
#  153 = flowEndMilliseconds
################################################################################
sub decode_binary_flow {
    my ($FlowRef, $HeaderHashRef, $sender_ip) = @_;
    
    # Skip template records
    return undef unless exists $FlowRef->{SetId} && $FlowRef->{SetId} >= 256;
    
    # Extract IP addresses
    my $src_ip = "";
    my $dst_ip = "";
    
    if (exists $FlowRef->{8}) {
        # sourceIPv4Address
        $src_ip = inet_ntoa($FlowRef->{8});
    } elsif (exists $FlowRef->{27}) {
        # sourceIPv6Address - convert to string
        $src_ip = unpack("H*", $FlowRef->{27});
        # Format as IPv6
        $src_ip =~ s/(.{4})/$1:/g;
        $src_ip =~ s/:$//;
    }
    
    if (exists $FlowRef->{12}) {
        # destinationIPv4Address
        $dst_ip = inet_ntoa($FlowRef->{12});
    } elsif (exists $FlowRef->{28}) {
        # destinationIPv6Address
        $dst_ip = unpack("H*", $FlowRef->{28});
        $dst_ip =~ s/(.{4})/$1:/g;
        $dst_ip =~ s/:$//;
    }
    
    return undef unless $src_ip && $dst_ip;
    
    # Extract ports
    my $src_port = 0;
    my $dst_port = 0;
    
    if (exists $FlowRef->{7}) {
        $src_port = unpack("n", $FlowRef->{7});
    }
    if (exists $FlowRef->{11}) {
        $dst_port = unpack("n", $FlowRef->{11});
    }
    
    # Extract protocol
    my $protocol = 0;
    if (exists $FlowRef->{4}) {
        $protocol = unpack("C", $FlowRef->{4});
    }
    
    # Extract byte/packet counts
    my $bytes = 0;
    my $packets = 0;
    
    if (exists $FlowRef->{1}) {
        # octetDeltaCount - can be 4 or 8 bytes
        my $len = length($FlowRef->{1});
        if ($len == 4) {
            $bytes = unpack("N", $FlowRef->{1});
        } elsif ($len == 8) {
            my ($hi, $lo) = unpack("NN", $FlowRef->{1});
            $bytes = ($hi << 32) | $lo;
        }
    }
    
    if (exists $FlowRef->{2}) {
        # packetDeltaCount
        my $len = length($FlowRef->{2});
        if ($len == 4) {
            $packets = unpack("N", $FlowRef->{2});
        } elsif ($len == 8) {
            my ($hi, $lo) = unpack("NN", $FlowRef->{2});
            $packets = ($hi << 32) | $lo;
        }
    }
    
    # Extract timestamps
    my $start_time = 0;
    my $end_time = 0;
    my $version = $HeaderHashRef->{VersionNum} || 0;
    
    if ($version == 5) {
        # NetFlow v5: use header timestamps
        $start_time = $HeaderHashRef->{UnixSecs} || time();
        $end_time = $start_time;
    } elsif (exists $FlowRef->{150}) {
        # flowStartSeconds
        $start_time = unpack("N", $FlowRef->{150});
    } elsif (exists $FlowRef->{152}) {
        # flowStartMilliseconds
        my ($hi, $lo) = unpack("NN", $FlowRef->{152});
        $start_time = (($hi << 32) | $lo) / 1000;
    } elsif (exists $FlowRef->{22} && $HeaderHashRef->{SysUpTime}) {
        # flowStartSysUpTime - relative to SysUpTime
        my $uptime_ms = unpack("N", $FlowRef->{22});
        $start_time = ($HeaderHashRef->{UnixSecs} || time()) - 
                      (($HeaderHashRef->{SysUpTime} - $uptime_ms) / 1000);
    } else {
        $start_time = $HeaderHashRef->{UnixSecs} || time();
    }
    
    if (exists $FlowRef->{151}) {
        # flowEndSeconds
        $end_time = unpack("N", $FlowRef->{151});
    } elsif (exists $FlowRef->{153}) {
        # flowEndMilliseconds
        my ($hi, $lo) = unpack("NN", $FlowRef->{153});
        $end_time = (($hi << 32) | $lo) / 1000;
    } elsif (exists $FlowRef->{21} && $HeaderHashRef->{SysUpTime}) {
        # flowEndSysUpTime
        my $uptime_ms = unpack("N", $FlowRef->{21});
        $end_time = ($HeaderHashRef->{UnixSecs} || time()) - 
                    (($HeaderHashRef->{SysUpTime} - $uptime_ms) / 1000);
    } else {
        $end_time = $start_time + 1;
    }
    
    $end_time = $start_time + 1 if $end_time <= $start_time;
    
    # Extract TCP flags
    my $tcp_flags = undef;
    if ($protocol == 6 && exists $FlowRef->{6}) {
        $tcp_flags = unpack("C", $FlowRef->{6});
    }
    
    return {
        start_time  => $start_time,
        end_time    => $end_time,
        src_ip      => $src_ip,
        dst_ip      => $dst_ip,
        src_port    => $src_port,
        dst_port    => $dst_port,
        protocol    => $protocol,
        tcp_flags   => $tcp_flags,
        src_packets => $packets,
        src_bytes   => $bytes,
        dst_packets => 0,
        dst_bytes   => 0,
        exporter    => $sender_ip,
    };
}

################################################################################
# Process nfdump binary files using nfdump subprocess
################################################################################
sub process_nfdump_files {
    my @files = @_;
    
    # Check if nfdump is available
    my $nfdump_version = `$NFDUMP_PATH -V 2>&1`;
    if ($? != 0) {
        die "Error: nfdump not found at '$NFDUMP_PATH'.\n" .
            "Install nfdump or specify path with --nfdump /path/to/nfdump\n";
    }
    
    if ($DEBUG) {
        chomp($nfdump_version);
        print STDERR "Using $nfdump_version\n";
    }
    
    for my $file (@files) {
        print STDERR "Processing file: $file\n" if $DEBUG;
        
        unless (-r $file) {
            warn "Cannot read file: $file\n";
            next;
        }
        
        # Build nfdump command
        my @cmd = ($NFDUMP_PATH, '-r', $file, '-o', 'csv', '-q');
        push @cmd, $NFDUMP_FILTER if $NFDUMP_FILTER;
        
        if ($DEBUG) {
            print STDERR "Running: " . join(' ', @cmd) . "\n";
        }
        
        # Open pipe to nfdump
        open(my $fh, '-|', @cmd) or do {
            warn "Failed to run nfdump on $file: $!\n";
            next;
        };
        
        process_text_input($fh, \&parse_nfdump);
        
        close($fh);
        
        if ($? != 0) {
            warn "nfdump exited with error for $file\n" if $DEBUG;
        }
    }
}

################################################################################
# Text mode processing from stdin
################################################################################
sub process_text_mode {
    print STDERR "NetFlow to Arkime Importer (text mode)\n" if $DEBUG;
    print STDERR "Elasticsearch: $ELASTICSEARCH\n" if $DEBUG;
    print STDERR "Node: $NODE, Tag: $TAG, Format: $FORMAT\n" if $DEBUG;
    print STDERR "Combine bidirectional: " . ($COMBINE_BIDIR ? "yes" : "no") . "\n" if $DEBUG;
    print STDERR "De-duplication: " . ($DEDUP ? "yes" : "no") . "\n" if $DEBUG;
    
    my $parser = $FORMAT eq "simple" ? \&parse_simple : \&parse_nfdump;
    
    process_text_input(\*STDIN, $parser);
}

################################################################################
# Main
################################################################################

print STDERR "NetFlow to Arkime Importer\n" if $DEBUG;
print STDERR "Elasticsearch: $ELASTICSEARCH\n" if $DEBUG;
print STDERR "Prefix: '$PREFIX', Node: $NODE, Tag: $TAG\n" if $DEBUG;
print STDERR "Combine: " . ($COMBINE_BIDIR ? "yes" : "no") . ", Dedup: " . ($DEDUP ? "yes" : "no") . "\n" if $DEBUG;
print STDERR "Dry run: " . ($DRY_RUN ? "yes" : "no") . "\n" if $DEBUG;

if ($LISTEN_PORT) {
    # Binary UDP mode
    process_binary_netflow();
} elsif (@FILES) {
    # nfdump file mode
    process_nfdump_files(@FILES);
} else {
    # Text mode from stdin
    process_text_mode();
}

# Flush any remaining pending flows
if ($COMBINE_BIDIR) {
    for my $flow (flush_all_pending()) {
        queue_flow($flow);
    }
}

# Send any remaining batch
if (@batch) {
    my $sent = esBulk(\@batch);
    $total_sent += $sent if $sent;
}

# Summary
print STDERR "\nSummary:\n";
print STDERR "  Total flows read: $total_flows\n";
print STDERR "  Sessions sent to ES: $total_sent\n";
print STDERR "  Flows skipped (dedup): $total_skipped\n" if $DEDUP && $total_skipped;
print STDERR "  Pending unmatched: " . scalar(keys %pending_flows) . "\n" if $COMBINE_BIDIR && %pending_flows;

exit(0);
