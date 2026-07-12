#!/usr/bin/env python3
"""Regenerate tests/pcap/synchrophasor_synthetic.pcap in full.

Layout:
  - LEGACY blob: all 34 packets (predate generator code; add new
    sessions as section functions below)

Run from the tests directory:  python3 gen/gen_synchrophasor_synthetic.py
Each section function below documents the session(s) it generates.
"""

import base64
import struct
import sys
import zlib

# Packets accumulated before per-session generator code existed
# (zlib-compressed pcap prefix, including the 24-byte global header)
LEGACY = zlib.decompress(base64.b64decode(
    'eNrtlm9sU1UUwM997f51/1sY+wPjTWNQUme7sVENsnWOgGRooUjGEsGSdrTQrYurDD84CFkUkhFh'
    '6AcU2UbERIImmqCSkWiyDyNxGEnMDIkf1BhxJn5YFiMJifPce0/f2vtqGl/4aJPT19P37u/8fefe'
    '76Y/ndTADqnP0hIA4z8WghF+aSW5MjULoIlvVghbAB7lj7Xn995y4D8o2nTAGQWw/cZXBTQdLrXh'
    'jWwUJlkmisZJzuh0AKDgWyTNB5w6vLMPb1j1ZR5JtwMVSNmPGlG2kZgoTxFlKiulWoeJcVzlByeE'
    'I8EFm0hX3r2zduLWalLUGLUSyQ3PKzHeRvZ9zp17BVd52S+CK0tR3tEOWjAZSsYS/f74QDSk4wfC'
    'UA42pLFANDSYeNmrpz5Sb1L0ZkP394fiiYPLz3fG0hSpNyl6s6JvUPQWRW9V9I2K7lP0JxXd61F0'
    'r6I3KXqzom9Q9BZFT3cQZJMbsrTE+x4YZbwjkgzJ5yKYcY0/kyvjaoZVC1C/syvPagfeByi6xjul'
    'bz6tA/NEp2iPBfOJO0piesuGiTuU2YFF1wDspZz7fgxXMdgjuPlyEnQs0HUR4OnZ2VsdfwJs7sLr'
    'gPy/7fi/PJd2/9hogUXfHMUp3y4UpPmGv0+0PBjf8g4UWvUtlPLtGKT5VgjMfujB+Db+QxH51kti'
    '8q2bfHtP8e3zlG/d23FVJXiEb0WctTsymNQDO17Qqc11MVmeEN1+xNvogVcdFjvUXoqW73KrWl1a'
    'hzpEQOzcneIsE5wJLsuY4GGc4Ay5TJvxcW7xF6nd5K03AEqyUZhkmSgaJzmjMz4cphNIus53k5E5'
    'gFKrvlxH0iTfTUa+ByjLkqkMCmUqPJWVgpn68S/K1E3MVJnIlO1SZzlxb5CYYvyYuFuVGCeR/TXn'
    'lrhxVQO8Lrjlxm7Ctob6+kK8+qkJdVPMNo3PtozNAGdbxmaAs82r6Jn3TbOOy9HQoQqKJUpiiqWH'
    'Ynk3M5aKdalYhhfF+9UkYqnI/d60fQXwyYlKq3b3GnbH0+xWAuinc9s9W+Uku5tIuEVN9IQme6JR'
    '2K08cMOB/6BozqgzBrWNe3gfMDfacspTxdhlF7GSJJIiiYLVS6wRzuE8Zwz7a9f5oSte2C5YruXa'
    'd0biyfTaMzfWXhzz1H3NqN8mz8YVFuN57W0jnhVyj3rm4EpidZGY4tlMrKsZ8ay/+xHW4SHBWqnU'
    'n/If/LvKInu312BXAfjrzOwP9q2yyB79yWCvwj2r1cx2j1VbZB+eNNjVANM7zeySz2os1m7d70bt'
    'auT0Pl9Wm4VlEyxbBuslZNmQZdNcJ12noNb3sGCdQVat7OsRdx2xJpgUSZFEzmJjkrW/m3M4z3XK'
    'dZIN73oWT8hHBKtuua+3DAzG4ol+o7PZGTqvsf96Xvv/RGw+DUPN+sXVVK/nSdR6gZ9q/2F6veDx'
    'y1fx7PGIqNdqvp4qJY4fOxLhSFzv3tszVLrGYmt986XRWmvkiPnjzXpi9ZCYXO0i1sUMV30zR/F1'
    '8ghWfe4tBuDn59ZatDRxz7C0FuBCey5L8dO6RUsvHjYs6cB8x3NZct1psFiJX/1GJRrkwJhz/AM1'
    'DTf7'))




def sec_cfg3():
    # CFG-3 frame (IEEE C37.118.2) with 2 PMUs, spec-correct layout:
    #   PMU-ALPHA: 1 phasor, 1 analog, 1 digital word -> CHNAM has
    #   1+1+16 names, PHSCALE is 12 bytes/phasor
    #   PMU-BRAVO: 0/0/0, then DATA_RATE 30
    # Regression for synchrophasor.c CFG-3 using dgnmr (not 16*dgnmr) name
    # count and 8-byte (not 12) PHSCALE: the buggy parser desyncs and never
    # reaches PMU-BRAVO / DATA_RATE.
    # Session 10.0.0.3:50001 -> 10.0.0.4:4712, frame sent twice (classify+parse).
    import struct

    CFG3 = bytes.fromhex(
        'aa5200f400076553f100000000000000000f4240000209504d552d414c5048410001'
        '0000000000000000000000000000000000000001000100010350483003414e300344'
        '30300344303103443032034430330344303403443035034430360344303703443038'
        '03443039034431300344313103443132034431330344313403443135000000000000'
        '0000000000000000000000000000000000000000000000000000000000004d000000'
        '00000000000000000109504d552d425241564f000100000000000000000000000000'
        '00000000000000000000000000000000000000000000004d00000000000000000000'
        '0001001e0a0a')

    CLI_IP = '10.0.0.3'
    SRV_IP = '10.0.0.4'
    CLI_PORT = 50001
    SRV_PORT = 4712
    TS_START = 1700008000.0
    CLI_MAC = bytes.fromhex('02aa00001001')
    SRV_MAC = bytes.fromhex('02aa00001002')

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
        tcp = struct.pack('>HHIIBBHHH', sport, dport, seq, ack, 0x50, flags, 8192, 0, 0)
        pseudo = ip[12:20] + struct.pack('>BBH', 0, 6, 20 + len(payload))
        tcp = tcp[:16] + struct.pack('>H', csum(pseudo + tcp + payload)) + tcp[18:]
        return dmac + smac + b'\x08\x00' + ip + tcp + payload

    pkts = []
    cseq, sseq = 0x1000, 0x2000
    pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT, cseq, 0, 0x02))
    cseq += 1
    pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT, sseq, cseq, 0x12))
    sseq += 1
    pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT, cseq, sseq, 0x10))
    pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT, sseq, cseq, 0x18, CFG3))
    sseq += len(CFG3)
    pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT, cseq, sseq, 0x10))
    pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT, cseq, sseq, 0x11))
    cseq += 1
    pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT, sseq, cseq, 0x11))
    sseq += 1
    pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT, cseq, sseq, 0x10))

    out = b''
    ts = TS_START
    for p in pkts:
        sec = int(ts)
        usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.05
    return out


def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/synchrophasor_synthetic.pcap'
    out = LEGACY
    out += sec_cfg3()
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
