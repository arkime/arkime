#!/usr/bin/env python3
"""Regenerate tests/pcap/dnssec_synthetic.pcap in full.

Layout:
  - LEGACY blob: all 10 packets (predate generator code; add new
    sessions as section functions below)
  - dns_query_midsave (481 packets)
  - dns_bad_idna (2 packets)
  - dns_utf8_names (5 packets)

Run from the tests directory:  python3 gen/gen_dnssec_synthetic.py
Each section function below documents the session(s) it generates.
"""

import base64
import struct
import sys
import zlib

# Packets accumulated before per-session generator code existed
# (zlib-compressed pcap prefix, including the 24-byte global header)
LEGACY = zlib.decompress(base64.b64decode(
    'eNq7cnjTQiYGFgYY+P+fgYERSAf8n5wO4rtDMYOgkrFLaFp5x8xVuzkYXBkYLEHKHARXTD+wgjGF'
    'AwgMLBlMGVSlK4RMGCFGAAF7akVibkFOKnNyfi5IkDEQaiwnIwRDDIQYDjb2N8TY5VdBRoKMZjAF'
    'Gvxc1EDIpLEBKMeM1VhUAT2QGYw6DPYMjLxMDAx8AulTqxnSc98xGFiiKLy3dt97fBjVVH2oqVIs'
    'eakVJahybAkMDBwMzKiC2mAPNzCoXDHkZVq1+8xdeBAygHjv/iOHaRCx4T0NGCip4PC2AoU3V6qy'
    'K57wDoYamwHFGOEdBTW2FBreqcDwtmJwm3FZ2RUc3owkhDcXPLw7Q4j1zlSgnWlg71iDvPMz0CQM'
    'j3dCocYmQTGGd0KgxlZBvZMG9I41g8MlM5Mwor2jDfUOi4ElB1MYsR6ZArQtHewRG5BHXti5puPx'
    'SDjU2BIoxvBIGtTYdKhH0oEesWEI8gt0TSfaI7AUK4YlxSokMEQQ67XJQPszwF6zBXntunZYBR6v'
    'RUKNbYRiDK8VQ42NhHotA+g1W4b4dyphFSQnOWXMLI6RrQHpCjCx'))




def sec_dns_query_midsave():
    # Session 10.9.27.1:49901 -> 10.9.27.2:53 (UDP):
    #   480 unique queries with no answers (plus one answer to the first) so
    #   the DNS field passes DNS_MAX_JSON_SIZE and the session is mid saved.
    #   Expect two session records.
    TS_START = 1700021700.0
    CMAC = bytes.fromhex('02aa00001b01')
    SMAC = bytes.fromhex('02aa00001b02')

    def csum(data):
        if len(data) & 1:
            data += b'\0'
        s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
        while s >> 16:
            s = (s & 0xffff) + (s >> 16)
        return (~s) & 0xffff

    def build_udp(src_str, dst_str, sport, dport, payload, smac, dmac):
        src = bytes(map(int, src_str.split('.')))
        dst = bytes(map(int, dst_str.split('.')))
        udp_len = 8 + len(payload)
        udp = struct.pack('!HHHH', sport, dport, udp_len, 0)
        pseudo = src + dst + struct.pack('!BBH', 0, 17, udp_len)
        udp = udp[:6] + struct.pack('!H', csum(pseudo + udp + payload))
        total_len = 20 + udp_len
        ip = struct.pack('!BBHHHBBH4s4s', 0x45, 0, total_len, 0x9101, 0, 64, 17, 0, src, dst)
        ip = ip[:10] + struct.pack('!H', csum(ip)) + ip[12:]
        return dmac + smac + b'\x08\x00' + ip + udp + payload

    def qname(name):
        return b''.join(bytes([len(p)]) + p.encode() for p in name.split('.')) + b'\0'

    def build():
        pkts = []
        for i in range(480):
            q = struct.pack('!HHHHHH', i + 1, 0x0100, 1, 0, 0, 0) + qname('q%04d.example.com' % i) + struct.pack('!HH', 1, 1)
            pkts.append(build_udp('10.9.27.1', '10.9.27.2', 49901, 53, q, CMAC, SMAC))
            if i == 0:
                a = struct.pack('!HHHHHH', 1, 0x8180, 1, 1, 0, 0) + qname('q0000.example.com') + struct.pack('!HH', 1, 1)
                a += b'\xc0\x0c' + struct.pack('!HHIH', 1, 1, 300, 4) + bytes([10, 9, 27, 100])
                pkts.append(build_udp('10.9.27.2', '10.9.27.1', 53, 49901, a, SMAC, CMAC))
        out = b''
        ts = TS_START
        for p in pkts:
            sec = int(ts)
            usec = int(round((ts - sec) * 1e6))
            out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
            ts += 0.01
        return out
    return build()


def sec_dns_bad_idna():
    # Session 10.9.58.1:49902 -> 10.9.58.2:53 (UDP):
    #   Query for xn--zzzzzzzzz.bad-idna.example, which isn't valid punycode, answered
    #   with a CNAME to xn--99999999.bad-idna.example (also invalid) and an A record.
    #   Expect the raw names, the A record ip and the bad-punycode tag.
    TS_START = 1700021800.0
    CMAC = bytes.fromhex('02aa00002801')
    SMAC = bytes.fromhex('02aa00002802')

    def csum(data):
        if len(data) & 1:
            data += b'\0'
        s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
        while s >> 16:
            s = (s & 0xffff) + (s >> 16)
        return (~s) & 0xffff

    def build_udp(src_str, dst_str, sport, dport, payload, smac, dmac):
        src = bytes(map(int, src_str.split('.')))
        dst = bytes(map(int, dst_str.split('.')))
        udp_len = 8 + len(payload)
        udp = struct.pack('!HHHH', sport, dport, udp_len, 0)
        pseudo = src + dst + struct.pack('!BBH', 0, 17, udp_len)
        udp = udp[:6] + struct.pack('!H', csum(pseudo + udp + payload))
        ip = struct.pack('!BBHHHBBH4s4s', 0x45, 0, 20 + udp_len, 0x9102, 0, 64, 17, 0, src, dst)
        ip = ip[:10] + struct.pack('!H', csum(ip)) + ip[12:]
        return dmac + smac + b'\x08\x00' + ip + udp + payload

    def qname(name):
        return b''.join(bytes([len(p)]) + p.encode() for p in name.split('.')) + b'\0'

    qn = qname('xn--zzzzzzzzz.bad-idna.example')
    cn = qname('xn--99999999.bad-idna.example')
    q = struct.pack('!HHHHHH', 0x4242, 0x0100, 1, 0, 0, 0) + qn + struct.pack('!HH', 1, 1)
    a = struct.pack('!HHHHHH', 0x4242, 0x8180, 1, 2, 0, 0) + qn + struct.pack('!HH', 1, 1)
    a += b'\xc0\x0c' + struct.pack('!HHIH', 5, 1, 300, len(cn)) + cn
    a += cn + struct.pack('!HHIH', 1, 1, 300, 4) + bytes([10, 9, 58, 100])
    pkts = [
        build_udp('10.9.58.1', '10.9.58.2', 49902, 53, q, CMAC, SMAC),
        build_udp('10.9.58.2', '10.9.58.1', 53, 49902, a, SMAC, CMAC),
    ]
    out = b''
    ts = TS_START
    for p in pkts:
        sec = int(ts)
        usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.01
    return out


def sec_dns_utf8_names():
    # Raw UTF-8 and non-UTF-8 bytes in DNS and mDNS names.
    # Session 10.9.59.1:5353 -> 224.0.0.251:5353 (mDNS query, 2 questions):
    #   PTR "Andy’s MacBook Pro._airplay._tcp.local" and PTR "_airplay._tcp.local".
    #   Expect the UTF-8 name kept (lowercased ASCII) and no dns:raw-utf8 or bad-hostname tag.
    # Session 10.9.59.2:5353 -> 224.0.0.251:5353 (mDNS response, no questions):
    #   _airplay._tcp.local PTR "Café Speaker._airplay._tcp.local".
    #   Expect the UTF-8 PTR name kept and no dns:raw-utf8 or bad-hostname tag.
    # Session 10.9.59.3:49903 -> 10.9.59.4:53 (DNS):
    #   A query for café.example.com sent as raw UTF-8, answered 10.9.59.100.
    #   Expect café.example.com and the dns:raw-utf8 tag.
    # Session 10.9.59.3:49904 -> 10.9.59.4:53 (DNS):
    #   A query for a name with an invalid UTF-8 byte (0xff) and a control character (0x01).
    #   Expect both still escaped cat -v style and no dns:raw-utf8 tag.
    TS_START = 1700021900.0
    MAC1 = bytes.fromhex('02aa00003b01')
    MAC2 = bytes.fromhex('02aa00003b02')
    MAC3 = bytes.fromhex('02aa00003b03')
    MAC4 = bytes.fromhex('02aa00003b04')
    MCAST = bytes.fromhex('01005e0000fb')

    def csum(data):
        if len(data) & 1:
            data += b'\0'
        s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
        while s >> 16:
            s = (s & 0xffff) + (s >> 16)
        return (~s) & 0xffff

    def build_udp(src_str, dst_str, sport, dport, payload, smac, dmac):
        src = bytes(map(int, src_str.split('.')))
        dst = bytes(map(int, dst_str.split('.')))
        udp_len = 8 + len(payload)
        udp = struct.pack('!HHHH', sport, dport, udp_len, 0)
        pseudo = src + dst + struct.pack('!BBH', 0, 17, udp_len)
        udp = udp[:6] + struct.pack('!H', csum(pseudo + udp + payload))
        ip = struct.pack('!BBHHHBBH4s4s', 0x45, 0, 20 + udp_len, 0x9103, 0, 255, 17, 0, src, dst)
        ip = ip[:10] + struct.pack('!H', csum(ip)) + ip[12:]
        return dmac + smac + b'\x08\x00' + ip + udp + payload

    def qname(labels):
        # labels are str (UTF-8 encoded) or raw bytes
        out = b''
        for label in labels:
            raw = label if isinstance(label, bytes) else label.encode('utf-8')
            out += bytes([len(raw)]) + raw
        return out + b'\0'

    PTR, A, IN = 12, 1, 1
    airplay = ['_airplay', '_tcp', 'local']

    mq = struct.pack('!HHHHHH', 0, 0, 2, 0, 0, 0)
    mq += qname(['Andy’s MacBook Pro'] + airplay) + struct.pack('!HH', PTR, IN)
    mq += qname(airplay) + struct.pack('!HH', PTR, IN)

    speaker = qname(['Café Speaker'] + airplay)
    mr = struct.pack('!HHHHHH', 0, 0x8400, 0, 1, 0, 0)
    mr += qname(airplay) + struct.pack('!HHIH', PTR, IN, 4500, len(speaker)) + speaker

    cafe = qname(['café', 'example', 'com'])
    dq = struct.pack('!HHHHHH', 0x5901, 0x0100, 1, 0, 0, 0) + cafe + struct.pack('!HH', A, IN)
    da = struct.pack('!HHHHHH', 0x5901, 0x8180, 1, 1, 0, 0) + cafe + struct.pack('!HH', A, IN)
    da += b'\xc0\x0c' + struct.pack('!HHIH', A, IN, 300, 4) + bytes([10, 9, 59, 100])

    bad = qname([b'bad\xffname', b'ctl\x01', 'example', 'com'])
    bq = struct.pack('!HHHHHH', 0x5902, 0x0100, 1, 0, 0, 0) + bad + struct.pack('!HH', A, IN)

    pkts = [
        build_udp('10.9.59.1', '224.0.0.251', 5353, 5353, mq, MAC1, MCAST),
        build_udp('10.9.59.2', '224.0.0.251', 5353, 5353, mr, MAC2, MCAST),
        build_udp('10.9.59.3', '10.9.59.4', 49903, 53, dq, MAC3, MAC4),
        build_udp('10.9.59.4', '10.9.59.3', 53, 49903, da, MAC4, MAC3),
        build_udp('10.9.59.3', '10.9.59.4', 49904, 53, bq, MAC3, MAC4),
    ]
    out = b''
    ts = TS_START
    for p in pkts:
        sec = int(ts)
        usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.01
    return out


def _max_ts(blob):
    # Latest timestamp among the pcap records in blob (header already stripped for sections)
    pos = 24 if blob[:4] in (b'\xd4\xc3\xb2\xa1', b'\xa1\xb2\xc3\xd4') else 0
    best = 0.0
    while pos + 16 <= len(blob):
        sec, usec, caplen, _ = struct.unpack('<IIII', blob[pos:pos + 16])
        best = max(best, sec + usec / 1e6)
        pos += 16 + caplen
    return best


def _after(blob, tmax):
    # Shift a section's records so they start after tmax, keeping their relative spacing.
    # Sections were written with independent clocks; without this the file runs backwards in
    # time and tools that expire sessions on the capture clock (Zeek, Arkime) tear every
    # session apart packet by packet.
    recs = []
    pos = 0
    while pos + 16 <= len(blob):
        sec, usec, caplen, wirelen = struct.unpack('<IIII', blob[pos:pos + 16])
        recs.append([sec, usec, blob[pos + 16:pos + 16 + caplen], wirelen])
        pos += 16 + caplen
    if not recs:
        return blob, tmax
    lowest = min(r[0] + r[1] / 1e6 for r in recs)
    shift = int(tmax) + 1 - int(lowest) if lowest <= tmax else 0
    out = b''
    for sec, usec, pkt, wirelen in recs:
        out += struct.pack('<IIII', sec + shift, usec, len(pkt), wirelen) + pkt
        tmax = max(tmax, sec + shift + usec / 1e6)
    return out, tmax


def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/dnssec_synthetic.pcap'
    out = LEGACY
    tmax = _max_ts(LEGACY)
    for blob in [
        sec_dns_query_midsave(),
        sec_dns_bad_idna(),
        sec_dns_utf8_names(),
    ]:
        blob, tmax = _after(blob, tmax)
        out += blob
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
