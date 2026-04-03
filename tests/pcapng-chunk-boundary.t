#!/usr/bin/env perl
#
# SPDX-License-Identifier: Apache-2.0
#
# Test that pcapNG files larger than the scheme reader's chunk size (1MB)
# correctly flush all packets when a chunk boundary falls mid-EPB-header.
#
# Bug: arkime_reader_scheme_processNG() had three code paths that returned 0
# without going through the processNG: label, which skipped batch flushing.
# Packets accumulated in chunk 1's batch were silently lost.

use strict;
use Test::More tests => 2;

my $capture = "../capture/capture";
my $pcap = "pcap/pcapng-chunk-boundary.pcap";

# The test PCAP has 12115 packets across 10 unique flows.
# It is constructed so the 1MB chunk boundary falls 15 bytes into an
# Enhanced Packet Block header, triggering the mid-header return path.
#
# Without the fix: only ~201 packets from chunk 2 are processed.
# With the fix: all 12115 packets are processed.

my $output = `$capture --scheme --dryrun -r $pcap --debug -n test -o pcapDir=/tmp -o pluginsDir=../capture/plugins -o parsersDir=../capture/parsers 2>&1`;

my ($total_packets) = $output =~ /totalPackets:\s+(\d+)/;
my ($total_sessions) = $output =~ /totalSessions:\s+(\d+)/;

is($total_packets, 12115, "scheme reader processes all 12115 packets across chunk boundary");
is($total_sessions, 10, "scheme reader creates correct 10 sessions across chunk boundary");
