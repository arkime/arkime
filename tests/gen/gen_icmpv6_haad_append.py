#!/usr/bin/env python3
"""Append ICMPv6 Home Agent Address Discovery (HAAD, RFC 6275) sessions to
arkime_synthetic.pcap, exercising the type 144/145 community-id port mapping
in capture/db.c arkime_db_community_id_icmp().

Sessions:
  1  2001:db8:90::1 -> 2001:db8:90::2  HAAD Request (144) then Reply (145 back)
     first packet type 144 -> mapped peer 145
  2  2001:db8:90::3 -> 2001:db8:90::4  lone HAAD Reply (145)
     first packet type 145 -> mapped peer 144 (regression: was mapped to 145)

Expected community_ids (verified against the reference `communityid` pkg):
  1: computed from (2001:db8:90::1, 2001:db8:90::2, 58, type 144)
  2: computed from (2001:db8:90::3, 2001:db8:90::4, 58, type 145)

Idempotent: skips appending if the pcap already has > BASE_PACKETS packets.
"""

import socket
import struct

PCAP = 'pcap/arkime_synthetic.pcap'
BASE_PACKETS = 708  # packets before this script's additions
TS_START = 1700009000.0

CMAC = bytes.fromhex('001122334455')
SMAC = bytes.fromhex('66778899aabb')


def csum(data):
    if len(data) & 1:
        data += b'\0'
    s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
    while s >> 16:
        s = (s & 0xffff) + (s >> 16)
    return (~s) & 0xffff


def haad_request(ident):
    # Type(144) Code(0) Checksum(2) Identifier(2) Reserved(2)
    return struct.pack('!BBHHH', 144, 0, 0, ident, 0)


def haad_reply(ident, home_agent):
    # Type(145) Code(0) Checksum(2) Identifier(2) Reserved(2) + HA address(16)
    return struct.pack('!BBHHH', 145, 0, 0, ident, 0) + home_agent


def build_icmpv6(src_str, dst_str, payload):
    src = socket.inet_pton(socket.AF_INET6, src_str)
    dst = socket.inet_pton(socket.AF_INET6, dst_str)
    # IPv6 pseudo-header: src(16) + dst(16) + len(4) + zeros(3) + nxt(1)=58
    pseudo = src + dst + struct.pack('!I', len(payload)) + b'\x00\x00\x00\x3a'
    payload = payload[:2] + struct.pack('!H', csum(pseudo + payload)) + payload[4:]

    ip = struct.pack('!IHBB',
                     (6 << 28),      # version=6, tc=0, flow=0
                     len(payload),
                     58,             # next header = ICMPv6
                     64)             # hop limit
    ip += src + dst

    eth = SMAC + CMAC + b'\x86\xdd'  # linktype 1 = Ethernet
    return eth + ip + payload


def main():
    data = open(PCAP, 'rb').read()
    off = 24
    n = 0
    while off < len(data):
        _, _, cl, _ = struct.unpack('<IIII', data[off:off + 16])
        off += 16 + cl
        n += 1
    if n > BASE_PACKETS:
        print('%s already has %d packets, not appending' % (PCAP, n))
        return

    pkts = []

    # Session 1: request/reply pair
    ha1 = socket.inet_pton(socket.AF_INET6, '2001:db8:90::2')
    pkts.append(build_icmpv6('2001:db8:90::1', '2001:db8:90::2', haad_request(0x1234)))
    pkts.append(build_icmpv6('2001:db8:90::2', '2001:db8:90::1', haad_reply(0x1234, ha1)))

    # Session 2: lone reply, session starts with type 145
    ha2 = socket.inet_pton(socket.AF_INET6, '2001:db8:90::4')
    pkts.append(build_icmpv6('2001:db8:90::3', '2001:db8:90::4', haad_reply(0x5678, ha2)))

    out = b''
    ts = TS_START
    for p in pkts:
        sec = int(ts)
        usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.1

    with open(PCAP, 'ab') as f:
        f.write(out)
    print('appended %d packets to %s' % (len(pkts), PCAP))


if __name__ == '__main__':
    main()
