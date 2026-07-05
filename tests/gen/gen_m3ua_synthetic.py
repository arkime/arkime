#!/usr/bin/env python3
"""Regenerate tests/pcap/m3ua_synthetic.pcap in full.

Layout:
  - LEGACY blob: packets 1-60 (original m3ua/camel sessions)
  - m3ua_sctp_ooo (15 packets)

Run from the tests directory:  python3 gen/gen_m3ua_synthetic.py
Each section function below documents the session(s) it generates.
"""

import base64
import struct
import sys
import zlib

# Packets accumulated before per-session generator code existed
# (zlib-compressed pcap prefix, including the 24-byte global header)
LEGACY = zlib.decompress(base64.b64decode(
    'eNrdl21IU1EYgN97d910V2hTyywpNYtRfsz5rVPnR6F9KmrMGWqmJhkpJOqvEowUKtB/iRJK/pAJ'
    'EiX2YViRZJTSB/4ISfoRlP/6wEIRsvPqnYPLZdvZLmKdMTbuzp77nvc559z3zDy/288CB7a2sgLA'
    'kE/f27pzywd4yCLf8Q01zR3dVtCGxeYUe8NBgDjsZrpyxqRmGEbNsixv5s3Y8ativJ78FOAXd7Jl'
    'lacmL1DMI7EwwiUioSHVRpxM0uxnCbG9OjvUTlS9ReJAhDsxdvs/0xFEAIgaEn8SooF8N0gRwwVi'
    'vjjGxtFLXTwAh4ShSB6GybVhKULfGqFyRhzTE/2RU6CAVsyVEA7LgBJlNCgAzrYFQBX4gELFc9lV'
    'oOayA0B9vDI/l8NE1+3ThStVoL3qRah9QRWBrayqtd9HqeJQp4EZV+o4JVvAnA/u38EyGgLW+7Wq'
    'ukKzq9t9frSpum42FRk031fH7xNFldESvgQ7WiZ27UHr1rGpObsj/gESLVFU1gXiq9eN3Wh9bmrM'
    'aidu6UOi1a0Yg+YK+tA6I2pIXIqisi4QNRfDXtisN0fzUEmuVUoRigRCiDgmuLF8iFiPw1wJ1hUM'
    'MGg9nNVAEPl8SRR2egGKVGrJpFDbZup0NFUWLLxldZ7NLg6gKexqz6t/BxKD9VSmBOL7h5l5aCon'
    'NkxrJwY2ILFO706MtYu+RjTFihoS7+upTAnEx8bJDJupxBge8KJFinBUIOjEMSXkbU0hpiIwVyJT'
    'wcQU2UsY8htrxr2UYRWrd7LGUI2+lC/FjrOHO8fQUDH5hz2fO81I/BNDZUgg3hvx3oaGOppriu3E'
    '3SYkHjO4E+N8Qu8IGlKIGhJ7DVSGBOKHpZ5am6HRWB5GybVRKcKgsINOiWNqyTXWEkMdmCuRoRZi'
    '6ALaIW4WyG5q20gXGLKRmtm1jXRo+psf3UaqtW2kxdh+kY20BtvvtfXZE+f8OVA2pSZZU3Mct9me'
    'Ayfi/+XoB+Odz8HyXIwcRyD1FF/wmJCW4HxllSeJM+ioNrkuO3HONaJolI4qsqhEuYmXE90ZtaM6'
    '9I3sxKIk50//8u3rRBme/h83/I6pyVQrQqJGaveY8C6ZypwLlV9IitzE+hSqFeBCvftUdqIm1Z1R'
    'O6ryK2Qnfkp1XqeVh6wTPajTrhk36k6fjVQrQKJ6jUzzlNCURmXKhZp8QnaiJp1qxrtwEjktO/FO'
    'ujujdnT+4jLkJg5mOK+kyybWiZu0ks43/Q+jGDBRrVyJc9EXjwl7M6lmmAunvQbZiY8yqVaqC2dc'
    'VZbcxMIsd0bt6GR/S1biX/yapS0='))


def sec_m3ua_sctp_ooo():
    # Append SCTP out-of-order regression sessions to m3ua_synthetic.pcap
    # 
    # Both sessions are M3UA (SCTP ppid 3) carrying CAMEL initialDP messages whose
    # calling-party digits differ per message, so each delivered message is
    # visible in the session's camel.calling field.
    # 


    ETH = bytes.fromhex('00667788 99aa 0011 22334455'.replace(' ', '')) + b'\x08\x00'
    ETH_R = bytes.fromhex('00112233 4455 0066 778899aa'.replace(' ', '')) + b'\x08\x00'
    TS_START = 1781048077.0

    # M3UA TRANSFER/DATA + Protocol Data (SI=3 SCCP) + TCAP + CAMEL initialDP,
    # from existing frame 20; calling/called digit bytes are patched per message.
    M3UA_TEMPLATE = bytearray.fromhex(
        '01000101000000780210006e00000258000001f403000000'
        '090003070b0443f4010a044358020a4e62504804abcdef12'
        '6b262824060700118605010101a019601780020780a10906'
        '0704000001003201be062804060251016c1ca11a02011102'
        '01003012'
        '8007915555555555f5'   # calling party digits (offset 100, digits at 103..108)
        '8307916666666666f6'   # called party digits  (offset 109, digits at 112..117)
        '0000')

    CRC_TABLE = []
    for i in range(256):
        c = i
        for _ in range(8):
            c = (c >> 1) ^ 0x82F63B78 if c & 1 else c >> 1
        CRC_TABLE.append(c)


    def crc32c(data):
        c = 0xffffffff
        for b in data:
            c = CRC_TABLE[(c ^ b) & 0xff] ^ (c >> 8)
        return c ^ 0xffffffff


    def csum(data):
        if len(data) & 1:
            data += b'\0'
        s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
        while s >> 16:
            s = (s & 0xffff) + (s >> 16)
        return (~s) & 0xffff


    def m3ua_msg(digit):
        """M3UA message with calling/called digits set to the given digit."""
        m = bytearray(M3UA_TEMPLATE)
        d = (digit << 4) | digit
        for i in range(103, 108):
            m[i] = d
        m[108] = 0xf0 | digit
        for i in range(112, 117):
            m[i] = d
        m[117] = 0xf0 | digit
        return bytes(m)


    def sctp_packet(src, dst, chunks):
        sctp = struct.pack('>HHII', 2905, 2905, 0, 0)
        body = b''
        for c in chunks:
            body += c + b'\0' * ((4 - len(c) % 4) % 4)
        sctp = sctp[:8] + struct.pack('<I', 0) + body
        crc = crc32c(sctp[:8] + b'\0\0\0\0' + body)
        sctp = sctp[:8] + struct.pack('<I', crc) + body
        iplen = 20 + len(sctp)
        eth = ETH if src.startswith(('10.5', '10.7')) else ETH_R
        ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, iplen, 1, 0, 64, 132, 0,
                         bytes(map(int, src.split('.'))), bytes(map(int, dst.split('.'))))
        ip = ip[:10] + struct.pack('>H', csum(ip)) + ip[12:]
        return eth + ip + sctp


    def chunk_data(tsn, flags, payload, ppid=3):
        h = struct.pack('>BBHIHHI', 0, flags, 16 + len(payload), tsn, 0, 0, ppid)
        return h + payload


    def chunk_init(ctype, itag, itsn):
        return struct.pack('>BBHIIHHI', ctype, 0, 20, itag, 65536, 10, 10, itsn)


    def session(cli, srv, t, msgs):
        """msgs: list of (tsn, flags, payload)"""
        pkts = [
            sctp_packet(cli, srv, [chunk_init(1, 0x11223344, t)]),         # INIT
            sctp_packet(srv, cli, [chunk_init(2, 0x44332211, 9000)]),      # INIT_ACK
            sctp_packet(cli, srv, [struct.pack('>BBH', 10, 0, 8) + b'\xca\xfe\xca\xfe']),  # COOKIE_ECHO
            sctp_packet(srv, cli, [struct.pack('>BBH', 11, 0, 4)]),        # COOKIE_ACK
        ]
        for tsn, flags, payload in msgs:
            pkts.append(sctp_packet(cli, srv, [chunk_data(tsn, flags, payload)]))
        return pkts


    def build():
        B, E, BE = 0x02, 0x01, 0x03
        t = 5000

        msg_a = m3ua_msg(1)  # fragmented across t+1 (B) and t+2 (E)
        pkts = session('10.5.5.5', '10.6.6.6', t, [
            (t + 1, B, msg_a[:60]),
            (t + 2, E, msg_a[60:]),
            (t, BE, m3ua_msg(2)),
            (t + 3, BE, m3ua_msg(3)),
        ])
        pkts += session('10.7.7.7', '10.8.8.8', t, [
            (t + 1, BE, m3ua_msg(7)),
            (t, BE, m3ua_msg(8)),
            (t + 2, BE, m3ua_msg(9)),
        ])

        out = b''
        ts = TS_START
        for p in pkts:
            sec = int(ts)
            usec = int(round((ts - sec) * 1e6))
            out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
            ts += 0.05

        return out
    return build()


def sec_m2ua_pd2():
    # M2UA MAUP-DATA carrying Protocol Data 2 (tag 0x0301), which per
    # RFC 3331 starts with a 1-byte Length Indicator before the MTP2-user
    # message. Calling digits patched to 4s so the session is identifiable
    # (camel.calling == 444444444).
    # Regression for m3ua.c parsing PD2 like PD1 (LI octet read as the SIO).
    # Session 10.20.1.1:2906 -> 10.20.1.2:2906, SCTP ppid 2.
    import struct

    M2UA_PD2 = bytes.fromhex(
        '0100060100000070030100683f8314006400090003070b044364000a044314000a4e'
        '62504804123456786b262824060700118605010101a019601780020780a109060704'
        '000001003201be062804060251016c1ca11a02011002010030128007912143658709'
        'f18307914444444444f4')

    CRC_TABLE = []
    for i in range(256):
        c = i
        for _ in range(8):
            c = (c >> 1) ^ 0x82F63B78 if c & 1 else c >> 1
        CRC_TABLE.append(c)

    def crc32c(data):
        c = 0xffffffff
        for b in data:
            c = CRC_TABLE[(c ^ b) & 0xff] ^ (c >> 8)
        return c ^ 0xffffffff

    def csum(data):
        if len(data) & 1:
            data += b'\0'
        s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
        while s >> 16:
            s = (s & 0xffff) + (s >> 16)
        return (~s) & 0xffff

    ETH = bytes.fromhex('02aa000011020 2aa00001101'.replace(' ','')) + b'\x08\x00'
    ETH_R = bytes.fromhex('02aa000011010 2aa00001102'.replace(' ','')) + b'\x08\x00'

    def sctp_packet(src, dst, eth, vtag, chunks):
        sctp = struct.pack('>HHII', 2906, 2906, vtag, 0)
        body = b''
        for c in chunks:
            body += c + b'\0' * ((4 - len(c) % 4) % 4)
        crc = crc32c(sctp[:8] + b'\0\0\0\0' + body)
        sctp = sctp[:8] + struct.pack('<I', crc) + body
        iplen = 20 + len(sctp)
        ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, iplen, 1, 0, 64, 132, 0,
                         bytes(map(int, src.split('.'))), bytes(map(int, dst.split('.'))))
        ip = ip[:10] + struct.pack('>H', csum(ip)) + ip[12:]
        return eth + ip + sctp

    def chunk(ctype, flags, body):
        return struct.pack('>BBH', ctype, flags, 4+len(body)) + body

    def data_chunk(tsn, ppid, payload):
        return chunk(0, 0x03, struct.pack('>IHHI', tsn, 0, 0, ppid) + payload)

    CLI, SRV = '10.20.1.1', '10.20.1.2'
    pkts = [
        sctp_packet(CLI, SRV, ETH, 0, [chunk(1, 0, struct.pack('>IIHHI', 0x31313131, 65535, 4, 4, 100))]),
        sctp_packet(SRV, CLI, ETH_R, 0x31313131, [chunk(2, 0, struct.pack('>IIHHI', 0x32323232, 65535, 4, 4, 200))]),
        sctp_packet(CLI, SRV, ETH, 0x32323232, [data_chunk(100, 2, M2UA_PD2)]),
    ]

    out = b''
    ts = 1781049000.0
    for p in pkts:
        sec = int(ts)
        usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.05
    return out


def sec_m3ua_v6_pd1():
    # M3UA draft-v6 DATA carrying Protocol Data 1 (tag 0x0002), whose payload
    # is a raw MTP3 message (SIO + routing label), NOT the RFC 4666 structured
    # OPC/DPC/SI/NI/MP/SLS layout. Calling/called digits patched to 7s so the
    # session is identifiable (camel.calling == 777777777).
    # Regression for m3ua.c parsing tag 0x0002 with the RFC 4666 layout.
    # Session 10.20.2.1:2905 -> 10.20.2.2:2905, SCTP ppid 3.
    import struct

    # Raw MTP3: SIO 0x83 (NI=international spare, SI=3 SCCP) + ITU label,
    # then SCCP UDT + TCAP + CAMEL initialDP with 7s digits.
    MTP3 = bytes.fromhex(
        '8314006400090003070b044364000a044314000a4e'
        '62504804123456786b262824060700118605010101a019601780020780a109060704'
        '000001003201be062804060251016c1ca11a020110020100301280079177777777'
        '77f78307917777777777f7')

    m3uaLen = 8 + 4 + len(MTP3)
    pad = (4 - len(MTP3) % 4) % 4
    M3UA_V6_PD1 = (struct.pack('>BBBBI', 1, 0, 1, 1, m3uaLen + pad) +
                   struct.pack('>HH', 0x0002, 4 + len(MTP3)) + MTP3 + b'\0' * pad)

    CRC_TABLE = []
    for i in range(256):
        c = i
        for _ in range(8):
            c = (c >> 1) ^ 0x82F63B78 if c & 1 else c >> 1
        CRC_TABLE.append(c)

    def crc32c(data):
        c = 0xffffffff
        for b in data:
            c = CRC_TABLE[(c ^ b) & 0xff] ^ (c >> 8)
        return c ^ 0xffffffff

    def csum(data):
        if len(data) & 1:
            data += b'\0'
        s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
        while s >> 16:
            s = (s & 0xffff) + (s >> 16)
        return (~s) & 0xffff

    ETH = bytes.fromhex('02aa000012020 2aa00001201'.replace(' ','')) + b'\x08\x00'
    ETH_R = bytes.fromhex('02aa000012010 2aa00001202'.replace(' ','')) + b'\x08\x00'

    def sctp_packet(src, dst, eth, vtag, chunks):
        sctp = struct.pack('>HHII', 2905, 2905, vtag, 0)
        body = b''
        for c in chunks:
            body += c + b'\0' * ((4 - len(c) % 4) % 4)
        crc = crc32c(sctp[:8] + b'\0\0\0\0' + body)
        sctp = sctp[:8] + struct.pack('<I', crc) + body
        iplen = 20 + len(sctp)
        ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, iplen, 1, 0, 64, 132, 0,
                         bytes(map(int, src.split('.'))), bytes(map(int, dst.split('.'))))
        ip = ip[:10] + struct.pack('>H', csum(ip)) + ip[12:]
        return eth + ip + sctp

    def chunk(ctype, flags, body):
        return struct.pack('>BBH', ctype, flags, 4+len(body)) + body

    def data_chunk(tsn, ppid, payload):
        return chunk(0, 0x03, struct.pack('>IHHI', tsn, 0, 0, ppid) + payload)

    CLI, SRV = '10.20.2.1', '10.20.2.2'
    pkts = [
        sctp_packet(CLI, SRV, ETH, 0, [chunk(1, 0, struct.pack('>IIHHI', 0x41414141, 65535, 4, 4, 100))]),
        sctp_packet(SRV, CLI, ETH_R, 0x41414141, [chunk(2, 0, struct.pack('>IIHHI', 0x42424242, 65535, 4, 4, 200))]),
        sctp_packet(CLI, SRV, ETH, 0x42424242, [data_chunk(100, 3, M3UA_V6_PD1)]),
    ]

    out = b''
    ts = 1781049100.0
    for p in pkts:
        sec = int(ts)
        usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.05
    return out


def sec_m3ua_asp_handshake():
    # M3UA association captured from the start: ASPUP / ASPUP-ACK (class 3
    # ASPSM) are the first DATA chunks in each direction, followed by a
    # TRANSFER/DATA message (digits 8s, camel.calling == 888888888).
    # Regression for the classifier rejecting the one-shot SCTP classify
    # because the first chunk isn't TRANSFER/DATA, leaving the session
    # untagged and unparsed despite PPID 3.
    # Session 10.20.3.1:2905 -> 10.20.3.2:2905, SCTP ppid 3.
    import struct

    # ASPUP (class 3 type 1) and ASPUP-ACK (class 3 type 4), header only
    ASPUP = struct.pack('>BBBBI', 1, 0, 3, 1, 8)
    ASPUP_ACK = struct.pack('>BBBBI', 1, 0, 3, 4, 8)

    # RFC 4666 TRANSFER/DATA + Protocol Data 0x0210 with SI=3 SCCP, 8s digits
    SCCP = bytes.fromhex(
        '090003070b044364000a044314000a4e'
        '62504804123456786b262824060700118605010101a019601780020780a109060704'
        '000001003201be062804060251016c1ca11a020110020100301280079188888888'
        '88f88307918888888888f8')
    pd = struct.pack('>IIBBBB', 0x258, 0x1f4, 3, 0, 0, 0) + SCCP
    pdPad = (4 - len(pd) % 4) % 4
    m3uaLen = 8 + 4 + len(pd) + pdPad
    M3UA_DATA = (struct.pack('>BBBBI', 1, 0, 1, 1, m3uaLen) +
                 struct.pack('>HH', 0x0210, 4 + len(pd)) + pd + b'\0' * pdPad)

    CRC_TABLE = []
    for i in range(256):
        c = i
        for _ in range(8):
            c = (c >> 1) ^ 0x82F63B78 if c & 1 else c >> 1
        CRC_TABLE.append(c)

    def crc32c(data):
        c = 0xffffffff
        for b in data:
            c = CRC_TABLE[(c ^ b) & 0xff] ^ (c >> 8)
        return c ^ 0xffffffff

    def csum(data):
        if len(data) & 1:
            data += b'\0'
        s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
        while s >> 16:
            s = (s & 0xffff) + (s >> 16)
        return (~s) & 0xffff

    ETH = bytes.fromhex('02aa000013020 2aa00001301'.replace(' ','')) + b'\x08\x00'
    ETH_R = bytes.fromhex('02aa000013010 2aa00001302'.replace(' ','')) + b'\x08\x00'

    def sctp_packet(src, dst, eth, vtag, chunks):
        sctp = struct.pack('>HHII', 2905, 2905, vtag, 0)
        body = b''
        for c in chunks:
            body += c + b'\0' * ((4 - len(c) % 4) % 4)
        crc = crc32c(sctp[:8] + b'\0\0\0\0' + body)
        sctp = sctp[:8] + struct.pack('<I', crc) + body
        iplen = 20 + len(sctp)
        ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, iplen, 1, 0, 64, 132, 0,
                         bytes(map(int, src.split('.'))), bytes(map(int, dst.split('.'))))
        ip = ip[:10] + struct.pack('>H', csum(ip)) + ip[12:]
        return eth + ip + sctp

    def chunk(ctype, flags, body):
        return struct.pack('>BBH', ctype, flags, 4+len(body)) + body

    def data_chunk(tsn, ssn, ppid, payload):
        return chunk(0, 0x03, struct.pack('>IHHI', tsn, 0, ssn, ppid) + payload)

    CLI, SRV = '10.20.3.1', '10.20.3.2'
    pkts = [
        sctp_packet(CLI, SRV, ETH, 0, [chunk(1, 0, struct.pack('>IIHHI', 0x51515151, 65535, 4, 4, 100))]),
        sctp_packet(SRV, CLI, ETH_R, 0x51515151, [chunk(2, 0, struct.pack('>IIHHI', 0x52525252, 65535, 4, 4, 200))]),
        sctp_packet(CLI, SRV, ETH, 0x52525252, [data_chunk(100, 0, 3, ASPUP)]),
        sctp_packet(SRV, CLI, ETH_R, 0x51515151, [data_chunk(200, 0, 3, ASPUP_ACK)]),
        sctp_packet(CLI, SRV, ETH, 0x52525252, [data_chunk(101, 1, 3, M3UA_DATA)]),
    ]

    out = b''
    ts = 1781049200.0
    for p in pkts:
        sec = int(ts)
        usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.05
    return out


def sec_m3ua_mixed_si():
    # M3UA association carrying both ISUP (SI=5) and SCCP (SI=3) DATA
    # messages. Regression for the TUP/ISUP sub-parser unregistering the
    # whole m3ua parser after the first message, so the following SCCP/CAMEL
    # message was never parsed (camel.calling == 999999999).
    # Session 10.20.4.1:2905 -> 10.20.4.2:2905, SCTP ppid 3.
    import struct

    def m3ua_data(si, payload):
        pd = struct.pack('>IIBBBB', 0x258, 0x1f4, si, 0, 0, 0) + payload
        pad = (4 - len(pd) % 4) % 4
        mlen = 8 + 4 + len(pd) + pad
        return (struct.pack('>BBBBI', 1, 0, 1, 1, mlen) +
                struct.pack('>HH', 0x0210, 4 + len(pd)) + pd + b'\0' * pad)

    ISUP = bytes.fromhex('0001000a020100')  # minimal ISUP-ish bytes
    SCCP = bytes.fromhex(
        '090003070b044364000a044314000a4e'
        '62504804123456786b262824060700118605010101a019601780020780a109060704'
        '000001003201be062804060251016c1ca11a020110020100301280079199999999'
        '99f98307919999999999f9')

    MSG_ISUP = m3ua_data(5, ISUP)
    MSG_SCCP = m3ua_data(3, SCCP)

    CRC_TABLE = []
    for i in range(256):
        c = i
        for _ in range(8):
            c = (c >> 1) ^ 0x82F63B78 if c & 1 else c >> 1
        CRC_TABLE.append(c)

    def crc32c(data):
        c = 0xffffffff
        for b in data:
            c = CRC_TABLE[(c ^ b) & 0xff] ^ (c >> 8)
        return c ^ 0xffffffff

    def csum(data):
        if len(data) & 1:
            data += b'\0'
        s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
        while s >> 16:
            s = (s & 0xffff) + (s >> 16)
        return (~s) & 0xffff

    ETH = bytes.fromhex('02aa000014020 2aa00001401'.replace(' ','')) + b'\x08\x00'
    ETH_R = bytes.fromhex('02aa000014010 2aa00001402'.replace(' ','')) + b'\x08\x00'

    def sctp_packet(src, dst, eth, vtag, chunks):
        sctp = struct.pack('>HHII', 2905, 2905, vtag, 0)
        body = b''
        for c in chunks:
            body += c + b'\0' * ((4 - len(c) % 4) % 4)
        crc = crc32c(sctp[:8] + b'\0\0\0\0' + body)
        sctp = sctp[:8] + struct.pack('<I', crc) + body
        iplen = 20 + len(sctp)
        ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, iplen, 1, 0, 64, 132, 0,
                         bytes(map(int, src.split('.'))), bytes(map(int, dst.split('.'))))
        ip = ip[:10] + struct.pack('>H', csum(ip)) + ip[12:]
        return eth + ip + sctp

    def chunk(ctype, flags, body):
        return struct.pack('>BBH', ctype, flags, 4+len(body)) + body

    def data_chunk(tsn, ssn, ppid, payload):
        return chunk(0, 0x03, struct.pack('>IHHI', tsn, 0, ssn, ppid) + payload)

    CLI, SRV = '10.20.4.1', '10.20.4.2'
    pkts = [
        sctp_packet(CLI, SRV, ETH, 0, [chunk(1, 0, struct.pack('>IIHHI', 0x61616161, 65535, 4, 4, 100))]),
        sctp_packet(SRV, CLI, ETH_R, 0x61616161, [chunk(2, 0, struct.pack('>IIHHI', 0x62626262, 65535, 4, 4, 200))]),
        sctp_packet(CLI, SRV, ETH, 0x62626262, [data_chunk(100, 0, 3, MSG_ISUP)]),
        sctp_packet(CLI, SRV, ETH, 0x62626262, [data_chunk(101, 1, 3, MSG_SCCP)]),
    ]

    out = b''
    ts = 1781049300.0
    for p in pkts:
        sec = int(ts)
        usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.05
    return out


def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/m3ua_synthetic.pcap'
    out = LEGACY
    out += sec_m3ua_sctp_ooo()
    out += sec_m2ua_pd2()
    out += sec_m3ua_v6_pd1()
    out += sec_m3ua_asp_handshake()
    out += sec_m3ua_mixed_si()
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
