#!/usr/bin/env python3
"""Generate adcs_synthetic.pcap covering AD CS web enrollment endpoints
(MS-WCCE/ESC8 detection paths registered in misc.c).

Each session is a one-segment HTTP/1.1 request + one-segment response over a
freshly-handshaked TCP connection. Both IPv4 and IPv6 transports are exercised.

Sessions:
   1  IPv4  GET /certsrv/                                      -> adcs-web
   2  IPv6  GET /certsrv/certfnsh.asp                          -> adcs-web
   3  IPv4  POST /certsrv/certfnsh.asp                         -> adcs-web (with body)
   4  IPv4  GET /certsrv/certrqxt.asp                          -> adcs-web
   5  IPv4  GET /CERTSRV/certrqma.asp     (mixed case)         -> adcs-web (case-insens.)
   6  IPv4  GET /certsrv/certrqus.asp                          -> adcs-web
   7  IPv4  GET /certsrv/certcarc.asp                          -> adcs-web
   8  IPv4  GET /certsrv/mscep/mscep.dll                       -> adcs-ndes
   9  IPv6  GET /certsrv/mscep_admin/                          -> adcs-ndes
  10  IPv4  POST /ADPolicyProvider_CEP_Kerberos/service.svc/CEP -> adcs-cep
  11  IPv4  POST /EnrollmentServer/service.svc                 -> adcs-ces
  12  IPv4  GET /unrelated/path                                -> (no adcs-* tag)
"""

import socket
import struct
import sys

START_TIME = 1700000100  # 2023-11-14
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


def http_session(pw, src, dst, sport, dport, request, response, v6=False):
    build = build_tcp_v6 if v6 else build_tcp_v4
    cseq = 1000
    sseq = 2000
    # 3-way handshake
    pw.write(build(src, dst, sport, dport, cseq, 0, SYN))
    pw.write(build(dst, src, dport, sport, sseq, cseq + 1, SYN_ACK))
    pw.write(build(src, dst, sport, dport, cseq + 1, sseq + 1, ACK))
    # request
    pw.write(build(src, dst, sport, dport, cseq + 1, sseq + 1, PSH_ACK, request))
    cseq += 1 + len(request)
    pw.write(build(dst, src, dport, sport, sseq + 1, cseq, ACK))
    # response
    pw.write(build(dst, src, dport, sport, sseq + 1, cseq, PSH_ACK, response))
    sseq += 1 + len(response)
    pw.write(build(src, dst, sport, dport, cseq, sseq, ACK))
    # close
    pw.write(build(src, dst, sport, dport, cseq, sseq, FIN_ACK))
    pw.write(build(dst, src, dport, sport, sseq, cseq + 1, ACK))
    pw.write(build(dst, src, dport, sport, sseq, cseq + 1, FIN_ACK))
    pw.write(build(src, dst, sport, dport, cseq + 1, sseq + 1, ACK))


def req(method, path, host, body=b''):
    if body:
        r = (f'{method} {path} HTTP/1.1\r\n'
             f'Host: {host}\r\n'
             f'Content-Type: application/x-www-form-urlencoded\r\n'
             f'Content-Length: {len(body)}\r\n'
             f'User-Agent: Mozilla/5.0\r\n'
             f'\r\n').encode() + body
    else:
        r = (f'{method} {path} HTTP/1.1\r\n'
             f'Host: {host}\r\n'
             f'User-Agent: Mozilla/5.0\r\n'
             f'\r\n').encode()
    return r


def resp(status='200 OK', body=b'<html>ok</html>'):
    return (f'HTTP/1.1 {status}\r\n'
            f'Content-Type: text/html\r\n'
            f'Content-Length: {len(body)}\r\n'
            f'Server: Microsoft-IIS/10.0\r\n'
            f'\r\n').encode() + body


SESSIONS = [
    # (v6, src, dst, sport, method, path, host, body, status)
    (False, '10.0.1.1', '10.0.1.2', 50001, 'GET',  '/certsrv/',                                            'ca.example.com', b'', '401 Unauthorized'),
    (True,  '2001:db8::1', '2001:db8::2', 50002, 'GET',  '/certsrv/certfnsh.asp',                          'ca.example.com', b'', '200 OK'),
    (False, '10.0.1.3', '10.0.1.4', 50003, 'POST', '/certsrv/certfnsh.asp',                                'ca.example.com', b'Mode=newreq&CertRequest=...&TargetStoreFlags=0', '200 OK'),
    (False, '10.0.1.5', '10.0.1.6', 50004, 'GET',  '/certsrv/certrqxt.asp',                                'ca.example.com', b'', '200 OK'),
    (False, '10.0.1.7', '10.0.1.8', 50005, 'GET',  '/CERTSRV/certrqma.asp',                                'ca.example.com', b'', '200 OK'),
    (False, '10.0.1.9', '10.0.1.10', 50006, 'GET', '/certsrv/certrqus.asp',                                'ca.example.com', b'', '200 OK'),
    (False, '10.0.1.11', '10.0.1.12', 50007, 'GET', '/certsrv/certcarc.asp',                               'ca.example.com', b'', '200 OK'),
    (False, '10.0.1.13', '10.0.1.14', 50008, 'GET', '/certsrv/mscep/mscep.dll?operation=GetCACert&message=foo', 'ca.example.com', b'', '200 OK'),
    (True,  '2001:db8::3', '2001:db8::4', 50009, 'GET', '/certsrv/mscep_admin/',                           'ca.example.com', b'', '200 OK'),
    (False, '10.0.1.15', '10.0.1.16', 50010, 'POST', '/ADPolicyProvider_CEP_Kerberos/service.svc/CEP',     'ca.example.com', b'<soap/>', '200 OK'),
    (False, '10.0.1.17', '10.0.1.18', 50011, 'POST', '/EnrollmentServer/service.svc',                      'ca.example.com', b'<soap/>', '200 OK'),
    (False, '10.0.1.19', '10.0.1.20', 50012, 'GET',  '/unrelated/path',                                    'other.example.com', b'', '200 OK'),
]


def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/adcs_synthetic.pcap'
    pw = PcapWriter(outpath)
    for v6, src, dst, sport, method, path, host, body, status in SESSIONS:
        dport = 80
        request = req(method, path, host, body)
        response = resp(status)
        http_session(pw, src, dst, sport, dport, request, response, v6=v6)
    pw.close()
    print(f'wrote {outpath}')


if __name__ == '__main__':
    main()
