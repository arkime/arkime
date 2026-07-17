#!/usr/bin/env python3
"""Generate multidlt_synthetic.pcapng: a pcapng file with multiple interfaces
using different link types (DLTs), exercising Arkime's per-interface/per-packet
DLT support with real HTTP and DNS traffic.

Interfaces (one IDB each, interface_id = order below):
   0  EN10MB   (linktype 1)
   1  EN10MB   (linktype 1)
   2  RAW      (linktype 101)
   3  RAW      (linktype 101)
   4  NULL     (linktype 0, BSD loopback)
   5  NULL     (linktype 0, BSD loopback)

Sessions are keyed by 5-tuple, NOT by interface, so the same tuple seen on
different interfaces/DLTs is one Arkime session. Flows:

   A cross-DLT HTTP  10.20.0.1:40001  <-> 10.20.0.2:80   on EN10MB(0)+RAW(2)+NULL(4)
   B cross-DLT DNS   10.20.0.3:40002  <-> 10.20.0.4:53   on EN10MB(1)+RAW(3)
   C single EN10MB HTTP 10.20.0.5:40003  <-> 10.20.0.6:80   on iface 0
   D single EN10MB DNS  10.20.0.7:40004  <-> 10.20.0.8:53   on iface 1
   E single RAW HTTP    10.20.0.9:40005  <-> 10.20.0.10:80  on iface 2
   F single RAW DNS     10.20.0.11:40006 <-> 10.20.0.12:53  on iface 3
   G single NULL HTTP   10.20.0.13:40007 <-> 10.20.0.14:80  on iface 4
   H single NULL DNS    10.20.0.15:40008 <-> 10.20.0.16:53  on iface 5
"""

import socket
import struct
import sys

BASE_TIME = 1700000000  # 2023-11-14, > Sep-2001 sanity check

CMAC = bytes.fromhex('001122334455')
SMAC = bytes.fromhex('66778899aabb')

# Link types
LT_NULL = 0
LT_EN10MB = 1
LT_RAW = 101

# ---------------------------------------------------------------------------
# checksum + L3/L4 builders
# ---------------------------------------------------------------------------


def ip_cksum(data):
    if len(data) % 2:
        data += b'\x00'
    s = sum(struct.unpack('!%dH' % (len(data) // 2), data))
    while s >> 16:
        s = (s & 0xFFFF) + (s >> 16)
    return ~s & 0xFFFF


def ipv4(src_str, dst_str, proto, payload):
    src = socket.inet_aton(src_str)
    dst = socket.inet_aton(dst_str)
    total_len = 20 + len(payload)
    ip = struct.pack('!BBHHHBBH4s4s',
                     0x45, 0, total_len,
                     0x4242, 0,
                     64, proto, 0,
                     src, dst)
    ip = ip[:10] + struct.pack('!H', ip_cksum(ip)) + ip[12:]
    return ip + payload


def tcp(src_str, dst_str, sport, dport, seq, ack, flags, payload=b''):
    src = socket.inet_aton(src_str)
    dst = socket.inet_aton(dst_str)
    offset = (5 << 4)
    hdr = struct.pack('!HHIIBBHHH',
                      sport, dport, seq, ack,
                      offset, flags, 8192, 0, 0)
    pseudo = struct.pack('!4s4sBBH', src, dst, 0, 6, len(hdr) + len(payload))
    chk = ip_cksum(pseudo + hdr + payload)
    hdr = hdr[:16] + struct.pack('!H', chk) + hdr[18:]
    return ipv4(src_str, dst_str, 6, hdr + payload)


def udp(src_str, dst_str, sport, dport, payload=b''):
    src = socket.inet_aton(src_str)
    dst = socket.inet_aton(dst_str)
    udp_len = 8 + len(payload)
    hdr = struct.pack('!HHHH', sport, dport, udp_len, 0)
    pseudo = struct.pack('!4s4sBBH', src, dst, 0, 17, udp_len)
    chk = ip_cksum(pseudo + hdr + payload)
    hdr = struct.pack('!HHHH', sport, dport, udp_len, chk)
    return ipv4(src_str, dst_str, 17, hdr + payload)


# ---------------------------------------------------------------------------
# HTTP + DNS payloads
# ---------------------------------------------------------------------------


def http_request(host):
    return (f'GET / HTTP/1.1\r\nHost: {host}\r\n'
            f'User-Agent: arkime-multidlt-test\r\n\r\n').encode()


def http_response(body=b'hi'):
    return (b'HTTP/1.1 200 OK\r\nServer: arkime-test\r\n'
            b'Content-Type: text/plain\r\nContent-Length: %d\r\n\r\n' % len(body)) + body


def dns_name(name):
    out = b''
    for label in name.split('.'):
        out += bytes([len(label)]) + label.encode()
    return out + b'\x00'


def dns_query(txid, qname):
    hdr = struct.pack('!HHHHHH', txid, 0x0100, 1, 0, 0, 0)
    q = dns_name(qname) + struct.pack('!HH', 1, 1)  # type A, class IN
    return hdr + q


def dns_response(txid, qname, ip):
    hdr = struct.pack('!HHHHHH', txid, 0x8180, 1, 1, 0, 0)
    q = dns_name(qname) + struct.pack('!HH', 1, 1)
    ans = struct.pack('!HHHIH', 0xC00C, 1, 1, 300, 4) + socket.inet_aton(ip)
    return hdr + q + ans


# ---------------------------------------------------------------------------
# link-layer framing per DLT
# ---------------------------------------------------------------------------


def frame(linktype, ip_pkt, to_server):
    if linktype == LT_EN10MB:
        if to_server:
            return SMAC + CMAC + b'\x08\x00' + ip_pkt
        return CMAC + SMAC + b'\x08\x00' + ip_pkt
    if linktype == LT_RAW:
        return ip_pkt
    if linktype == LT_NULL:
        return struct.pack('<I', 2) + ip_pkt  # AF_INET
    raise ValueError(linktype)


# ---------------------------------------------------------------------------
# pcapng writer
# ---------------------------------------------------------------------------


class PcapNGWriter:
    def __init__(self, path, interfaces):
        self.f = open(path, 'wb')
        self.ts_us = BASE_TIME * 1_000_000
        self._write_shb()
        for linktype in interfaces:
            self._write_idb(linktype)

    def _block(self, block_type, body):
        total = 12 + len(body)
        self.f.write(struct.pack('<II', block_type, total))
        self.f.write(body)
        self.f.write(struct.pack('<I', total))

    def _write_shb(self):
        body = struct.pack('<IHHq', 0x1A2B3C4D, 1, 0, -1)
        self._block(0x0A0D0D0A, body)

    def _write_idb(self, linktype):
        body = struct.pack('<HHI', linktype, 0, 262144)
        self._block(0x00000001, body)

    def write(self, interface_id, data, advance_ms=10):
        self.ts_us += advance_ms * 1000
        caplen = len(data)
        pad = (4 - (caplen & 3)) & 3
        body = struct.pack('<IIIII',
                           interface_id,
                           self.ts_us >> 32,
                           self.ts_us & 0xFFFFFFFF,
                           caplen, caplen)
        body += data + (b'\x00' * pad)
        self._block(0x00000006, body)

    def close(self):
        self.f.close()


# ---------------------------------------------------------------------------
# session helpers
# ---------------------------------------------------------------------------


def http_session(pw, steps, c_ip, s_ip, sport, dport, host):
    """steps = list of 5 (interface_id, linktype) for SYN, SYN-ACK, ACK,
    request, response. Same list repeated for a single-DLT session, or mixed
    for a cross-DLT session."""
    req = http_request(host)
    resp = http_response()
    s0, r0 = 1000, 5000
    syn, synack, ack, reqs, resps = steps

    pw.write(syn[0],    frame(syn[1],    tcp(c_ip, s_ip, sport, dport, s0, 0, 0x02), True))
    pw.write(synack[0], frame(synack[1], tcp(s_ip, c_ip, dport, sport, r0, s0 + 1, 0x12), False))
    pw.write(ack[0],    frame(ack[1],    tcp(c_ip, s_ip, sport, dport, s0 + 1, r0 + 1, 0x10), True))
    pw.write(reqs[0],   frame(reqs[1],   tcp(c_ip, s_ip, sport, dport, s0 + 1, r0 + 1, 0x18, req), True))
    pw.write(resps[0],  frame(resps[1],  tcp(s_ip, c_ip, dport, sport, r0 + 1, s0 + 1 + len(req), 0x18, resp), False))


def dns_session(pw, qstep, rstep, c_ip, s_ip, sport, host, answer_ip):
    """qstep/rstep = (interface_id, linktype) for the query and response."""
    txid = 0x1234
    pw.write(qstep[0], frame(qstep[1], udp(c_ip, s_ip, sport, 53, dns_query(txid, host)), True))
    pw.write(rstep[0], frame(rstep[1], udp(s_ip, c_ip, 53, sport, dns_response(txid, host, answer_ip)), False))


def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/multidlt_synthetic.pcapng'

    interfaces = [LT_EN10MB, LT_EN10MB, LT_RAW, LT_RAW, LT_NULL, LT_NULL]
    pw = PcapNGWriter(outpath, interfaces)

    EN0 = (0, LT_EN10MB)
    EN1 = (1, LT_EN10MB)
    RAW2 = (2, LT_RAW)
    RAW3 = (3, LT_RAW)
    NULL4 = (4, LT_NULL)
    NULL5 = (5, LT_NULL)

    # A: cross-DLT HTTP, handshake+exchange spread over EN10MB(0), RAW(2), NULL(4)
    http_session(pw, [EN0, RAW2, NULL4, EN0, RAW2],
                 '10.20.0.1', '10.20.0.2', 40001, 80, 'a.example.com')

    # B: cross-DLT DNS, query EN10MB(1), response RAW(3)
    dns_session(pw, EN1, RAW3, '10.20.0.3', '10.20.0.4', 40002, 'b.example.com', '10.20.0.4')

    # C: single EN10MB HTTP on iface 0
    http_session(pw, [EN0] * 5, '10.20.0.5', '10.20.0.6', 40003, 80, 'c.example.com')

    # D: single EN10MB DNS on iface 1
    dns_session(pw, EN1, EN1, '10.20.0.7', '10.20.0.8', 40004, 'd.example.com', '10.20.0.8')

    # E: single RAW HTTP on iface 2
    http_session(pw, [RAW2] * 5, '10.20.0.9', '10.20.0.10', 40005, 80, 'e.example.com')

    # F: single RAW DNS on iface 3
    dns_session(pw, RAW3, RAW3, '10.20.0.11', '10.20.0.12', 40006, 'f.example.com', '10.20.0.12')

    # G: single NULL HTTP on iface 4
    http_session(pw, [NULL4] * 5, '10.20.0.13', '10.20.0.14', 40007, 80, 'g.example.com')

    # H: single NULL DNS on iface 5
    dns_session(pw, NULL5, NULL5, '10.20.0.15', '10.20.0.16', 40008, 'h.example.com', '10.20.0.16')

    pw.close()
    print(f'Created {outpath} with {len(interfaces)} interfaces')


if __name__ == '__main__':
    main()
