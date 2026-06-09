#!/usr/bin/env python3
"""Generate flow_synthetic.pcap exercising every flow-export protocol
Arkime's misc.c classifies, over both IPv4 and IPv6 transport.

Sessions (each one UDP datagram):
   1 IPv4  NetFlow v5     src 10.0.0.1     -> 10.0.0.2     dst port 2055
   2 IPv6  NetFlow v5     src 2001:db8::1  -> 2001:db8::2  dst port 2055
   3 IPv4  NetFlow v7     src 10.0.0.3     -> 10.0.0.4     dst port 2055
   4 IPv6  NetFlow v7     src 2001:db8::3  -> 2001:db8::4  dst port 2055
   5 IPv4  NetFlow v9     src 10.0.0.5     -> 10.0.0.6     dst port 2055
   6 IPv6  NetFlow v9     src 2001:db8::5  -> 2001:db8::6  dst port 2055
   7 IPv4  IPFIX (v10)    src 10.0.0.7     -> 10.0.0.8     dst port 4739
   8 IPv6  IPFIX (v10)    src 2001:db8::7  -> 2001:db8::8  dst port 4739
   9 IPv4  sFlow v5       src 10.0.0.9     -> 10.0.0.10    dst port 6343
  10 IPv6  sFlow v5       src 2001:db8::9  -> 2001:db8::a  dst port 6343
"""

import socket
import struct
import sys

EXPORT_TIME = 1700000000  # 2023-11-14, > Sep-2001 sanity check
SYS_UPTIME_MS = 60_000

# ---------------------------------------------------------------------------
# flow protocol payload builders
# ---------------------------------------------------------------------------

def netflow_v5():
    # 24-byte header + one 48-byte record = 72 bytes
    hdr = struct.pack('!HHIIIIBBH',
                      5, 1,          # version, count
                      SYS_UPTIME_MS, # sys_uptime
                      EXPORT_TIME,   # unix_secs
                      0,             # unix_nsecs
                      1,             # flow_sequence
                      0, 0, 0)       # engine_type, engine_id, sampling
    record = struct.pack('!4s4s4sHHIIIIHHBBBBHHBBH',
                         socket.inet_aton('10.1.1.1'),
                         socket.inet_aton('10.2.2.2'),
                         socket.inet_aton('0.0.0.0'),
                         1, 2,           # input, output snmp
                         10,             # dPkts
                         1000,           # dOctets
                         0, SYS_UPTIME_MS,
                         12345, 80,       # src/dst port
                         0, 0x10, 6, 0,   # pad, tcp_flags, proto=TCP, tos
                         0, 0,            # src_as, dst_as
                         24, 24,          # src_mask, dst_mask
                         0)               # pad2
    return hdr + record


def netflow_v7():
    # 24-byte header + one 52-byte v7 record
    hdr = struct.pack('!HHIIIIII',
                      7, 1,
                      SYS_UPTIME_MS,
                      EXPORT_TIME,
                      0,
                      1,             # flow_sequence
                      0, 0)          # reserved
    record = struct.pack('!4s4s4sHHIIIIHHBBBBHHBBHI',
                         socket.inet_aton('10.3.3.3'),
                         socket.inet_aton('10.4.4.4'),
                         socket.inet_aton('0.0.0.0'),
                         1, 2,
                         5,
                         500,
                         0, SYS_UPTIME_MS,
                         54321, 443,
                         0, 0x18, 6, 0,
                         0, 0,
                         24, 24,
                         0, 0)
    return hdr + record


def netflow_v9():
    # 20-byte v9 header + a tiny template/data set
    hdr = struct.pack('!HHIIII',
                      9, 1,           # version, count (flowsets)
                      SYS_UPTIME_MS,
                      EXPORT_TIME,
                      1,              # package_sequence
                      42)             # source_id
    # FlowSet id=0 (template), one template with 3 fields, padded to 4 bytes
    fields = struct.pack('!HHHHHH',
                         8, 4,   # IPV4_SRC_ADDR
                         12, 4,  # IPV4_DST_ADDR
                         2, 4)   # IN_PKTS
    template = struct.pack('!HH', 256, 3) + fields  # template id, field_count
    setlen = 4 + len(template)
    flowset = struct.pack('!HH', 0, setlen) + template
    return hdr + flowset


def ipfix():
    # 16-byte IPFIX header + simple template set
    fields = struct.pack('!HHHHHH',
                         8, 4,    # sourceIPv4Address
                         12, 4,   # destinationIPv4Address
                         2, 4)    # packetDeltaCount
    template = struct.pack('!HH', 256, 3) + fields
    set_data = struct.pack('!HH', 2, 4 + len(template)) + template  # set id=2 (template)
    msg_len = 16 + len(set_data)
    hdr = struct.pack('!HHIII',
                      10,          # version
                      msg_len,
                      EXPORT_TIME,
                      1,           # seq
                      99)          # observation domain
    return hdr + set_data


def sflow(version):
    # 4-byte version + 4-byte agent address type + agent IP +
    # sub_agent_id + sequence_number + sys_uptime + samples_count
    # Use IPv4 agent address (type=1) so classifier accepts it
    agent_addr = socket.inet_aton('10.99.99.99')  # 4 bytes
    body = struct.pack('!IIII',
                       1,             # sub_agent_id
                       1,             # sequence_number
                       SYS_UPTIME_MS, # sys_uptime
                       0)             # samples_count (no samples needed)
    return struct.pack('!II', version, 1) + agent_addr + body


FLOWS = [
    ('netflow_v5', netflow_v5(), 2055),
    ('netflow_v7', netflow_v7(), 2055),
    ('netflow_v9', netflow_v9(), 2055),
    ('ipfix',      ipfix(),      4739),
    ('sflow_v5',   sflow(5),     6343),
]


# ---------------------------------------------------------------------------
# packet helpers
# ---------------------------------------------------------------------------

CMAC = bytes.fromhex('001122334455')
SMAC = bytes.fromhex('66778899aabb')


def ip_cksum(data):
    if len(data) % 2:
        data += b'\x00'
    s = sum(struct.unpack('!%dH' % (len(data) // 2), data))
    while s >> 16:
        s = (s & 0xFFFF) + (s >> 16)
    return ~s & 0xFFFF


def udp_cksum_v4(src, dst, sport, dport, payload):
    udp_len = 8 + len(payload)
    udp = struct.pack('!HHHH', sport, dport, udp_len, 0)
    pseudo = struct.pack('!4s4sBBH', src, dst, 0, 17, udp_len)
    return ip_cksum(pseudo + udp + payload)


def udp_cksum_v6(src, dst, sport, dport, payload):
    udp_len = 8 + len(payload)
    udp = struct.pack('!HHHH', sport, dport, udp_len, 0)
    # IPv6 pseudo-header: src(16) + dst(16) + len(4) + zeros(3) + nxt(1)
    pseudo = src + dst + struct.pack('!I', udp_len) + b'\x00\x00\x00\x11'
    return ip_cksum(pseudo + udp + payload)


def build_udp_v4(src_str, dst_str, sport, dport, payload):
    src = socket.inet_aton(src_str)
    dst = socket.inet_aton(dst_str)
    chk = udp_cksum_v4(src, dst, sport, dport, payload)
    udp = struct.pack('!HHHH', sport, dport, 8 + len(payload), chk)

    total_len = 20 + 8 + len(payload)
    ip = struct.pack('!BBHHHBBH4s4s',
                     0x45, 0,        # ver+ihl, tos
                     total_len,
                     0x1234, 0,      # id, flags+frag
                     64, 17, 0,      # ttl, proto=UDP, checksum
                     src, dst)
    ip = ip[:10] + struct.pack('!H', ip_cksum(ip)) + ip[12:]

    eth = SMAC + CMAC + b'\x08\x00'  # IPv4 ethertype
    return eth + ip + udp + payload


def build_udp_v6(src_str, dst_str, sport, dport, payload):
    src = socket.inet_pton(socket.AF_INET6, src_str)
    dst = socket.inet_pton(socket.AF_INET6, dst_str)
    chk = udp_cksum_v6(src, dst, sport, dport, payload)
    udp = struct.pack('!HHHH', sport, dport, 8 + len(payload), chk)

    payload_len = 8 + len(payload)
    ip = struct.pack('!IHBB',
                     (6 << 28),     # version=6, tc=0, flow=0
                     payload_len,
                     17,            # next header = UDP
                     64)            # hop limit
    ip += src + dst

    eth = SMAC + CMAC + b'\x86\xdd'  # IPv6 ethertype
    return eth + ip + udp + payload


class PcapWriter:
    def __init__(self, path):
        self.f = open(path, 'wb')
        # pcap global header: magic, v2.4, tz=0, sigfigs=0, snaplen=65535,
        # linktype=ETHERNET(1)
        self.f.write(struct.pack('<IHHiIII', 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1))
        self.ts_sec = EXPORT_TIME
        self.ts_usec = 0

    def write(self, data, advance_ms=100):
        self.ts_usec += advance_ms * 1000
        while self.ts_usec >= 1000000:
            self.ts_sec += 1
            self.ts_usec -= 1000000
        self.f.write(struct.pack('<IIII',
                                 self.ts_sec, self.ts_usec,
                                 len(data), len(data)))
        self.f.write(data)

    def close(self):
        self.f.close()


def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/flow_synthetic.pcap'
    pw = PcapWriter(outpath)

    v4_idx = 1
    v6_idx = 1
    sport_base = 40000

    for proto, payload, dport in FLOWS:
        # IPv4 session
        src = f'10.0.0.{v4_idx}'
        dst = f'10.0.0.{v4_idx + 1}'
        pkt = build_udp_v4(src, dst, sport_base, dport, payload)
        pw.write(pkt, advance_ms=1000)
        v4_idx += 2

        # IPv6 session
        src6 = f'2001:db8::{v6_idx:x}'
        dst6 = f'2001:db8::{v6_idx + 1:x}'
        pkt6 = build_udp_v6(src6, dst6, sport_base, dport, payload)
        pw.write(pkt6, advance_ms=1000)
        v6_idx += 2

        sport_base += 1

    pw.close()
    print(f'Created {outpath} ({len(FLOWS) * 2} flow datagrams)')


if __name__ == '__main__':
    main()
