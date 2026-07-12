#!/usr/bin/env python3
"""Generate rpch_synthetic.pcap covering RPC-over-HTTP / Outlook Anywhere
(MS-RPCH) request-line classification (registered in misc.c).

RPC-over-HTTP uses two non-standard HTTP verbs that the standard http_parser
rejects: RPC_IN_DATA (client->proxy long-lived inbound channel) and
RPC_OUT_DATA (proxy->client long-lived outbound channel). Arkime classifies
these on the raw TCP byte prefix, not via the HTTP parser.

Sessions:
   1  IPv4  RPC_IN_DATA  /rpc/rpcproxy.dll           -> rpc-over-http
   2  IPv4  RPC_OUT_DATA /rpc/rpcproxy.dll           -> rpc-over-http
   3  IPv6  RPC_IN_DATA  /rpcwithcert/rpcproxy.dll   -> rpc-over-http
   4  IPv4  GET          /not/rpch                   -> (no tag, negative ctrl)
"""

import socket
import struct
import sys

START_TIME = 1700000200
CMAC = bytes.fromhex('001122334455')
SMAC = bytes.fromhex('66778899aabb')


def ip_cksum(data):
    if len(data) % 2:
        data += b'\x00'
    s = sum(struct.unpack('!%dH' % (len(data) // 2), data))
    while s >> 16:
        s = (s & 0xFFFF) + (s >> 16)
    return ~s & 0xFFFF


def tcp_cksum_v4(src, dst, sport, dport, seq, ack, flags, payload):
    tcp_len = 20 + len(payload)
    tcp = struct.pack('!HHIIBBHHH',
                      sport, dport, seq, ack,
                      5 << 4, flags, 65535, 0, 0) + payload
    pseudo = struct.pack('!4s4sBBH', src, dst, 0, 6, tcp_len)
    return ip_cksum(pseudo + tcp)


def tcp_cksum_v6(src, dst, sport, dport, seq, ack, flags, payload):
    tcp_len = 20 + len(payload)
    tcp = struct.pack('!HHIIBBHHH',
                      sport, dport, seq, ack,
                      5 << 4, flags, 65535, 0, 0) + payload
    pseudo = src + dst + struct.pack('!I', tcp_len) + b'\x00\x00\x00\x06'
    return ip_cksum(pseudo + tcp)


def build_tcp_v4(src_str, dst_str, sport, dport, seq, ack, flags, payload=b''):
    src = socket.inet_aton(src_str)
    dst = socket.inet_aton(dst_str)
    chk = tcp_cksum_v4(src, dst, sport, dport, seq, ack, flags, payload)
    tcp = struct.pack('!HHIIBBHHH',
                      sport, dport, seq, ack,
                      5 << 4, flags, 65535, chk, 0) + payload
    total_len = 20 + 20 + len(payload)
    ip = struct.pack('!BBHHHBBH4s4s',
                     0x45, 0, total_len,
                     0x1234, 0, 64, 6, 0, src, dst)
    ip = ip[:10] + struct.pack('!H', ip_cksum(ip)) + ip[12:]
    eth = SMAC + CMAC + b'\x08\x00'
    return eth + ip + tcp


def build_tcp_v6(src_str, dst_str, sport, dport, seq, ack, flags, payload=b''):
    src = socket.inet_pton(socket.AF_INET6, src_str)
    dst = socket.inet_pton(socket.AF_INET6, dst_str)
    chk = tcp_cksum_v6(src, dst, sport, dport, seq, ack, flags, payload)
    tcp = struct.pack('!HHIIBBHHH',
                      sport, dport, seq, ack,
                      5 << 4, flags, 65535, chk, 0) + payload
    payload_len = 20 + len(payload)
    ip = struct.pack('!IHBB', (6 << 28), payload_len, 6, 64) + src + dst
    eth = SMAC + CMAC + b'\x86\xdd'
    return eth + ip + tcp


SYN = 0x02
SYN_ACK = 0x12
ACK = 0x10
PSH_ACK = 0x18
FIN_ACK = 0x11


class PcapWriter:
    def __init__(self, path):
        self.f = open(path, 'wb')
        self.f.write(struct.pack('<IHHiIII', 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1))
        self.ts_sec = START_TIME
        self.ts_usec = 0

    def write(self, data, advance_ms=10):
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


def session(pw, src, dst, sport, dport, request, response, v6=False):
    build = build_tcp_v6 if v6 else build_tcp_v4
    cseq = 1000
    sseq = 2000
    pw.write(build(src, dst, sport, dport, cseq, 0, SYN))
    pw.write(build(dst, src, dport, sport, sseq, cseq + 1, SYN_ACK))
    pw.write(build(src, dst, sport, dport, cseq + 1, sseq + 1, ACK))
    pw.write(build(src, dst, sport, dport, cseq + 1, sseq + 1, PSH_ACK, request))
    cseq += 1 + len(request)
    pw.write(build(dst, src, dport, sport, sseq + 1, cseq, ACK))
    pw.write(build(dst, src, dport, sport, sseq + 1, cseq, PSH_ACK, response))
    sseq += 1 + len(response)
    pw.write(build(src, dst, sport, dport, cseq, sseq, ACK))
    pw.write(build(src, dst, sport, dport, cseq, sseq, FIN_ACK))
    pw.write(build(dst, src, dport, sport, sseq, cseq + 1, ACK))
    pw.write(build(dst, src, dport, sport, sseq, cseq + 1, FIN_ACK))
    pw.write(build(src, dst, sport, dport, cseq + 1, sseq + 1, ACK))


def rpch_req(verb, path, host):
    return (f'{verb} {path} HTTP/1.1\r\n'
            f'Host: {host}\r\n'
            f'User-Agent: MSRPC\r\n'
            f'Cache-Control: no-cache\r\n'
            f'Connection: Keep-Alive\r\n'
            f'Content-Length: 1073741824\r\n'
            f'\r\n').encode()


def rpch_resp():
    return (b'HTTP/1.1 200 Success\r\n'
            b'Server: Microsoft-IIS/10.0\r\n'
            b'\r\n')


def http_req(method, path, host):
    return (f'{method} {path} HTTP/1.1\r\n'
            f'Host: {host}\r\n'
            f'User-Agent: curl/8.0\r\n'
            f'\r\n').encode()


def http_resp():
    body = b'<html>ok</html>'
    return (f'HTTP/1.1 200 OK\r\n'
            f'Content-Type: text/html\r\n'
            f'Content-Length: {len(body)}\r\n'
            f'\r\n').encode() + body


SESSIONS = [
    # (v6, src, dst, sport, dport, request, response)
    (False, '10.0.2.1',  '10.0.2.2', 51001, 80,
     rpch_req('RPC_IN_DATA',  '/rpc/rpcproxy.dll?MAIL.example.com:6001', 'mail.example.com'),
     rpch_resp()),
    (False, '10.0.2.3',  '10.0.2.4', 51002, 80,
     rpch_req('RPC_OUT_DATA', '/rpc/rpcproxy.dll?MAIL.example.com:6001', 'mail.example.com'),
     rpch_resp()),
    (True,  '2001:db8::5', '2001:db8::6', 51003, 80,
     rpch_req('RPC_IN_DATA',  '/rpcwithcert/rpcproxy.dll?MAIL.example.com:6004', 'mail.example.com'),
     rpch_resp()),
    (False, '10.0.2.5',  '10.0.2.6', 51004, 80,
     http_req('GET', '/not/rpch', 'other.example.com'),
     http_resp()),
]


def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/rpch_synthetic.pcap'
    pw = PcapWriter(outpath)
    for v6, src, dst, sport, dport, request, response in SESSIONS:
        session(pw, src, dst, sport, dport, request, response, v6=v6)
    pw.close()
    print(f'wrote {outpath}')


if __name__ == '__main__':
    main()
