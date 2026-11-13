#!/usr/bin/env python3
# Copyright Andy Wick
# SPDX-License-Identifier: Apache-2.0
"""Regenerate tests/pcap/ip-frags_synthetic.pcap in full.

Layout, one UDP/53 DNS query per case, each from its own source ip,
the qname is <case>.frag.test:
  - 10.77.0.1-11 ip4: in order, reverse, duplicate first, duplicate middle,
    overlap, hole (never completes), 50, 51, 128 and 129 (over the limit) fragments,
    overlap with different bytes (dropped)
  - fd77::1-6 ip6: atomic fragment, in order, reverse, duplicate,
    destination options before the fragment header, hole (never completes)

Run from the tests directory:  python3 gen/gen_ip-frags_synthetic.py
"""

import struct
import sys

CMAC = bytes.fromhex('020000007701')
SMAC = bytes.fromhex('020000007702')


def csum(data):
    if len(data) & 1:
        data += b'\0'
    s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
    while s >> 16:
        s = (s & 0xffff) + (s >> 16)
    return (~s) & 0xffff


def ip6addr(n):
    # fd77::n
    return bytes.fromhex('fd77' + '00' * 12) + struct.pack('>H', n)


def dns_query(name, pad=0):
    qname = b''.join(bytes([len(p)]) + p for p in name.encode().split(b'.')) + b'\0'
    dns = struct.pack('>HHHHHH', 0x1234, 0x0100, 1, 0, 0, 1 if pad else 0) + qname + struct.pack('>HH', 1, 1)
    if pad:
        # EDNS0 OPT with a padding option
        dns += b'\0' + struct.pack('>HHIH', 41, 4096, 0, 4 + pad) + struct.pack('>HH', 12, pad) + b'\0' * pad
    return dns


def udp(src, dst, payload, v6):
    hdr = struct.pack('>HHHH', 40000, 53, 8 + len(payload), 0)
    if v6:
        pseudo = src + dst + struct.pack('>IxxxB', 8 + len(payload), 17)
    else:
        pseudo = src + dst + struct.pack('>BBH', 0, 17, 8 + len(payload))
    c = csum(pseudo + hdr + payload) or 0xffff
    return hdr[:6] + struct.pack('>H', c) + payload


def v4frag(src, dst, ipid, off, data, more):
    ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, 20 + len(data), ipid, (0x2000 if more else 0) | (off // 8), 64, 17, 0, src, dst)
    ip = ip[:10] + struct.pack('>H', csum(ip)) + ip[12:]
    return SMAC + CMAC + b'\x08\x00' + ip + data


def v6frag(src, dst, fid, off, data, more, destopt=False):
    frag = struct.pack('>BBHI', 17, 0, (off // 8) << 3 | (1 if more else 0), fid)
    ext = (struct.pack('>BB', 44, 0) + b'\x01\x04\0\0\0\0' if destopt else b'') + frag
    ip = struct.pack('>IHBB', 0x60000000, len(ext) + len(data), 60 if destopt else 44, 64) + src + dst
    return SMAC + CMAC + b'\x86\xdd' + ip + ext + data


def chunks(data, size):
    return [(off, data[off:off + size]) for off in range(0, len(data), size)]


def v4case(n, case, size=16, nfrags=None, order=None, dup=None, overlap=False, drop=None):
    src, dst = bytes([10, 77, 0, n]), bytes([10, 77, 1, 1])
    name = case + '.frag.test'
    if nfrags:
        # Pad so the udp datagram is exactly nfrags 8 byte fragments
        base = 8 + len(dns_query(name, 1)) - 1
        data = udp(src, dst, dns_query(name, nfrags * 8 - base), False)
        assert len(data) == nfrags * 8
    else:
        data = udp(src, dst, dns_query(name), False)
    cs = chunks(data, size)
    frags = [v4frag(src, dst, 0x4000 + n, off, c, off + len(c) < len(data)) for off, c in cs]
    if overlap:
        # second fragment restarts 8 bytes early with the same bytes, or different ones for 'diff'
        off = cs[1][0] - 8
        body = data[off:cs[1][0] + len(cs[1][1])]
        if overlap == 'diff':
            body = bytes(b ^ 0xff for b in body[:8]) + body[8:]
        frags[1] = v4frag(src, dst, 0x4000 + n, off, body, True)
    if drop is not None:
        del frags[drop]
    if dup is not None:
        frags.insert(dup + 1, frags[dup])
    return order(frags) if order else frags


def v6case(n, case, size=16, order=None, dup=None, drop=None, destopt=False, atomic=False):
    src, dst = ip6addr(n), bytes.fromhex('fd77' + '00' * 11) + b'\x01\x00\x01'
    data = udp(src, dst, dns_query(case + '.frag.test'), True)
    if atomic:
        return [v6frag(src, dst, 0x6000 + n, 0, data, False)]
    frags = [v6frag(src, dst, 0x6000 + n, off, c, off + len(c) < len(data), destopt) for off, c in chunks(data, size)]
    if drop is not None:
        del frags[drop]
    if dup is not None:
        frags.insert(dup + 1, frags[dup])
    return order(frags) if order else frags


def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/ip-frags_synthetic.pcap'

    def rev(fs):
        return list(reversed(fs))

    pkts = []
    pkts += v4case(1, 'v4-inorder')
    pkts += v4case(2, 'v4-reverse', order=rev)
    pkts += v4case(3, 'v4-dup-first', dup=0)
    pkts += v4case(4, 'v4-dup-middle', dup=1)
    pkts += v4case(5, 'v4-overlap', overlap=True)
    pkts += v4case(6, 'v4-hole', drop=1)
    pkts += v4case(7, 'v4-50', size=8, nfrags=50)
    pkts += v4case(8, 'v4-51', size=8, nfrags=51)
    pkts += v4case(9, 'v4-128', size=8, nfrags=128)
    pkts += v4case(10, 'v4-129', size=8, nfrags=129)
    pkts += v4case(11, 'v4-overlap-diff', overlap='diff')
    pkts += v6case(1, 'v6-atomic', atomic=True)
    pkts += v6case(2, 'v6-inorder')
    pkts += v6case(3, 'v6-reverse', order=rev)
    pkts += v6case(4, 'v6-dup', dup=1)
    pkts += v6case(5, 'v6-destopt', destopt=True)
    pkts += v6case(6, 'v6-hole', drop=1)

    out = struct.pack('<IHHiIII', 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1)
    ts = 1700000000.0
    for p in pkts:
        sec = int(ts)
        usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.001
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
