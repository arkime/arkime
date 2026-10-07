#!/usr/bin/env python3
"""Regenerate tests/pcap/tcpseq_synthetic.pcap in full.

Layout:
  - LEGACY blob: all 39 packets (predate generator code; add new
    sessions as section functions below)
  - out_of_window_rst_syn (12 packets)
  - rst_loss_direction (4 sessions, 58 packets)

Run from the tests directory:  python3 gen/gen_tcpseq_synthetic.py
Each section function below documents the session(s) it generates.
"""

import base64
import struct
import sys
import zlib

# Packets accumulated before per-session generator code existed
# (zlib-compressed pcap prefix, including the 24-byte global header)
LEGACY = zlib.decompress(base64.b64decode(
    'eNqtlV1IFFEUx89+tK4rpCZkEDQLfShitilYrFD7MbuQSS2BlH1B2KcR6UNCECTRwwrmQ7QVMoGC'
    '1EsghZJLmSsiUVAoRK/qS7gPmkok9tB2zj1zLXQfxosHzu7A3Ps7//M/M3e+jr7usYMT/oUTbPT3'
    'tOZqwAA4A5xgywAsvcB7BeCGugqoE4tdADvEhWNlv7+OQ59shP8D97fBnqbGs3ZRoMtHv/59mQyV'
    'Ku3eyFKb6YdK+YjWJQqWylJ0r9rMR2U35zruNTWP9S0suyGCq2hpwHX5swc3YtqNoBbAkjO0K2b3'
    'wrtW9mbGYZFiJ5IWMIIAOeNISse2eKE/wZTxHAUtaSRNxAqQ8pgpg3loBXCuoZSblI9ZKdu8UOkG'
    'CGIQqTtfoasJpM0KPQ9ZT7xIQc+s1OOzA4QwiNRcrKhnQejpZD0nt1ugkB4HptMIaUjJG5QT7+9m'
    'ymHNIsVJJC1ohADycW9ekibe+4MpJTsVtCSR1EMd9c4zJb/EgsNjqyhDgoIOL+4FaMAg0lKZAikp'
    'ST+rAE5hEGmyQsGfHqSNiM5muLMPlRYopGcTpssIa0gpistZvfzElL5qixQXkbSQEQYobkZSO80q'
    'cYQpT/wKWtqR1EIdJWqZcveQBYdHslLQ4Sm8Oo1BpCuhjSIdjyr404K0B6KzKHd2oNYChfTkYLoN'
    'XQtnMpkpOavUdaZoxyxS3ETSwoaOCwuQNE2zSpkuu04oaJnGxYXUUeooU+bqAXBcItdQfCYlmZWC'
    'Dr+t5pNUnqbfGhQ6K0RiWmgKs6b35xQ0paWmN2V8msoT9fkFRU3iRE3VsKbOSwD0EESzUQ6alFer'
    'NC1ITbvxSQyb0WgGUVuvqWijT7zQVs7azt9Yr19EkNr68Fuqm0G0aIuiplyhaRdrqrhlgTKEhFxM'
    'jxHRdPRrfuVNqWfK1tsWKR4iaboRocbE7PBNGdaZ8ueOghacnU24PBxhyvc2Cy4PZKWgy7/xrN1v'
    'BtG+3F8vjWfGtEV8z6rMINpAXEHbsqT9ughQaQbRnnUouG6TT8AwfhX/Apybp/I='))




def sec_out_of_window_rst_syn():
    # Session 10.9.34.1:49960 -> 10.9.34.2:80 (Ethernet linktype):
    #   After GET /before-rst, a spoofed server RST far outside the window and
    #   a spoofed client SYN with a new ISN arrive, then the real client sends
    #   GET /after-rst on the original sequence. Expect one session with both
    #   urls, not a split.
    TS_START = 1748736100.0
    CLI, SRV = '10.9.34.1', '10.9.34.2'
    CMAC = bytes.fromhex('02aa00002201')
    SMAC = bytes.fromhex('02aa00002202')

    def csum(data):
        if len(data) & 1:
            data += b'\0'
        s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
        while s >> 16:
            s = (s & 0xffff) + (s >> 16)
        return (~s) & 0xffff

    def eth_ip_tcp(src, dst, smac, dmac, sport, dport, seq, ack, flags, payload=b''):
        iplen = 20 + 20 + len(payload)
        ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, iplen, 1, 0, 64, 6, 0,
                         bytes(map(int, src.split('.'))), bytes(map(int, dst.split('.'))))
        ip = ip[:10] + struct.pack('>H', csum(ip)) + ip[12:]
        tcp = struct.pack('>HHIIBBHHH', sport, dport, seq & 0xffffffff, ack & 0xffffffff, 0x50, flags, 8192, 0, 0)
        pseudo = ip[12:20] + struct.pack('>BBH', 0, 6, 20 + len(payload))
        tcp = tcp[:16] + struct.pack('>H', csum(pseudo + tcp + payload)) + tcp[18:]
        return dmac + smac + b'\x08\x00' + ip + tcp + payload

    def build():
        pkts = []
        cseq, sseq = 0x10000, 0x20000
        c = lambda flags, seq, ack, p=b'': pkts.append(eth_ip_tcp(CLI, SRV, CMAC, SMAC, 49960, 80, seq, ack, flags, p))
        s = lambda flags, seq, ack, p=b'': pkts.append(eth_ip_tcp(SRV, CLI, SMAC, CMAC, 80, 49960, seq, ack, flags, p))

        c(0x02, cseq, 0); cseq += 1
        s(0x12, sseq, cseq); sseq += 1
        c(0x10, cseq, sseq)
        req1 = b'GET /before-rst HTTP/1.1\r\nHost: tcpsplit.example\r\n\r\n'
        c(0x18, cseq, sseq, req1); cseq += len(req1)
        resp = b'HTTP/1.1 204 No Content\r\n\r\n'
        s(0x18, sseq, cseq, resp); sseq += len(resp)

        s(0x04, sseq + 0x40000000, 0)       # spoofed RST, out of window
        c(0x02, 0x55550000, 0)              # spoofed SYN, new ISN

        req2 = b'GET /after-rst HTTP/1.1\r\nHost: tcpsplit.example\r\n\r\n'
        c(0x18, cseq, sseq, req2); cseq += len(req2)
        s(0x18, sseq, cseq, resp); sseq += len(resp)
        c(0x11, cseq, sseq); cseq += 1
        s(0x11, sseq, cseq); sseq += 1
        c(0x10, cseq, sseq)

        out = b''
        ts = TS_START
        for p in pkts:
            sec = int(ts)
            usec = int(round((ts - sec) * 1e6))
            out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
            ts += 0.05
        return out
    return build()


def sec_rst_loss_direction():
    # Only loss in the RST's own direction lets an out of window RST count as a close.
    # Each session: GET /before-rst, a loss marker, an out of window client RST and a
    # client SYN with a new ISN, then either a new connection or the old one continuing.
    #   10.9.47.1:49961 client segments past a gap arrive out of order (the RST's direction):
    #                   split, second session has GET /new-conn
    #   10.9.48.1:49962 server segments past a gap arrive out of order (other direction):
    #                   no split, GET /after-rst kept
    #   10.9.49.1:49963 server acks unseen client bytes (loss in the RST's direction): split
    #   10.9.50.1:49964 client acks unseen server bytes (other direction): no split
    TS_START = 1748736200.0
    CMAC = bytes.fromhex('02aa00002401')
    SMAC = bytes.fromhex('02aa00002402')

    def csum(data):
        if len(data) & 1:
            data += b'\0'
        s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
        while s >> 16:
            s = (s & 0xffff) + (s >> 16)
        return (~s) & 0xffff

    def eth_ip_tcp(src, dst, smac, dmac, sport, dport, seq, ack, flags, payload=b''):
        iplen = 20 + 20 + len(payload)
        ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, iplen, 1, 0, 64, 6, 0,
                         bytes(map(int, src.split('.'))), bytes(map(int, dst.split('.'))))
        ip = ip[:10] + struct.pack('>H', csum(ip)) + ip[12:]
        tcp = struct.pack('>HHIIBBHHH', sport, dport, seq & 0xffffffff, ack & 0xffffffff, 0x50, flags, 8192, 0, 0)
        pseudo = ip[12:20] + struct.pack('>BBH', 0, 6, 20 + len(payload))
        tcp = tcp[:16] + struct.pack('>H', csum(pseudo + tcp + payload)) + tcp[18:]
        return dmac + smac + b'\x08\x00' + ip + tcp + payload

    def session(n, port, loss, split):
        cli, srv = '10.9.%d.1' % n, '10.9.%d.2' % n
        pkts = []
        c = lambda flags, seq, ack, p=b'': pkts.append(eth_ip_tcp(cli, srv, CMAC, SMAC, port, 80, seq, ack, flags, p))
        s = lambda flags, seq, ack, p=b'': pkts.append(eth_ip_tcp(srv, cli, SMAC, CMAC, 80, port, seq, ack, flags, p))
        req = lambda path: b'GET /%s HTTP/1.1\r\nHost: rstdir.example\r\n\r\n' % path
        resp = b'HTTP/1.1 204 No Content\r\n\r\n'

        cseq, sseq = 0x30000, 0x40000
        c(0x02, cseq, 0); cseq += 1
        s(0x12, sseq, cseq); sseq += 1
        c(0x10, cseq, sseq)
        c(0x18, cseq, sseq, req(b'before-rst')); cseq += len(req(b'before-rst'))
        s(0x18, sseq, cseq, resp); sseq += len(resp)

        if loss == 'client-gap':
            c(0x18, cseq + 2000, sseq, b'second-after-gap')
            c(0x18, cseq + 1000, sseq, b'first-after-gap')
        elif loss == 'server-gap':
            s(0x18, sseq + 2000, cseq, b'second-after-gap')
            s(0x18, sseq + 1000, cseq, b'first-after-gap')
        elif loss == 'server-acks-unseen':
            s(0x10, sseq, cseq + 5000)
        elif loss == 'client-acks-unseen':
            c(0x10, cseq, sseq + 5000)

        c(0x04, cseq + 0x40000000, 0)       # out of window client RST
        nseq = 0x77770000
        c(0x02, nseq, 0)                    # client SYN, new ISN

        if split:
            nsseq = 0x88880000
            nseq += 1
            s(0x12, nsseq, nseq); nsseq += 1
            c(0x10, nseq, nsseq)
            c(0x18, nseq, nsseq, req(b'new-conn')); nseq += len(req(b'new-conn'))
            s(0x18, nsseq, nseq, resp); nsseq += len(resp)
            c(0x11, nseq, nsseq); nseq += 1
            s(0x11, nsseq, nseq); nsseq += 1
            c(0x10, nseq, nsseq)
        else:
            c(0x18, cseq, sseq, req(b'after-rst')); cseq += len(req(b'after-rst'))
            s(0x18, sseq, cseq, resp); sseq += len(resp)
            c(0x11, cseq, sseq); cseq += 1
            s(0x11, sseq, cseq); sseq += 1
            c(0x10, cseq, sseq)
        return pkts

    pkts = session(47, 49961, 'client-gap', True)
    pkts += session(48, 49962, 'server-gap', False)
    pkts += session(49, 49963, 'server-acks-unseen', True)
    pkts += session(50, 49964, 'client-acks-unseen', False)

    out = b''
    ts = TS_START
    for p in pkts:
        sec = int(ts)
        usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.05
    return out


def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/tcpseq_synthetic.pcap'
    out = LEGACY
    out += sec_out_of_window_rst_syn()
    out += sec_rst_loss_direction()
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
