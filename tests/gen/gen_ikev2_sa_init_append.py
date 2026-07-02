#!/usr/bin/env python3
"""Append an IKEv2 IKE_SA_INIT session to arkime_synthetic.pcap exercising
two isakmp.c fixes:
  1. Initiator-side payloads are parsed (IKEv2 flag 0x08 is Initiator, not
     "encrypted"; only non-SA_INIT exchanges are encrypted).
  2. IKEv2 ENCR transform IDs use the v2 IANA table (12=aes-cbc, 20=aes-gcm-16,
     28=chacha20-poly1305, ...), not the IKEv1 attribute table.

Session 10.0.90.1:500 -> 10.0.90.2:500 (UDP):
  pkt1: initiator IKE_SA_INIT request  (classify only, UDP port classifier)
  pkt2: same request retransmitted     (parsed: ENCR aes-gcm-16, PRF sha2-256,
                                        DH curve25519)
  pkt3: responder IKE_SA_INIT response (parsed: ENCR chacha20-poly1305,
                                        PRF sha2-512, DH curve448)

Asymmetric transforms prove both directions are parsed. Expected merged
fields: encryption [aes-gcm-16, chacha20-poly1305], hash [prf-hmac-sha2-256,
prf-hmac-sha2-512], dhGroup [curve25519, curve448].

Idempotent: skips appending if the pcap already has > BASE_PACKETS packets.
"""

import struct

PCAP = 'pcap/arkime_synthetic.pcap'
BASE_PACKETS = 711  # packets before this script's additions
TS_START = 1700009100.0

CMAC = bytes.fromhex('001122334455')
SMAC = bytes.fromhex('66778899aabb')

INIT_SPI = bytes.fromhex('1122334455667788')
RESP_SPI = bytes.fromhex('99aabbccddeeff00')


def csum(data):
    if len(data) & 1:
        data += b'\0'
    s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
    while s >> 16:
        s = (s & 0xffff) + (s >> 16)
    return (~s) & 0xffff


def transform(last, ttype, tid):
    # Last(0)/More(3) | Reserved | Length | Type | Reserved | ID
    return struct.pack('!BBHBBH', last, 0, 8, ttype, 0, tid)


def sa_payload(next_payload, transforms):
    # One proposal: Last(0) | Reserved | Length | Proposal#1 | ProtoID=1(IKE) |
    # SPISize=0 | NumTransforms
    tdata = b''.join(transforms)
    proposal = struct.pack('!BBHBBBB', 0, 0, 8 + len(tdata), 1, 1, 0,
                           len(transforms)) + tdata
    # Generic payload header: NextPayload | Critical/Reserved | Length
    return struct.pack('!BBH', next_payload, 0, 4 + len(proposal)) + proposal


def ike_sa_init(resp_spi, flags, sa):
    # ISAKMP header: ISPI(8) RSPI(8) NextPayload=33(SA) Ver=0x20 Exch=34 Flags
    # MsgID(4) Length(4)
    length = 28 + len(sa)
    hdr = INIT_SPI + resp_spi + struct.pack('!BBBBII', 33, 0x20, 34, flags,
                                            0, length)
    return hdr + sa


def build_udp(src_str, dst_str, sport, dport, payload):
    src = bytes(map(int, src_str.split('.')))
    dst = bytes(map(int, dst_str.split('.')))
    udp_len = 8 + len(payload)
    udp = struct.pack('!HHHH', sport, dport, udp_len, 0)
    pseudo = src + dst + struct.pack('!BBH', 0, 17, udp_len)
    udp = udp[:6] + struct.pack('!H', csum(pseudo + udp + payload))

    total_len = 20 + udp_len
    ip = struct.pack('!BBHHHBBH4s4s', 0x45, 0, total_len, 0x9000, 0, 64, 17,
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

    # Initiator proposal: ENCR aes-gcm-16(20), PRF hmac-sha2-256(5), DH curve25519(31)
    req_sa = sa_payload(0, [
        transform(3, 1, 20),
        transform(3, 2, 5),
        transform(0, 4, 31),
    ])
    req = ike_sa_init(b'\x00' * 8, 0x08, req_sa)  # Initiator flag

    # Responder choice: ENCR chacha20-poly1305(28), PRF hmac-sha2-512(7), DH curve448(32)
    resp_sa = sa_payload(0, [
        transform(3, 1, 28),
        transform(3, 2, 7),
        transform(0, 4, 32),
    ])
    resp = ike_sa_init(RESP_SPI, 0x20, resp_sa)  # Response flag

    pkts = [
        build_udp('10.0.90.1', '10.0.90.2', 500, 500, req),
        build_udp('10.0.90.1', '10.0.90.2', 500, 500, req),  # retransmit, gets parsed
        build_udp('10.0.90.2', '10.0.90.1', 500, 500, resp),
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
