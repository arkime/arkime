#!/usr/bin/env python3
"""Append a DNS HTTPS-RR session with an EMPTY alpn SvcParam to
arkime_synthetic.pcap, exercising the dns.c save-path fix where an empty
SVCB value list made the unconditional comma-rewind eat the '=' from
"alpn="/"ipv4hint="/"ipv6hint=" in dns.answers.https.

Session 10.0.91.1:41000 -> 10.0.91.2:53 (UDP):
  pkt1: query svc-empty-alpn.example.com type HTTPS (classify only)
  pkt2: same query retransmitted (parsed)
  pkt3: response with HTTPS RR: priority 1, target ".", SvcParams:
        alpn (key 1, len 0 — EMPTY), port=443, ipv4hint=192.0.2.1

Expected answer string: "HTTPS 1 . alpn= port=443 ipv4hint=192.0.2.1"
(regression: was "HTTPS 1 . alpn port=443 ipv4hint=192.0.2.1")

Idempotent: skips appending if the pcap already has > BASE_PACKETS packets.
"""

import struct

PCAP = 'pcap/arkime_synthetic.pcap'
BASE_PACKETS = 714  # packets before this script's additions
TS_START = 1700009200.0

CMAC = bytes.fromhex('001122334455')
SMAC = bytes.fromhex('66778899aabb')

QNAME = b'svc-empty-alpn' + b'\x07example\x03com\x00'


def csum(data):
    if len(data) & 1:
        data += b'\0'
    s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
    while s >> 16:
        s = (s & 0xffff) + (s >> 16)
    return (~s) & 0xffff


def dns_query():
    hdr = struct.pack('!HHHHHH', 0x9120, 0x0100, 1, 0, 0, 0)
    qname = bytes([14]) + QNAME
    return hdr + qname + struct.pack('!HH', 65, 1)  # type HTTPS, class IN


def dns_response():
    hdr = struct.pack('!HHHHHH', 0x9120, 0x8180, 1, 1, 0, 0)
    qname = bytes([14]) + QNAME
    question = qname + struct.pack('!HH', 65, 1)
    # RDATA: priority 1, target "." (root), then SvcParams
    rdata = struct.pack('!H', 1) + b'\x00'
    rdata += struct.pack('!HH', 1, 0)                          # alpn, EMPTY
    rdata += struct.pack('!HHH', 3, 2, 443)                    # port=443
    rdata += struct.pack('!HH', 4, 4) + bytes([192, 0, 2, 1])  # ipv4hint
    answer = b'\xc0\x0c' + struct.pack('!HHIH', 65, 1, 300, len(rdata)) + rdata
    return hdr + question + answer


def build_udp(src_str, dst_str, sport, dport, payload):
    src = bytes(map(int, src_str.split('.')))
    dst = bytes(map(int, dst_str.split('.')))
    udp_len = 8 + len(payload)
    udp = struct.pack('!HHHH', sport, dport, udp_len, 0)
    pseudo = src + dst + struct.pack('!BBH', 0, 17, udp_len)
    udp = udp[:6] + struct.pack('!H', csum(pseudo + udp + payload))

    total_len = 20 + udp_len
    ip = struct.pack('!BBHHHBBH4s4s', 0x45, 0, total_len, 0x9100, 0, 64, 17,
                     0, src, dst)
    ip = ip[:10] + struct.pack('!H', csum(ip)) + ip[12:]

    eth = SMAC + CMAC + b'\x08\x00'
    return eth + ip + udp + payload


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

    q = dns_query()
    r = dns_response()
    pkts = [
        build_udp('10.0.91.1', '10.0.91.2', 41000, 53, q),
        build_udp('10.0.91.1', '10.0.91.2', 41000, 53, q),  # retransmit, parsed
        build_udp('10.0.91.2', '10.0.91.1', 53, 41000, r),
    ]

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
