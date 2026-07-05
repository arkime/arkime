#!/usr/bin/env python3
"""Generate python-packet-cb.pcap for tests/python-packet-cb.t

4 ethernet frames with experimental ethertype 0x88B5, each carrying a
complete IPv4/UDP datagram (same flow, different payloads).

Without the python plugin the frames are unknown-ether (no sessions).
tests/plugins/pythonpacketcb.py registers an ethernet cb for 0x88B5 that
hands the payload to the real IPv4 handler and returns None (a non-int)
for the first packet, exercising the PyLong_Check fix in
capture/python.c:arkime_python_packet_cb. All 4 packets must end up in
the session.
"""

import struct
import sys


def ip_checksum(hdr):
    s = 0
    for i in range(0, len(hdr), 2):
        s += (hdr[i] << 8) | hdr[i + 1]
    while s > 0xffff:
        s = (s & 0xffff) + (s >> 16)
    return (~s) & 0xffff


def frame(payload_str, ident):
    udp = struct.pack('>HHHH', 12345, 54321, 8 + len(payload_str), 0) + payload_str
    totlen = 20 + len(udp)
    iph = struct.pack('>BBHHHBBH4s4s', 0x45, 0, totlen, ident, 0, 64, 17, 0,
                      bytes([10, 0, 0, 1]), bytes([10, 0, 0, 2]))
    iph = iph[:10] + struct.pack('>H', ip_checksum(iph)) + iph[12:]
    ether = bytes.fromhex('667788990011001122334455') + struct.pack('>H', 0x88B5)
    return ether + iph + udp


def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else '../pcap/python-packet-cb.pcap'
    with open(outpath, 'wb') as f:
        f.write(struct.pack('<IHHiIII', 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1))
        ts = 1704067200
        for i in range(4):
            pkt = frame(b'python packet cb test %d' % i, 100 + i)
            f.write(struct.pack('<IIII', ts, i * 1000, len(pkt), len(pkt)))
            f.write(pkt)
    print(f'Created {outpath}')


if __name__ == '__main__':
    main()
