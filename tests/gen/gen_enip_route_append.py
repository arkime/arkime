#!/usr/bin/env python3
"""Append a routed-CIP ENIP regression session to arkime_synthetic.pcap.

Session 10.40.3.100:50006 -> 10.40.3.10:44818  (EtherNet/IP over UDP):
  The SAME SendRRData datagram is sent twice. Arkime's UDP classifier consumes
  the first datagram to classify the session as "enip" and register the parser;
  the second datagram is the one actually parsed.

  The datagram carries a CIP Get_Attribute_Single request whose request path is
  *routed*: it begins with a Port Segment (Extended Link Address, port 1, link
  address "1.2.3.4") before the Logical Class segment (Class 0x01 = Identity).
  This is the only session in arkime_synthetic.pcap whose CIP path starts with a
  Port Segment, so it is the only one that exercises the port-segment branch of
  cip_extract_class() in capture/parsers/enip.c -- every other CIP session uses
  Logical-only paths (0x20/0x24). A parser that does not skip the routed port
  segment extracts no class; the fixed parser walks past it and records
  enip.class = "Identity".

Run from the tests/ directory:  python3 gen/gen_enip_route_append.py
Idempotent: does nothing if the session's unique CIP path is already present.
"""

import os
import struct

PCAP = os.path.join(os.path.dirname(__file__), '..', 'pcap', 'arkime_synthetic.pcap')

# MACs match the other enip frames already in arkime_synthetic.pcap.
CLIENT_MAC = bytes.fromhex('6a70c5aff1f9')   # eth.src on client -> server
SERVER_MAC = bytes.fromhex('942a6fee8c83')   # eth.dst on client -> server

CLI_IP = '10.40.3.100'      # fresh subnet -> its own session, distinct from
SRV_IP = '10.40.3.10'       # the existing enip sessions (10.40.1.x / 10.40.2.x)
CLI_PORT = 50006
SRV_PORT = 44818            # EtherNet/IP

# Continue the enip timeline (last existing enip frame is ~1778361740).
TS_START = 1778361745.0

# CIP request path (8 words = 16 bytes): routed Port Segment then Logical Class.
#   0x11        Port Segment, port id 1, Extended Link Address bit (0x10)
#   0x07        link address size = 7 (odd -> 1 pad byte follows the address)
#   "1.2.3.4"   7 link-address bytes
#   0x00        pad
#   0x20 0x01   Logical 8-bit Class     = 0x01 (Identity)
#   0x24 0x01   Logical 8-bit Instance  = 1
#   0x30 0x07   Logical 8-bit Attribute = 7
CIP_PATH = bytes([0x11, 0x07, 0x31, 0x2e, 0x32, 0x2e, 0x33, 0x2e, 0x34, 0x00,
                  0x20, 0x01, 0x24, 0x01, 0x30, 0x07])
# Unique marker for idempotency: the port segment + "1.2.3.4" + pad byte.
MARKER = CIP_PATH[:10]


def build_enip_datagram():
    """SendRRData (0x006f) carrying the routed Get_Attribute_Single request."""
    # CIP: service Get_Attribute_Single (0x0e), path size in 16-bit words, path.
    cip = bytes([0x0e, len(CIP_PATH) // 2]) + CIP_PATH
    # CPF: interface handle, timeout, item count 2; null address item; data item.
    body = (struct.pack('<IHH', 0, 0, 2) +
            struct.pack('<HH', 0x0000, 0) +
            struct.pack('<HH', 0x00b2, len(cip)) + cip)
    # ENIP encapsulation header (24 bytes) + body. The length field must equal
    # len(body) so that ENIP_HEADER_LEN(24) + length == datagram length exactly,
    # which enip_udp_classify() requires.
    return struct.pack('<HHII8sI', 0x006f, len(body), 1, 0, b'\x00' * 8, 0) + body


def _ones_complement(data):
    if len(data) & 1:
        data += b'\x00'
    s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
    while s >> 16:
        s = (s & 0xffff) + (s >> 16)
    return (~s) & 0xffff


def build_frame(payload):
    """Ethernet/IPv4/UDP frame carrying payload (CLI -> SRV)."""
    src = bytes(map(int, CLI_IP.split('.')))
    dst = bytes(map(int, SRV_IP.split('.')))

    udp_len = 8 + len(payload)
    udp = struct.pack('>HHHH', CLI_PORT, SRV_PORT, udp_len, 0)
    pseudo = src + dst + struct.pack('>BBH', 0, 17, udp_len)
    chk = _ones_complement(pseudo + udp + payload) or 0xffff
    udp = udp[:6] + struct.pack('>H', chk)

    total_len = 20 + udp_len
    ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, total_len,
                     0x4203, 0, 64, 17, 0, src, dst)
    ip = ip[:10] + struct.pack('>H', _ones_complement(ip)) + ip[12:]

    return SERVER_MAC + CLIENT_MAC + b'\x08\x00' + ip + udp + payload


def main():
    with open(PCAP, 'rb') as f:
        data = f.read()

    if MARKER in data:
        print('%s already contains the routed-CIP enip session, not appending' % PCAP)
        return

    frame = build_frame(build_enip_datagram())

    # Two identical datagrams: the first classifies, the second is parsed.
    out = b''
    ts = TS_START
    for _ in range(2):
        sec = int(ts)
        usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(frame), len(frame)) + frame
        ts += 0.001

    with open(PCAP, 'ab') as f:
        f.write(out)
    print('appended 2 packets (routed-CIP enip session) to %s' % PCAP)


if __name__ == '__main__':
    main()
