#!/usr/bin/env python3
"""Regenerate tests/pcap/arkime_synthetic.pcap in full.

Layout:
  - LEGACY blob: packets 1-708 accumulated before per-session generator code
    (multi-protocol coverage incl. the routed-CIP ENIP session)
  - icmpv6_haad (3 packets)
  - ikev2_sa_init (3 packets)
  - dns_https_empty_alpn (3 packets)
  - certs_keyusage (30 packets)
  - imap_crsplit (16 packets)
  - krb5_biglen (10 packets)
  - quic_zerotag (2 packets)
  - s7comm_frameclamp (8 packets)
  - sctp_interleave (5 packets)
  - smb1_dialect0 (8 packets)
  - ... (see main() for the full section list)
  - tls_256ext (8 packets)
  - dhcpv6_relay (6 packets)

Run from the tests directory:  python3 gen/gen_arkime_synthetic.py
Each section function below documents the session(s) it generates.
"""

import base64
import socket
import struct
import sys
import zlib

# Packets accumulated before per-session generator code existed
# (zlib-compressed pcap prefix, including the 24-byte global header)
LEGACY = zlib.decompress(base64.b64decode(
    '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'
    'Hovq8UPmsi/bpD/Yd/fE5+pmjF93JeP3V+7v/VnurFHzdpve7Ns2a8bBiXXtpgSmfL/u8obgXsmp'
    'k9cV9p6/M33XlYWT+8za8+n4H1/f9fTyKTuyQ4cJEoIrnz53McOwXzivckjGycaq0ICgxD0fJf9y'
    'dPocacDPW6LKJ10Nrw4P+fBSXeH3z59PnzCkV1DJnvcvzuua9sHW5obhz9xcvuq56ypx4YivOh2/'
    '+p1wUEnCjb2q9m1vjpV8fiDv8Z+mbJl/9g1hXqctnXV1324bs1H0fq8PZScj3+4WIV028Osfg27J'
    'd50ZdPt8j4cXP90mOHmVYuLKX/5Sx8yfN2HK8wcV0vCsL5853TD7rGjTzUHKoD6z/9g2bsm1f1/9'
    '5IFP39i9qmFIbofLjbd3inKefHL6hYPpp0rOnFD9XHTnvfBvpHeuvN8/u12iKHjr4YvagV8/seu1'
    'NFFBsPidqJ5FD2cLrlzZLJgZVXzQd9LQD5I3jN4x9r7HNnQWzjkk1JQM2d1PMm9Ow0CDcFf7D/6e'
    'NmrU1c2KfcNP5a69EXRkzBbToBfX9Gr73PwHTj/5cXq9eOLuiU8dHNDu9/0Hd/keyozt1rh3dkJQ'
    'cvLWpM8e+m74nlc7lw9sG6xIWbU4+uz8R2Ne1R0o+urnpHMPLr4iqi/J6T/y4V599kSfXDx664P6'
    'q+u2LXuj05KrPisb50/oPiRp3/2/j8rofPjCc2ulbW9unDphqjJSq8nrsPpE2kMLBw7vUxjaM2rl'
    '6YO3m94ccvXKyoHDt303dMHIZfvqVr+9aVBRzao/PpzmFx/QaeIC89yDCY/tHT0y8s93t/c759N9'
    'yGDh5lk1H47tlzNk+kz1xOkPPan7aHrwum9+XvXpgPbbxLuu7bpj+EP14M/Ld8lezPxTot+pfiNC'
    '+2xnNFu++f66Jc98N+rrbxe+X1SyVRL4wzOF32wM7nJ0X5vnun34al75DL+FinXzTh1X/1Ty9KoP'
    'fujj9/atkowX9zwb1DNjWY8HMnwGtV429mHKxvPOZeOxE0c5ZeOl+48OfUab+sSKnblPLikUffJR'
    '+zc/e70x+y2tX2efh6LLfz3TG68ZOv+Fp57rt++bz1ZvOPtXxN7BjxmTTssGf/rb9aKnxlT7T9Fu'
    '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'))


def sec_icmpv6_haad():
    # Append ICMPv6 Home Agent Address Discovery (HAAD, RFC 6275) sessions to
    # arkime_synthetic.pcap, exercising the type 144/145 community-id port mapping
    # in capture/db.c arkime_db_community_id_icmp().
    # 
    # Sessions:
    #   1  2001:db8:90::1 -> 2001:db8:90::2  HAAD Request (144) then Reply (145 back)

    TS_START = 1700009000.0

    CMAC = bytes.fromhex('001122334455')
    SMAC = bytes.fromhex('66778899aabb')


    def csum(data):
        if len(data) & 1:
            data += b'\0'
        s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
        while s >> 16:
            s = (s & 0xffff) + (s >> 16)
        return (~s) & 0xffff


    def haad_request(ident):
        # Type(144) Code(0) Checksum(2) Identifier(2) Reserved(2)
        return struct.pack('!BBHHH', 144, 0, 0, ident, 0)


    def haad_reply(ident, home_agent):
        # Type(145) Code(0) Checksum(2) Identifier(2) Reserved(2) + HA address(16)
        return struct.pack('!BBHHH', 145, 0, 0, ident, 0) + home_agent


    def build_icmpv6(src_str, dst_str, payload):
        src = socket.inet_pton(socket.AF_INET6, src_str)
        dst = socket.inet_pton(socket.AF_INET6, dst_str)
        # IPv6 pseudo-header: src(16) + dst(16) + len(4) + zeros(3) + nxt(1)=58
        pseudo = src + dst + struct.pack('!I', len(payload)) + b'\x00\x00\x00\x3a'
        payload = payload[:2] + struct.pack('!H', csum(pseudo + payload)) + payload[4:]

        ip = struct.pack('!IHBB',
                         (6 << 28),      # version=6, tc=0, flow=0
                         len(payload),
                         58,             # next header = ICMPv6
                         64)             # hop limit
        ip += src + dst

        eth = SMAC + CMAC + b'\x86\xdd'  # linktype 1 = Ethernet
        return eth + ip + payload


    def build():
        pkts = []

        # Session 1: request/reply pair
        ha1 = socket.inet_pton(socket.AF_INET6, '2001:db8:90::2')
        pkts.append(build_icmpv6('2001:db8:90::1', '2001:db8:90::2', haad_request(0x1234)))
        pkts.append(build_icmpv6('2001:db8:90::2', '2001:db8:90::1', haad_reply(0x1234, ha1)))

        # Session 2: lone reply, session starts with type 145
        ha2 = socket.inet_pton(socket.AF_INET6, '2001:db8:90::4')
        pkts.append(build_icmpv6('2001:db8:90::3', '2001:db8:90::4', haad_reply(0x5678, ha2)))

        out = b''
        ts = TS_START
        for p in pkts:
            sec = int(ts)
            usec = int(round((ts - sec) * 1e6))
            out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
            ts += 0.1

        return out
    return build()


def sec_ikev2_sa_init():
    # Append an IKEv2 IKE_SA_INIT session to arkime_synthetic.pcap exercising
    # two isakmp.c fixes:
    #   1. Initiator-side payloads are parsed (IKEv2 flag 0x08 is Initiator, not
    #      "encrypted"; only non-SA_INIT exchanges are encrypted).
    #   2. IKEv2 ENCR transform IDs use the v2 IANA table (12=aes-cbc, 20=aes-gcm-16,
    #      28=chacha20-poly1305, ...), not the IKEv1 attribute table.

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


    def build():
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

        return out
    return build()


def sec_dns_https_empty_alpn():
    # Append a DNS HTTPS-RR session with an EMPTY alpn SvcParam to
    # arkime_synthetic.pcap, exercising the dns.c save-path fix where an empty
    # SVCB value list made the unconditional comma-rewind eat the '=' from
    # "alpn="/"ipv4hint="/"ipv6hint=" in dns.answers.https.
    # 
    # Session 10.0.91.1:41000 -> 10.0.91.2:53 (UDP):

    TS_START = 1700009200.0

    CMAC = bytes.fromhex('001122334455')
    SMAC = bytes.fromhex('66778899aabb')

    QNAME = b'svc-empty-alpn' + b'\x07example\x03com\x00'


    def csum(data):
        if len(data) & 1:
            data += b'\0'
        s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
        while s >> 16:
            s = (s & 0xffff) + (s >> 16)
        return (~s) & 0xffff


    def dns_query():
        hdr = struct.pack('!HHHHHH', 0x9120, 0x0100, 1, 0, 0, 0)
        qname = bytes([14]) + QNAME
        return hdr + qname + struct.pack('!HH', 65, 1)  # type HTTPS, class IN


    def dns_response():
        hdr = struct.pack('!HHHHHH', 0x9120, 0x8180, 1, 1, 0, 0)
        qname = bytes([14]) + QNAME
        question = qname + struct.pack('!HH', 65, 1)
        # RDATA: priority 1, target "." (root), then SvcParams
        rdata = struct.pack('!H', 1) + b'\x00'
        rdata += struct.pack('!HH', 1, 0)                          # alpn, EMPTY
        rdata += struct.pack('!HHH', 3, 2, 443)                    # port=443
        rdata += struct.pack('!HH', 4, 4) + bytes([192, 0, 2, 1])  # ipv4hint
        answer = b'\xc0\x0c' + struct.pack('!HHIH', 65, 1, 300, len(rdata)) + rdata
        return hdr + question + answer


    def build_udp(src_str, dst_str, sport, dport, payload):
        src = bytes(map(int, src_str.split('.')))
        dst = bytes(map(int, dst_str.split('.')))
        udp_len = 8 + len(payload)
        udp = struct.pack('!HHHH', sport, dport, udp_len, 0)
        pseudo = src + dst + struct.pack('!BBH', 0, 17, udp_len)
        udp = udp[:6] + struct.pack('!H', csum(pseudo + udp + payload))

        total_len = 20 + udp_len
        ip = struct.pack('!BBHHHBBH4s4s', 0x45, 0, total_len, 0x9100, 0, 64, 17,
                         0, src, dst)
        ip = ip[:10] + struct.pack('!H', csum(ip)) + ip[12:]

        eth = SMAC + CMAC + b'\x08\x00'
        return eth + ip + udp + payload


    def build():
        q = dns_query()
        r = dns_response()
        pkts = [
            build_udp('10.0.91.1', '10.0.91.2', 41000, 53, q),
            build_udp('10.0.91.1', '10.0.91.2', 41000, 53, q),  # retransmit, parsed
            build_udp('10.0.91.2', '10.0.91.1', 53, 41000, r),
        ]

        out = b''
        ts = TS_START
        for p in pkts:
            sec = int(ts)
            usec = int(round((ts - sec) * 1e6))
            out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
            ts += 0.1

        return out
    return build()


def sec_certs_keyusage():
    # Append two TLS sessions with special keyUsage certificates to
    # tests/pcap/arkime_synthetic.pcap (Ethernet linktype).
    # 
    # Regression for certs.c certinfo_key_usage(): isCA must test the
    # keyCertSign bit (0x04), not cRLSign (0x02).
    # 


    CLI_IP = '10.9.1.1'
    SRV_IP = '10.9.1.2'
    SRV_PORT = 443
    TS_START = 1700001000.0

    CLI_MAC = bytes.fromhex('02aa00000901')
    SRV_MAC = bytes.fromhex('02aa00000902')

    # subjectAltName (alt.example.test) BEFORE keyUsage = keyCertSign only
    CERT_SAN_THEN_CERTSIGN = bytes.fromhex(
        '308201e43082014da003020102020411223344300d06092a864886f70d01010b'
        '0500301e311c301a06035504030c1373616e2d6265666f72652d6b6579757361'
        '6765301e170d3234303130313030303030305a170d3334303130313030303030'
        '305a301e311c301a06035504030c1373616e2d6265666f72652d6b6579757361'
        '676530819f300d06092a864886f70d010101050003818d0030818902818100d0'
        'ea4a40a365455a9cc89453abd70723f9c02b4572c9e51f4614a862c942724696'
        '671ca4e9360d52790187a37034cd1eb06caa0074e4458fcccc4b489bd633932b'
        'f9faa2f6f52e0616727070b3d6bfca4e04637e0c5ce862d61de82fac3d4ab37d'
        '901ce4a7245a1a6324161e0a58814469b78e9113236883e376e706bbdc92b302'
        '03010001a32f302d301b0603551d11041430128210616c742e6578616d706c65'
        '2e74657374300e0603551d0f0101ff040403020204300d06092a864886f70d01'
        '010b05000381810054f821cddd71009f4fa0bb250229acfad7fb8ee0e7dac4e0'
        '8671da3734228b20c400a7101fd0b104f7d414d1cbf70ee170e6cb50a3f9b55e'
        '64a0390033689fd607797125ade043e10e31ae75fa8fdacf5eef9700b466ef27'
        '70257242b871f7a002b3a9e25f1263d5c540a30937cc55bca032b89426789d7c'
        '32a2362061e7a7d6')

    # keyUsage extension = keyCertSign only (bits 03 02 02 04)
    CERT_CERTSIGN = bytes.fromhex(
        '308201cd30820136a003020102020411223344300d06092a864886f70d01010b'
        '05003021311f301d06035504030c166b657975736167652d636572747369676e'
        '2d6f6e6c79301e170d3234303130313030303030305a170d3334303130313030'
        '303030305a3021311f301d06035504030c166b657975736167652d6365727473'
        '69676e2d6f6e6c7930819f300d06092a864886f70d010101050003818d003081'
        '8902818100eb98abe939bd743e4c7d67b643dec2f594fba8a69a17e6478c2647'
        '8a280775bf5d118da5908e70998a2cfe038f269523507d8b1d1ae18a10376485'
        '4d24ba453ca014ebfcba241be6da6ece7003c1e231b56c61e4aad412061e52de'
        'fd28eaf4cda8359616abdb167569da42622895ba6d7561cd83ad44f6123bb5a5'
        '4703af9d130203010001a3123010300e0603551d0f0101ff040403020204300d'
        '06092a864886f70d01010b05000381810055336dfc8ea0c2fddb17658ae5ecc9'
        '3469dbd62cb0479b13ca9476fca0c9d208df015506542cd53293976339514e45'
        '429f2c489a3d9c1ee65ec9d3e3b7699546f4d473e4190aea97842c3c719ca455'
        '75356f74af5553a1298aab878a8f9da0ecfa1f946e8d9708302122068dfd32cf'
        'a6bf41ef8a1182bbe5473b813c73728f6d')

    # keyUsage extension = cRLSign only (bits 03 02 01 02)
    CERT_CRLSIGN = bytes.fromhex(
        '308201cb30820134a003020102020411223344300d06092a864886f70d01010b'
        '05003020311e301c06035504030c156b657975736167652d63726c7369676e2d'
        '6f6e6c79301e170d3234303130313030303030305a170d333430313031303030'
        '3030305a3020311e301c06035504030c156b657975736167652d63726c736967'
        '6e2d6f6e6c7930819f300d06092a864886f70d010101050003818d0030818902'
        '818100eb98abe939bd743e4c7d67b643dec2f594fba8a69a17e6478c26478a28'
        '0775bf5d118da5908e70998a2cfe038f269523507d8b1d1ae18a103764854d24'
        'ba453ca014ebfcba241be6da6ece7003c1e231b56c61e4aad412061e52defd28'
        'eaf4cda8359616abdb167569da42622895ba6d7561cd83ad44f6123bb5a54703'
        'af9d130203010001a3123010300e0603551d0f0101ff040403020102300d0609'
        '2a864886f70d01010b0500038181001c5e5aca69e68cd5c4976f49d609ef9399'
        '33a2db1a490854d7df8881f9111211077d88a19a64e590679d20a4c41e93437c'
        '5a6d2ea3bfd1dc2472d24f5e23c14737eac4ca16d33f437f9bffb8cf035b68f3'
        'af7f0e12b6fcfe2b5694889528dc5f70dee01ea9bdb4ac54e7bd9dbc62919b2b'
        '2a79736a14b9813e8a72d0253f9d90')


    def tls_rec(ctype, body):
        return struct.pack('>BHH', ctype, 0x0303, len(body)) + body


    def hs_msg(htype, body):
        return struct.pack('>B', htype) + struct.pack('>I', len(body))[1:] + body


    def client_hello():
        body = struct.pack('>H', 0x0303) + bytes(32)          # version + random
        body += b'\x00'                                       # session id len
        body += struct.pack('>H', 2) + struct.pack('>H', 0x002f)  # 1 cipher
        body += b'\x01\x00'                                   # compression null
        return tls_rec(0x16, hs_msg(1, body))


    def server_hello():
        body = struct.pack('>H', 0x0303) + bytes(32)
        body += b'\x00'
        body += struct.pack('>H', 0x002f) + b'\x00'
        return tls_rec(0x16, hs_msg(2, body))


    def certificate(cert):
        entry = struct.pack('>I', len(cert))[1:] + cert
        chain = struct.pack('>I', len(entry))[1:] + entry
        return tls_rec(0x16, hs_msg(11, chain))


    def server_hello_done():
        return tls_rec(0x16, hs_msg(14, b''))


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


    def tls_session(pkts, cport, cert):
        cseq, sseq = 0x1000, 0x2000

        def c(flags, payload=b''):
            nonlocal cseq
            pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, cport, SRV_PORT,
                                   cseq, sseq, flags, payload))
            cseq += len(payload)

        def s(flags, payload=b''):
            nonlocal sseq
            pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, cport,
                                   sseq, cseq, flags, payload))
            sseq += len(payload)

        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, cport, SRV_PORT,
                               cseq, 0, 0x02))                    # SYN
        cseq += 1
        pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, cport,
                               sseq, cseq, 0x12))                 # SYN-ACK
        sseq += 1
        c(0x10)                                                   # ACK
        c(0x18, client_hello())
        s(0x10)                                                   # ACK
        s(0x18, server_hello() + certificate(cert) + server_hello_done())
        c(0x10)                                                   # ACK
        c(0x11)                                                   # FIN
        sseq += 0
        s(0x11)                                                   # FIN
        cseq += 1
        sseq += 1
        c(0x10)                                                   # final ACK


    def build():
        pkts = []
        tls_session(pkts, 49301, CERT_CERTSIGN)
        tls_session(pkts, 49302, CERT_CRLSIGN)
        tls_session(pkts, 49303, CERT_SAN_THEN_CERTSIGN)

        out = b''
        ts = TS_START
        for p in pkts:
            sec = int(ts)
            usec = int(round((ts - sec) * 1e6))
            out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
            ts += 0.05

        return out
    return build()


def sec_imap_crsplit():
    # Append an IMAP CR/LF-split regression session to arkime_synthetic.pcap.
    # 
    # Session 10.9.2.1:49401 -> 10.9.2.2:143 (Ethernet linktype):
    #   Server FETCH response where lines are split between the \r and the \n
    #   across TCP segments:
    #     - "Subject: CrSplitCheck\r"  ends segment 1, "\n" starts segment 2


    CLI_IP = '10.9.2.1'
    SRV_IP = '10.9.2.2'
    CLI_PORT = 49401
    SRV_PORT = 143
    TS_START = 1700002000.0

    CLI_MAC = bytes.fromhex('02aa00000a01')
    SRV_MAC = bytes.fromhex('02aa00000a02')


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


    def build():
        pkts = []
        cseq, sseq = 0x1000, 0x2000

        def c(flags, payload=b''):
            nonlocal cseq
            pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                                   cseq, sseq, flags, payload))
            cseq += len(payload)

        def s(flags, payload=b''):
            nonlocal sseq
            pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT,
                                   sseq, cseq, flags, payload))
            sseq += len(payload)

        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                               cseq, 0, 0x02))                     # SYN
        cseq += 1
        pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT,
                               sseq, cseq, 0x12))                  # SYN-ACK
        sseq += 1
        c(0x10)                                                    # ACK

        s(0x18, b'* OK IMAP4rev1 Service Ready\r\n')
        c(0x18, b'a1 SELECT "INBOX"\r\n')
        s(0x18, b'a1 OK SELECT completed\r\n')
        c(0x18, b'a2 FETCH 1 (BODY[HEADER])\r\n')

        # FETCH response split between \r and \n across segments
        s(0x18, b'* 1 FETCH (BODY[HEADER] {90}\r\n'
                b'From: alice@crsplit.test\r\n'
                b'Subject: CrSplitCheck\r')                        # ends mid-CRLF
        c(0x10)
        s(0x18, b'\n\r')                                           # finishes subject, starts blank line
        c(0x10)
        s(0x18, b'\nTo: should-not-parse@after-blank.test\r\n'
                b')\r\n'
                b'a2 OK FETCH completed\r\n')
        c(0x10)

        c(0x11)                                                    # FIN
        cseq += 1
        s(0x11)                                                    # FIN
        sseq += 1
        c(0x10)

        out = b''
        ts = TS_START
        for p in pkts:
            sec = int(ts)
            usec = int(round((ts - sec) * 1e6))
            out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
            ts += 0.05

        return out
    return build()


def sec_krb5_biglen():
    # Append a Kerberos-TCP big-record regression session to arkime_synthetic.pcap.
    # 
    # Session 10.9.3.1:49501 -> 10.9.3.2:88 (Ethernet linktype):
    #   Record 1: real 269-byte AS-REQ (classifies + parses normally)
    #   Record 2: 4-byte RFC 4120 length prefix 0x00010100 (65792) followed by a
    #             truncated AS-REQ whose realm was patched to


    CLI_IP = '10.9.3.1'
    SRV_IP = '10.9.3.2'
    CLI_PORT = 49501
    SRV_PORT = 88
    TS_START = 1700003000.0

    CLI_MAC = bytes.fromhex('02aa00000b01')
    SRV_MAC = bytes.fromhex('02aa00000b02')

    ASREQ_RECORD = bytes.fromhex('0000010d6a82010930820105a103020105a20302010aa31530133011a10402020080a20904073005a0030101ffa481e13081dea00703050040810010a1193017a003020101a110300e1b0c787878787878787878787824a21f1b1d78787878787878787878787878782e7878787878782e7878782e434f4da3323030a003020102a12930271b066b72627467741b1d78787878787878787878787878782e7878787878782e7878782e434f4da511180f32303337303931333032343830355aa611180f32303337303931333032343830355aa706020434eb6deba81630140201120201170202ff7b0201800201180202ff79a91d301b3019a003020114a112041078787878787878787878782020202020')
    FAKE_RECORD = bytes.fromhex('000101006a82010930820105a103020105a20302010aa31530133011a10402020080a20904073005a0030101ffa481e13081dea00703050040810010a1193017a003020101a110300e1b0c787878787878787878787824a21f1b1d444553594e43444553594e4344452e53594e4345442e4241442e434f4da3323030a003020102a12930271b066b72627467741b1d444553594e43444553594e4344452e53594e4345442e4241442e434f4da511180f32303337303931333032343830355aa611180f32303337303931333032343830355aa706020434eb6deba81630140201120201170202ff7b0201800201180202ff79a91d301b3019a003020114a112041078787878787878')


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


    def build():
        pkts = []
        cseq, sseq = 0x1000, 0x2000

        def c(flags, payload=b''):
            nonlocal cseq
            pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                                   cseq, sseq, flags, payload))
            cseq += len(payload)

        def s(flags, payload=b''):
            nonlocal sseq
            pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT,
                                   sseq, cseq, flags, payload))
            sseq += len(payload)

        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                               cseq, 0, 0x02))                     # SYN
        cseq += 1
        pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT,
                               sseq, cseq, 0x12))                  # SYN-ACK
        sseq += 1
        c(0x10)                                                    # ACK

        c(0x18, ASREQ_RECORD)     # real AS-REQ, classifies the session
        s(0x10)
        c(0x18, FAKE_RECORD)      # 65792-byte record (truncated), DESYNC realm inside
        s(0x10)

        c(0x11)                                                    # FIN
        cseq += 1
        s(0x11)                                                    # FIN
        sseq += 1
        c(0x10)

        out = b''
        ts = TS_START
        for p in pkts:
            sec = int(ts)
            usec = int(round((ts - sec) * 1e6))
            out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
            ts += 0.05

        return out
    return build()


def sec_quic_zerotag():
    # Append a gQUIC Q043 zero-length-tag regression session to arkime_synthetic.pcap.
    # 
    # Session 10.9.4.1:52001 -> 10.9.4.2:443 UDP (Ethernet linktype):
    #   A Q043 CHLO whose tag table starts with a ZERO-LENGTH PAD value
    #   (endOffset == start == 0) followed by SNI (zerolen.example) and VER.
    # 


    CLI_IP = '10.9.4.1'
    SRV_IP = '10.9.4.2'
    CLI_PORT = 52001
    SRV_PORT = 443
    TS_START = 1700004000.0

    CLI_MAC = bytes.fromhex('02aa00000c01')
    SRV_MAC = bytes.fromhex('02aa00000c02')

    QUIC_CHLO = bytes.fromhex('0d11223344556677885130343301000000000000000000000000800143484c4f030000005041440000000000534e49000f00000056455200130000007a65726f6c656e2e6578616d706c655130343300000000000000000000000000000000000000000000000000000000000000')


    def csum(data):
        if len(data) & 1:
            data += b'\0'
        s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
        while s >> 16:
            s = (s & 0xffff) + (s >> 16)
        return (~s) & 0xffff


    def eth_ip_udp(src, dst, smac, dmac, sport, dport, payload=b''):
        udplen = 8 + len(payload)
        iplen = 20 + udplen
        ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, iplen, 1, 0, 64, 17, 0,
                         bytes(map(int, src.split('.'))), bytes(map(int, dst.split('.'))))
        ip = ip[:10] + struct.pack('>H', csum(ip)) + ip[12:]
        udp = struct.pack('>HHHH', sport, dport, udplen, 0)
        pseudo = ip[12:20] + struct.pack('>BBH', 0, 17, udplen)
        ck = csum(pseudo + udp + payload)
        udp = udp[:6] + struct.pack('>H', ck if ck else 0xffff)
        return dmac + smac + b'\x08\x00' + ip + udp + payload


    def build():
        pkts = []
        # First datagram classifies, second is parsed (UDP classify-then-parse rule)
        pkts.append(eth_ip_udp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT, QUIC_CHLO))
        pkts.append(eth_ip_udp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT, QUIC_CHLO))

        out = b''
        ts = TS_START
        for p in pkts:
            sec = int(ts)
            usec = int(round((ts - sec) * 1e6))
            out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
            ts += 0.5

        return out
    return build()


def sec_s7comm_frameclamp():
    # Append an S7comm frame-clamp regression session to arkime_synthetic.pcap.
    # 
    # Session 10.9.5.1:49601 -> 10.9.5.2:102 (Ethernet linktype), one client
    # segment containing TWO TPKT frames:
    #   Frame 1: 8-byte DT frame whose S7 payload is ONLY the protocol-id byte 0x32
    #   Frame 2: complete S7comm Job Read-Var (rosctr=1, pduRef=1, func=0x04)


    CLI_IP = '10.9.5.1'
    SRV_IP = '10.9.5.2'
    CLI_PORT = 49601
    SRV_PORT = 102
    TS_START = 1700005000.0

    CLI_MAC = bytes.fromhex('02aa00000d01')
    SRV_MAC = bytes.fromhex('02aa00000d02')

    S7_FRAMES = bytes.fromhex('0300000802f080320300001f02f080320100000001000e00000401120a10020001000184000000')


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


    def build():
        pkts = []
        cseq, sseq = 0x1000, 0x2000

        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                               cseq, 0, 0x02))                     # SYN
        cseq += 1
        pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT,
                               sseq, cseq, 0x12))                  # SYN-ACK
        sseq += 1
        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                               cseq, sseq, 0x10))                  # ACK
        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                               cseq, sseq, 0x18, S7_FRAMES))       # both TPKT frames
        cseq += len(S7_FRAMES)
        pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT,
                               sseq, cseq, 0x10))                  # ACK
        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                               cseq, sseq, 0x11))                  # FIN
        cseq += 1
        pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT,
                               sseq, cseq, 0x11))                  # FIN
        sseq += 1
        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                               cseq, sseq, 0x10))                  # ACK

        out = b''
        ts = TS_START
        for p in pkts:
            sec = int(ts)
            usec = int(round((ts - sec) * 1e6))
            out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
            ts += 0.05

        return out
    return build()


def sec_sctp_interleave():
    # Append an SCTP multi-stream interleaved-fragment regression session to
    # arkime_synthetic.pcap (Ethernet linktype).
    # 
    # Session 10.9.6.1:38001 -> 10.9.6.2:38002, IP proto 132 (SCTP):
    #   INIT/INIT-ACK pin the initial TSNs, then the client sends DATA chunks
    #   out of order so they all queue:


    CLI_IP = '10.9.6.1'
    SRV_IP = '10.9.6.2'
    CLI_PORT = 38001
    SRV_PORT = 38002
    TS_START = 1700006000.0

    CLI_MAC = bytes.fromhex('02aa00000e01')
    SRV_MAC = bytes.fromhex('02aa00000e02')

    CLI_TAG = 0x11110001
    SRV_TAG = 0x22220002


    def _crc32c_table():
        poly = 0x82F63B78
        table = []
        for i in range(256):
            c = i
            for _ in range(8):
                c = (c >> 1) ^ poly if c & 1 else c >> 1
            table.append(c)
        return table


    _TABLE = _crc32c_table()


    def crc32c(data):
        c = 0xFFFFFFFF
        for b in data:
            c = _TABLE[(c ^ b) & 0xFF] ^ (c >> 8)
        return c ^ 0xFFFFFFFF


    def csum(data):
        if len(data) & 1:
            data += b'\0'
        s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
        while s >> 16:
            s = (s & 0xffff) + (s >> 16)
        return (~s) & 0xffff


    def sctp_packet(src, dst, smac, dmac, sport, dport, vtag, chunks):
        sctp = struct.pack('>HHII', sport, dport, vtag, 0) + chunks
        ck = crc32c(sctp)
        sctp = sctp[:8] + struct.pack('<I', ck) + sctp[12:]
        iplen = 20 + len(sctp)
        ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, iplen, 1, 0, 64, 132, 0,
                         bytes(map(int, src.split('.'))), bytes(map(int, dst.split('.'))))
        ip = ip[:10] + struct.pack('>H', csum(ip)) + ip[12:]
        return dmac + smac + b'\x08\x00' + ip + sctp


    def chunk(ctype, flags, body):
        clen = 4 + len(body)
        pad = (4 - (clen & 3)) & 3
        return struct.pack('>BBH', ctype, flags, clen) + body + b'\x00' * pad


    def data_chunk(flags, tsn, stream, sseq, ppid, payload):
        return chunk(0, flags, struct.pack('>IHHI', tsn, stream, sseq, ppid) + payload)


    def init_chunk(tag, tsn):
        return chunk(1, 0, struct.pack('>IIHHI', tag, 65535, 4, 4, tsn))


    def init_ack_chunk(tag, tsn):
        return chunk(2, 0, struct.pack('>IIHHI', tag, 65535, 4, 4, tsn))


    def build():
        B, E = 0x02, 0x01
        pkts = [
            sctp_packet(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT, 0,
                        init_chunk(CLI_TAG, 10)),
            sctp_packet(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT, CLI_TAG,
                        init_ack_chunk(SRV_TAG, 20)),
            # out-of-order: complete stream-2 message first (queues; TSN gap at 10)
            sctp_packet(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT, SRV_TAG,
                        data_chunk(B | E, 11, 2, 0, 0, b'CCCC')),
            # first fragment of stream-1 message
            sctp_packet(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT, SRV_TAG,
                        data_chunk(B, 10, 1, 0, 0, b'AAAA')),
            # last fragment of stream-1 message
            sctp_packet(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT, SRV_TAG,
                        data_chunk(E, 12, 1, 0, 0, b'BBBB')),
        ]

        out = b''
        ts = TS_START
        for p in pkts:
            sec = int(ts)
            usec = int(round((ts - sec) * 1e6))
            out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
            ts += 0.05

        return out
    return build()


def sec_smb1_dialect0():
    # Append an SMB1 NEGOTIATE dialect-index-0 regression session to
    # arkime_synthetic.pcap (Ethernet linktype).
    # 
    # Session 10.9.7.1:49701 -> 10.9.7.2:445:
    #   Client NEGOTIATE offers a single dialect "NT LM 0.12";
    #   server response (wordcount 17) selects DialectIndex 0.


    CLI_IP = '10.9.7.1'
    SRV_IP = '10.9.7.2'
    CLI_PORT = 49701
    SRV_PORT = 445
    TS_START = 1700007000.0

    CLI_MAC = bytes.fromhex('02aa00000f01')
    SRV_MAC = bytes.fromhex('02aa00000f02')

    SMB_REQ = bytes.fromhex('0000002fff534d4272000000000000000000000000000000000000000000000000000000000c00024e54204c4d20302e313200')
    SMB_RSP = bytes.fromhex('00000045ff534d427200000000800000000000000000000000000000000000000000000011000000000000000000000000000000000000000000000000000000000000000000000000')


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


    def build():
        pkts = []
        cseq, sseq = 0x1000, 0x2000

        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                               cseq, 0, 0x02))
        cseq += 1
        pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT,
                               sseq, cseq, 0x12))
        sseq += 1
        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                               cseq, sseq, 0x10))
        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                               cseq, sseq, 0x18, SMB_REQ))
        cseq += len(SMB_REQ)
        pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT,
                               sseq, cseq, 0x18, SMB_RSP))
        sseq += len(SMB_RSP)
        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                               cseq, sseq, 0x11))
        cseq += 1
        pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT,
                               sseq, cseq, 0x11))
        sseq += 1
        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                               cseq, sseq, 0x10))

        out = b''
        ts = TS_START
        for p in pkts:
            sec = int(ts)
            usec = int(round((ts - sec) * 1e6))
            out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
            ts += 0.05

        return out
    return build()


def sec_smb1_malformed_delete():
    # SMB1 DELETE with a bogus wordcount (22) whose implied parameter skip
    # (22*2+3 = 47 bytes) runs past the message end and lands 4 bytes into
    # the NEXT message's filename, followed by a legit DELETE of
    # "realfile.txt".
    #
    # Session 10.9.7.3:49702 -> 10.9.7.4:445, both messages in one segment.
    #
    # Regression for smb.c parsing commands from the whole buffered stream:
    # the old code captured the cross-message string "file.txt" from the
    # malformed message and desynced; the message-clamped parser extracts
    # nothing from it and parses the second message normally.

    CLI_IP = '10.9.7.3'
    SRV_IP = '10.9.7.4'
    CLI_PORT = 49702
    SRV_PORT = 445
    TS_START = 1700007100.0

    CLI_MAC = bytes.fromhex('02aa00000f03')
    SRV_MAC = bytes.fromhex('02aa00000f04')

    # malformed DELETE (wordcount 22) + legit DELETE (realfile.txt)
    SMB_MSGS = bytes.fromhex(
        '00000022ff534d420600000000000000000000000000000000000000000000000000'
        '0000160000000033ff534d4206000000000000000000000000000000000000000000'
        '0000000000000100000e00047265616c66696c652e74787400')

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

    def build():
        pkts = []
        cseq, sseq = 0x1000, 0x2000

        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                               cseq, 0, 0x02))
        cseq += 1
        pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT,
                               sseq, cseq, 0x12))
        sseq += 1
        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                               cseq, sseq, 0x10))
        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                               cseq, sseq, 0x18, SMB_MSGS))
        cseq += len(SMB_MSGS)
        pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT,
                               sseq, cseq, 0x10))
        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                               cseq, sseq, 0x11))
        cseq += 1
        pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT,
                               sseq, cseq, 0x11))
        sseq += 1
        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                               cseq, sseq, 0x10))

        out = b''
        ts = TS_START
        for p in pkts:
            sec = int(ts)
            usec = int(round((ts - sec) * 1e6))
            out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
            ts += 0.05
        return out
    return build()


def sec_nbns_response():
    # NBNS query + positive name response in ONE session
    # (10.20.8.10:49200 <-> 10.20.8.1:137). Regression for nbns_udp_classify
    # never parsing the response direction: nbns.host/nbns.ip stayed empty
    # for normal query->response sessions (host TESTHOST, ip 10.20.8.77).
    import struct

    QUERY = bytes.fromhex('beef011000010000000000002046454546464446454549455046444645434143414341434143414341434141410000200001')
    RESP = bytes.fromhex('beef8580000000010000000020464545464644464545494550464446454341434143414341434143414341414100002000010000012c000600000a14084d')

    CLI_IP = '10.20.8.10'
    SRV_IP = '10.20.8.1'
    CLI_PORT = 49200
    SRV_PORT = 137
    TS_START = 1700009300.0
    CLI_MAC = bytes.fromhex('02aa00001201')
    SRV_MAC = bytes.fromhex('02aa00001202')

    def csum(data):
        if len(data) & 1:
            data += b'\0'
        s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
        while s >> 16:
            s = (s & 0xffff) + (s >> 16)
        return (~s) & 0xffff

    def eth_ip_udp(src, dst, smac, dmac, sport, dport, payload):
        udplen = 8 + len(payload)
        iplen = 20 + udplen
        ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, iplen, 1, 0, 64, 17, 0,
                         bytes(map(int, src.split('.'))), bytes(map(int, dst.split('.'))))
        ip = ip[:10] + struct.pack('>H', csum(ip)) + ip[12:]
        udp = struct.pack('>HHHH', sport, dport, udplen, 0)
        pseudo = ip[12:20] + struct.pack('>BBH', 0, 17, udplen)
        ck = csum(pseudo + udp + payload)
        udp = udp[:6] + struct.pack('>H', ck if ck else 0xffff)
        return dmac + smac + b'\x08\x00' + ip + udp + payload

    pkts = [
        eth_ip_udp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT, QUERY),
        eth_ip_udp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT, RESP),
    ]

    out = b''
    ts = TS_START
    for p in pkts:
        sec = int(ts)
        usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.05
    return out


def sec_ssh_kexinit_overrun():
    # SSH session whose client KEXINIT has a corrupt final name-list length
    # that runs past the record end, with a NEWKEYS record buffered in the
    # same TCP segment. Regression for ssh.c passing all buffered bytes
    # instead of the record length to the KEXINIT parse: hassh was computed
    # over bytes of the following record.
    # Session 10.9.7.1:49407 -> 10.9.7.2:22 (Ethernet linktype).

    CLI_IP = '10.9.7.1'
    SRV_IP = '10.9.7.2'
    CLI_PORT = 49407
    SRV_PORT = 22
    TS_START = 1700007000.0

    CLI_MAC = bytes.fromhex('02aa00000f01')
    SRV_MAC = bytes.fromhex('02aa00000f02')

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

    def nl(s):
        return struct.pack('>I', len(s)) + s

    def ssh_record(code, payload):
        # packet_length(4) + padding_length(1) + code(1) + payload + padding
        padlen = 4
        body = bytes([padlen, code]) + payload + b'\0' * padlen
        return struct.pack('>I', len(body)) + body

    # KEXINIT payload: cookie + name-lists. The comp_c2s list (which is part
    # of the client hassh) claims 12 bytes but only 8 remain in the record
    # (4 content + 4 padding), so a parse using buffered bytes pulls 4 bytes
    # of the following NEWKEYS record into the hash.
    kexpay = (b'\x00' * 16 +
              nl(b'curve25519-sha256') +          # kex (hashed)
              nl(b'ssh-ed25519') +                # host key
              nl(b'aes128-ctr') +                 # enc c2s (hashed)
              nl(b'aes128-ctr') +                 # enc s2c
              nl(b'hmac-sha2-256') +              # mac c2s (hashed)
              nl(b'hmac-sha2-256') +              # mac s2c
              struct.pack('>I', 12) + b'none')    # comp c2s: length overruns record
    kexinit = ssh_record(20, kexpay)
    # Follow-on bytes: under buffered-length parsing, comp_c2s (len 12) pulls
    # the first 4 bytes and comp_s2c reads len 0 from the next 4, so a hassh
    # polluted with cross-record bytes is stored
    follow = bytes.fromhex('0000000C00000000')

    def build():
        pkts = []
        cseq, sseq = 0x1000, 0x2000

        def c(flags, payload=b''):
            nonlocal cseq
            pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                                   cseq, sseq, flags, payload))
            cseq += len(payload)

        def s(flags, payload=b''):
            nonlocal sseq
            pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT,
                                   sseq, cseq, flags, payload))
            sseq += len(payload)

        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                               cseq, 0, 0x02))                     # SYN
        cseq += 1
        pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT,
                               sseq, cseq, 0x12))                  # SYN-ACK
        sseq += 1
        c(0x10)                                                    # ACK

        c(0x18, b'SSH-2.0-hasshtest\r\n')
        s(0x18, b'SSH-2.0-testserver\r\n')
        # Corrupt KEXINIT and follow-on record bytes in one segment
        c(0x18, kexinit + follow)
        s(0x10)

        c(0x11)                                                    # FIN
        cseq += 1
        s(0x11)                                                    # FIN
        sseq += 1
        c(0x10)

        out = b''
        ts = TS_START
        for p in pkts:
            sec = int(ts)
            usec = int(round((ts - sec) * 1e6))
            out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
            ts += 0.05

        return out
    return build()


def sec_udp_zero_ulen():
    # UDP session whose second packet claims uh_ulen == 0 (malformed, less
    # than the 8-byte UDP header). Regression for udp.c udp_pre_process
    # adding MIN(ulen, payload) - 8 == -8 to the uint64 databytes counter,
    # wrapping it to ~1.8e19.
    # Session 10.9.8.1:40100 -> 10.9.8.2:9999 (Ethernet linktype).

    CLI_IP = '10.9.8.1'
    SRV_IP = '10.9.8.2'
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

    def udp_pkt(payload, ulen=None):
        if ulen is None:
            ulen = 8 + len(payload)
        udp = struct.pack('>HHHH', 40100, 9999, ulen, 0) + payload
        iplen = 20 + len(udp)
        ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, iplen, 1, 0, 64, 17, 0,
                         bytes(map(int, CLI_IP.split('.'))), bytes(map(int, SRV_IP.split('.'))))
        ip = ip[:10] + struct.pack('>H', csum(ip)) + ip[12:]
        return SRV_MAC + CLI_MAC + b'\x08\x00' + ip + udp

    pkts = [
        udp_pkt(b'udplenokpayload!'),           # valid: 16 databytes
        udp_pkt(b'udplenokpayload!', ulen=0),   # malformed ulen
    ]

    out = b''
    ts = TS_START
    for p in pkts:
        sec = int(ts)
        usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.05
    return out


def sec_websocket_split():
    # WebSocket session exercising frames that span TCP segments:
    #   - masked Text frame split across two segments (was: sample lost)
    #   - fragmented Text message with an interleaved Ping (was: the
    #     continuation frame inherited the Ping opcode and lost its sample)
    #   - server Close frame split across two segments (was: code/reason lost)
    # Session 10.9.9.1:49500 -> 10.9.9.2:80 (Ethernet linktype).

    CLI_IP = '10.9.9.1'
    SRV_IP = '10.9.9.2'
    CLI_PORT = 49500
    SRV_PORT = 80
    TS_START = 1700009000.0

    CLI_MAC = bytes.fromhex('02aa00001101')
    SRV_MAC = bytes.fromhex('02aa00001102')

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

    def ws_frame(fin, opcode, payload, mask=None):
        b0 = (0x80 if fin else 0) | opcode
        b1 = len(payload) | (0x80 if mask else 0)
        out = bytes([b0, b1])
        if mask:
            out += mask + bytes(payload[i] ^ mask[i & 3] for i in range(len(payload)))
        else:
            out += payload
        return out

    MASK = bytes.fromhex('11223344')
    split_text = ws_frame(1, 1, b'SplitFrameSample', MASK)
    frag1 = ws_frame(0, 1, b'FragHeadPart', MASK)
    ping = ws_frame(1, 9, b'', MASK)
    cont = ws_frame(1, 0, b'FragTailPart', MASK)
    close = ws_frame(1, 8, struct.pack('>H', 1000) + b'bye-split')

    def build():
        pkts = []
        cseq, sseq = 0x1000, 0x2000

        def c(flags, payload=b''):
            nonlocal cseq
            pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                                   cseq, sseq, flags, payload))
            cseq += len(payload)

        def s(flags, payload=b''):
            nonlocal sseq
            pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT,
                                   sseq, cseq, flags, payload))
            sseq += len(payload)

        pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, CLI_PORT, SRV_PORT,
                               cseq, 0, 0x02))                     # SYN
        cseq += 1
        pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, SRV_PORT, CLI_PORT,
                               sseq, cseq, 0x12))                  # SYN-ACK
        sseq += 1
        c(0x10)                                                    # ACK

        c(0x18, b'GET /chat HTTP/1.1\r\n'
                b'Host: ws.split.test\r\n'
                b'Upgrade: websocket\r\n'
                b'Connection: Upgrade\r\n'
                b'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n'
                b'Sec-WebSocket-Version: 13\r\n\r\n')
        s(0x18, b'HTTP/1.1 101 Switching Protocols\r\n'
                b'Upgrade: websocket\r\n'
                b'Connection: Upgrade\r\n'
                b'Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n')
        c(0x10)

        # Text frame split mid-payload across two segments
        c(0x18, split_text[:14])
        s(0x10)
        c(0x18, split_text[14:])
        s(0x10)

        # Fragmented text message with interleaved ping
        c(0x18, frag1 + ping + cont)
        s(0x10)

        # Server close frame split mid-reason across two segments
        s(0x18, close[:6])
        c(0x10)
        s(0x18, close[6:])
        c(0x10)

        c(0x11)                                                    # FIN
        cseq += 1
        s(0x11)                                                    # FIN
        sseq += 1
        c(0x10)

        out = b''
        ts = TS_START
        for p in pkts:
            sec = int(ts)
            usec = int(round((ts - sec) * 1e6))
            out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
            ts += 0.05

        return out
    return build()


def sec_tls_ja3_edge():
    # TLS sessions with degenerate hellos:
    #   S1 10.9.10.1: server hello extensions block of 1 garbage byte - the
    #      truncated header import appended a bogus "0" (fake SNI) to the
    #      JA3S extension list
    #   S2 10.9.11.1: client hello whose extensions are all GREASE - the
    #      trailing-dash rewind on the empty list error-flagged the BSB and
    #      suppressed JA3 entirely
    #   S3 10.9.12.1: client hello with an all-GREASE supported-groups list
    #      and an empty ec-point-formats list - same empty-rewind suppression
    # (Ethernet linktype, port 443.)

    TS_START = 1700010000.0

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

    def ext(etype, body):
        return struct.pack('>HH', etype, len(body)) + body

    def client_hello(ciphers, exts):
        body = (b'\x03\x03' + bytes(range(32)) + b'\x00' +
                struct.pack('>H', 2 * len(ciphers)) + b''.join(struct.pack('>H', c) for c in ciphers) +
                b'\x01\x00' +
                struct.pack('>H', len(exts)) + exts)
        hs = b'\x01' + len(body).to_bytes(3, 'big') + body
        return b'\x16\x03\x01' + struct.pack('>H', len(hs)) + hs

    def server_hello(cipher, extbytes):
        body = (b'\x03\x03' + bytes(range(32, 64)) + b'\x00' +
                struct.pack('>H', cipher) + b'\x00' + extbytes)
        hs = b'\x02' + len(body).to_bytes(3, 'big') + body
        return b'\x16\x03\x03' + struct.pack('>H', len(hs)) + hs

    CIPHERS = [0x1301, 0x1302]
    sni = ext(0, b'\x00\x0f\x00\x00\x0cja3edge.test')

    sessions = [
        # S1: normal client, server ext block = length 1 + garbage byte
        ('10.9.10.1', '10.9.10.2', 49601, bytes.fromhex('02aa00001201'), bytes.fromhex('02aa00001202'),
         client_hello(CIPHERS, sni), server_hello(0x1301, b'\x00\x01\x00')),
        # S2: client extensions all GREASE
        ('10.9.11.1', '10.9.11.2', 49602, bytes.fromhex('02aa00001301'), bytes.fromhex('02aa00001302'),
         client_hello(CIPHERS, ext(0x0a0a, b'') + ext(0x1a1a, b'')), server_hello(0x1301, b'')),
        # S3: client supported-groups all GREASE + empty ec-point-formats
        ('10.9.12.1', '10.9.12.2', 49603, bytes.fromhex('02aa00001401'), bytes.fromhex('02aa00001402'),
         client_hello(CIPHERS, ext(0x000a, b'\x00\x02\x0a\x0a') + ext(0x000b, b'\x00')),
         server_hello(0x1301, b'')),
    ]

    out = b''
    ts = TS_START
    for cli, srv, cport, cmac, smac, chello, shello in sessions:
        pkts = []
        cseq, sseq = 0x1000, 0x2000
        pkts.append(eth_ip_tcp(cli, srv, cmac, smac, cport, 443, cseq, 0, 0x02))
        cseq += 1
        pkts.append(eth_ip_tcp(srv, cli, smac, cmac, 443, cport, sseq, cseq, 0x12))
        sseq += 1
        pkts.append(eth_ip_tcp(cli, srv, cmac, smac, cport, 443, cseq, sseq, 0x10))
        pkts.append(eth_ip_tcp(cli, srv, cmac, smac, cport, 443, cseq, sseq, 0x18, chello))
        cseq += len(chello)
        pkts.append(eth_ip_tcp(srv, cli, smac, cmac, 443, cport, sseq, cseq, 0x18, shello))
        sseq += len(shello)
        pkts.append(eth_ip_tcp(cli, srv, cmac, smac, cport, 443, cseq, sseq, 0x11))
        cseq += 1
        pkts.append(eth_ip_tcp(srv, cli, smac, cmac, 443, cport, sseq, cseq, 0x11))
        sseq += 1
        pkts.append(eth_ip_tcp(cli, srv, cmac, smac, cport, 443, cseq, sseq, 0x10))

        for p in pkts:
            sec = int(ts)
            usec = int(round((ts - sec) * 1e6))
            out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
            ts += 0.05
    return out


def sec_modbus_edge():
    # Modbus edge cases:
    #   S1 10.9.13.1: server exception response with malformed modbusLen == 2
    #      immediately followed by a valid frame in the same segment - the
    #      exception code was read from the next frame's transactionId byte
    #   S2 10.9.14.x: mid-stream capture whose first packet is the server
    #      (port1 == 502) - requests/responses were labeled by raw direction,
    #      recording the exception function code with the 0x80 bit set
    # (Ethernet linktype.)

    TS_START = 1700011000.0

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

    def mbap(tid, unit, pdu, mlen=None):
        if mlen is None:
            mlen = 1 + len(pdu)
        return struct.pack('>HHHB', tid, 0, mlen, unit) + pdu

    out = b''
    ts = TS_START

    # --- S1: normal direction, malformed short exception frame ---
    CLI, SRV = '10.9.13.1', '10.9.13.2'
    CMAC, SMAC = bytes.fromhex('02aa00001501'), bytes.fromhex('02aa00001502')
    pkts = []
    cseq, sseq = 0x1000, 0x2000
    pkts.append(eth_ip_tcp(CLI, SRV, CMAC, SMAC, 49700, 502, cseq, 0, 0x02)); cseq += 1
    pkts.append(eth_ip_tcp(SRV, CLI, SMAC, CMAC, 502, 49700, sseq, cseq, 0x12)); sseq += 1
    pkts.append(eth_ip_tcp(CLI, SRV, CMAC, SMAC, 49700, 502, cseq, sseq, 0x10))
    req = mbap(0x0101, 1, b'\x03\x00\x00\x00\x01')
    pkts.append(eth_ip_tcp(CLI, SRV, CMAC, SMAC, 49700, 502, cseq, sseq, 0x18, req)); cseq += len(req)
    # malformed: mlen 2 (unit+func only) with exception bit, then valid response
    bad = mbap(0x0101, 1, b'\x81', mlen=2)
    good = mbap(0x0102, 1, b'\x03\x02\x12\x34')
    pkts.append(eth_ip_tcp(SRV, CLI, SMAC, CMAC, 502, 49700, sseq, cseq, 0x18, bad + good)); sseq += len(bad + good)
    pkts.append(eth_ip_tcp(CLI, SRV, CMAC, SMAC, 49700, 502, cseq, sseq, 0x11)); cseq += 1
    pkts.append(eth_ip_tcp(SRV, CLI, SMAC, CMAC, 502, 49700, sseq, cseq, 0x11)); sseq += 1
    pkts.append(eth_ip_tcp(CLI, SRV, CMAC, SMAC, 49700, 502, cseq, sseq, 0x10))
    for p in pkts:
        sec = int(ts); usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.05

    # --- S2: connection initiated from the server side (addr1 = port 502),
    #     e.g. active-open or the first captured direction being the server ---
    SRV2, CLI2 = '10.9.14.2', '10.9.14.1'
    S2MAC, C2MAC = bytes.fromhex('02aa00001602'), bytes.fromhex('02aa00001601')
    pkts = []
    sseq, cseq = 0x3000, 0x4000
    pkts.append(eth_ip_tcp(SRV2, CLI2, S2MAC, C2MAC, 502, 49701, sseq, 0, 0x02)); sseq += 1
    pkts.append(eth_ip_tcp(CLI2, SRV2, C2MAC, S2MAC, 49701, 502, cseq, sseq, 0x12)); cseq += 1
    pkts.append(eth_ip_tcp(SRV2, CLI2, S2MAC, C2MAC, 502, 49701, sseq, cseq, 0x10))
    # server exception response seen first: tid 0x0201, func 0x83, code 2
    exc = mbap(0x0201, 1, b'\x83\x02')
    pkts.append(eth_ip_tcp(SRV2, CLI2, S2MAC, C2MAC, 502, 49701, sseq, cseq, 0x18, exc)); sseq += len(exc)
    # client request: tid 0x0202, func 4
    req2 = mbap(0x0202, 1, b'\x04\x00\x10\x00\x02')
    pkts.append(eth_ip_tcp(CLI2, SRV2, C2MAC, S2MAC, 49701, 502, cseq, sseq, 0x18, req2)); cseq += len(req2)
    pkts.append(eth_ip_tcp(SRV2, CLI2, S2MAC, C2MAC, 502, 49701, sseq, cseq, 0x11)); sseq += 1
    pkts.append(eth_ip_tcp(CLI2, SRV2, C2MAC, S2MAC, 49701, 502, cseq, sseq, 0x11)); cseq += 1
    pkts.append(eth_ip_tcp(SRV2, CLI2, S2MAC, C2MAC, 502, 49701, sseq, cseq, 0x10))
    for p in pkts:
        sec = int(ts); usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.05

    return out


def sec_oracle_big_connect():
    # Oracle TNS CONNECT whose connect-data is > 255 bytes, so the 16-bit
    # connect-data length/offset high bytes are nonzero. Regression for
    # oracle.c reading only the low bytes (data[25]/data[27]): such sessions
    # were never classified.
    # Session 10.9.15.1:49800 -> 10.9.15.2:1521 (Ethernet linktype).

    CLI_IP = '10.9.15.1'
    SRV_IP = '10.9.15.2'
    TS_START = 1700012000.0

    CLI_MAC = bytes.fromhex('02aa00001701')
    SRV_MAC = bytes.fromhex('02aa00001702')

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

    pad = '(PAD=' + 'x' * 200 + ')'
    cdata = ('(DESCRIPTION=(CONNECT_DATA=(SERVICE_NAME=bigsvc.example.test)'
             '(CID=(PROGRAM=sqlplus)(USER=bigoracleuser))' + pad +
             ')(ADDRESS=(PROTOCOL=TCP)(HOST=bighost.example.test)(PORT=1521)))').encode()
    assert len(cdata) > 255
    cdoff = 34
    tnslen = cdoff + len(cdata)
    hdr = struct.pack('>HHBBH', tnslen, 0, 1, 0, 0)          # len, csum, type=CONNECT, rsvd, hdrcsum
    body = struct.pack('>HHHHHHHHHH', 0x0138, 0x012c, 0, 2048, 32767, 0x4f98, 0, 1, len(cdata), cdoff)
    body += struct.pack('>IH', 0, 0)                         # max recv, flags
    connect = hdr + body + cdata
    assert len(connect) == tnslen

    pkts = []
    cseq, sseq = 0x1000, 0x2000
    pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, 49800, 1521, cseq, 0, 0x02)); cseq += 1
    pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, 1521, 49800, sseq, cseq, 0x12)); sseq += 1
    pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, 49800, 1521, cseq, sseq, 0x10))
    pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, 49800, 1521, cseq, sseq, 0x18, connect)); cseq += len(connect)
    pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, 1521, 49800, sseq, cseq, 0x10))
    pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, 49800, 1521, cseq, sseq, 0x11)); cseq += 1
    pkts.append(eth_ip_tcp(SRV_IP, CLI_IP, SRV_MAC, CLI_MAC, 1521, 49800, sseq, cseq, 0x11)); sseq += 1
    pkts.append(eth_ip_tcp(CLI_IP, SRV_IP, CLI_MAC, SRV_MAC, 49800, 1521, cseq, sseq, 0x10))

    out = b''
    ts = TS_START
    for p in pkts:
        sec = int(ts); usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.05
    return out


def sec_sip_radius_edge():
    # S1 10.9.16.x TCP 5060: session opening with "OPTIONS sip:" (was: TCP
    #    classifier missing OPTIONS prefix, only classified via the server's
    #    SIP/2.0 response so the request methods were lost) and a later
    #    PUBLISH request (was: not in sip_is_method, never recorded)
    # S2 10.9.17.x UDP 1812: RADIUS datagram with 2 trailing padding bytes
    #    (was: classifier required exact length, never classified)
    # S3 10.9.18.x UDP 1812: RADIUS with a malformed Framed-IP attribute
    #    before valid attributes (was: attribute walk aborted, dropping them)

    TS_START = 1700013000.0

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

    def eth_ip_udp(src, dst, smac, dmac, sport, dport, payload):
        udp = struct.pack('>HHHH', sport, dport, 8 + len(payload), 0) + payload
        iplen = 20 + len(udp)
        ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, iplen, 1, 0, 64, 17, 0,
                         bytes(map(int, src.split('.'))), bytes(map(int, dst.split('.'))))
        ip = ip[:10] + struct.pack('>H', csum(ip)) + ip[12:]
        return dmac + smac + b'\x08\x00' + ip + udp

    out = b''
    ts = TS_START

    # --- S1: SIP over TCP ---
    CLI, SRV = '10.9.16.1', '10.9.16.2'
    CMAC, SMAC = bytes.fromhex('02aa00001801'), bytes.fromhex('02aa00001802')
    options = (b'OPTIONS sip:probe@edge.test SIP/2.0\r\n'
               b'Via: SIP/2.0/TCP 10.9.16.1:5060\r\n'
               b'From: <sip:watcher@edge.test>\r\n'
               b'To: <sip:probe@edge.test>\r\n'
               b'Call-ID: opt1@edge.test\r\n'
               b'CSeq: 1 OPTIONS\r\n'
               b'Content-Length: 0\r\n\r\n')
    ok200 = (b'SIP/2.0 200 OK\r\n'
             b'Via: SIP/2.0/TCP 10.9.16.1:5060\r\n'
             b'From: <sip:watcher@edge.test>\r\n'
             b'To: <sip:probe@edge.test>\r\n'
             b'Call-ID: opt1@edge.test\r\n'
             b'CSeq: 1 OPTIONS\r\n'
             b'Content-Length: 0\r\n\r\n')
    publish = (b'PUBLISH sip:watcher@edge.test SIP/2.0\r\n'
               b'Via: SIP/2.0/TCP 10.9.16.1:5060\r\n'
               b'From: <sip:watcher@edge.test>\r\n'
               b'To: <sip:watcher@edge.test>\r\n'
               b'Call-ID: pub1@edge.test\r\n'
               b'CSeq: 2 PUBLISH\r\n'
               b'Event: presence\r\n'
               b'Content-Length: 0\r\n\r\n')
    pkts = []
    cseq, sseq = 0x1000, 0x2000
    pkts.append(eth_ip_tcp(CLI, SRV, CMAC, SMAC, 5060, 5060, cseq, 0, 0x02)); cseq += 1
    pkts.append(eth_ip_tcp(SRV, CLI, SMAC, CMAC, 5060, 5060, sseq, cseq, 0x12)); sseq += 1
    pkts.append(eth_ip_tcp(CLI, SRV, CMAC, SMAC, 5060, 5060, cseq, sseq, 0x10))
    pkts.append(eth_ip_tcp(CLI, SRV, CMAC, SMAC, 5060, 5060, cseq, sseq, 0x18, options)); cseq += len(options)
    pkts.append(eth_ip_tcp(SRV, CLI, SMAC, CMAC, 5060, 5060, sseq, cseq, 0x18, ok200)); sseq += len(ok200)
    pkts.append(eth_ip_tcp(CLI, SRV, CMAC, SMAC, 5060, 5060, cseq, sseq, 0x18, publish)); cseq += len(publish)
    pkts.append(eth_ip_tcp(SRV, CLI, SMAC, CMAC, 5060, 5060, sseq, cseq, 0x10))
    pkts.append(eth_ip_tcp(CLI, SRV, CMAC, SMAC, 5060, 5060, cseq, sseq, 0x11)); cseq += 1
    pkts.append(eth_ip_tcp(SRV, CLI, SMAC, CMAC, 5060, 5060, sseq, cseq, 0x11)); sseq += 1
    pkts.append(eth_ip_tcp(CLI, SRV, CMAC, SMAC, 5060, 5060, cseq, sseq, 0x10))
    for p in pkts:
        sec = int(ts); usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.05

    def attr(t, val):
        return bytes([t, len(val) + 2]) + val

    def radius(attrs, pad=0):
        body = b''.join(attrs)
        rlen = 20 + len(body)
        return struct.pack('>BBH', 1, 1, rlen) + bytes(16) + body + b'\0' * pad

    # --- S2: padded RADIUS ---
    r2 = radius([attr(1, b'paduser'),
                 attr(4, bytes(map(int, '10.9.17.2'.split('.')))),
                 attr(8, bytes(map(int, '192.168.50.5'.split('.'))))], pad=2)
    for _ in range(2):
        p = eth_ip_udp('10.9.17.1', '10.9.17.2', bytes.fromhex('02aa00001901'),
                       bytes.fromhex('02aa00001902'), 50100, 1812, r2)
        sec = int(ts); usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.05

    # --- S3: malformed Framed-IP attr (6-byte value) before valid attrs ---
    r3 = radius([attr(8, bytes(map(int, '192.168.60.6'.split('.'))) + b'\0\0'),
                 attr(4, bytes(map(int, '10.9.18.2'.split('.')))),
                 attr(1, b'badattruser')])
    for _ in range(2):
        p = eth_ip_udp('10.9.18.1', '10.9.18.2', bytes.fromhex('02aa00001a01'),
                       bytes.fromhex('02aa00001a02'), 50101, 1812, r3)
        sec = int(ts); usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.05

    return out


def sec_tls_256ext():
    # TLS session whose ServerHello carries exactly 256 non-GREASE extensions
    # (types 0x0000-0x00ff, all empty).  Exercises the ja4plus JA4S extension
    # hashing buffer: with tmpBuf[5*256] the 256th "%04x," snprintf lacked NUL
    # space, error-flagging the BSB and hashing 255 extensions plus a trailing
    # comma.  (Ethernet linktype, port 443, client 10.9.19.1.)

    TS_START = 1700014000.0

    CMAC = bytes.fromhex('02aa00001b01')
    SMAC = bytes.fromhex('02aa00001b02')

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

    def tls_rec(hs_type, body):
        hs = bytes([hs_type]) + len(body).to_bytes(3, 'big') + body
        return b'\x16\x03\x03' + struct.pack('>H', len(hs)) + hs

    chello = tls_rec(1, (b'\x03\x03' + bytes(range(32)) + b'\x00' +
                         b'\x00\x02\x13\x01' +      # 1 cipher
                         b'\x01\x00' +              # compression
                         b'\x00\x00'))              # no extensions

    exts = b''.join(struct.pack('>HH', t, 0) for t in range(256))
    shello = tls_rec(2, (b'\x03\x03' + bytes(range(32, 64)) + b'\x00' +
                         b'\x13\x02' + b'\x00' +
                         struct.pack('>H', len(exts)) + exts))

    CLI, SRV, CPORT = '10.9.19.1', '10.9.19.2', 49621
    pkts = []
    cseq, sseq = 0x1000, 0x2000
    pkts.append(eth_ip_tcp(CLI, SRV, CMAC, SMAC, CPORT, 443, cseq, 0, 0x02)); cseq += 1
    pkts.append(eth_ip_tcp(SRV, CLI, SMAC, CMAC, 443, CPORT, sseq, cseq, 0x12)); sseq += 1
    pkts.append(eth_ip_tcp(CLI, SRV, CMAC, SMAC, CPORT, 443, cseq, sseq, 0x10))
    pkts.append(eth_ip_tcp(CLI, SRV, CMAC, SMAC, CPORT, 443, cseq, sseq, 0x18, chello)); cseq += len(chello)
    pkts.append(eth_ip_tcp(SRV, CLI, SMAC, CMAC, 443, CPORT, sseq, cseq, 0x18, shello)); sseq += len(shello)
    pkts.append(eth_ip_tcp(CLI, SRV, CMAC, SMAC, CPORT, 443, cseq, sseq, 0x11)); cseq += 1
    pkts.append(eth_ip_tcp(SRV, CLI, SMAC, CMAC, 443, CPORT, sseq, cseq, 0x11)); sseq += 1
    pkts.append(eth_ip_tcp(CLI, SRV, CMAC, SMAC, CPORT, 443, cseq, sseq, 0x10))

    out = b''
    ts = TS_START
    for p in pkts:
        sec = int(ts)
        usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.05
    return out


def sec_dhcpv6_relay():
    # DHCPv6 sessions exercising message types beyond the basic client set:
    #   S1 fe80::9:1: Solicit (1) - baseline
    #   S2 fe80::9:2: Leasequery (14) - dropped by parsers that reject type > 11
    #   S3 fe80::9:3: Relay-Forward (12) - 34-byte relay header (msg-type,
    #      hop-count, link-address, peer-address) before the options; parsers
    #      that skip only 4 bytes read the link address as garbage TLVs
    # Covers capture/parsers/dhcp.c dhcpv6_process relay handling and the
    # ja4plus JA4D6 type/header fixes.  Each datagram is sent twice since the
    # first packet of a UDP session only runs the port classifier.

    TS_START = 1700015000.0

    CMAC = bytes.fromhex('02aa00001c01')
    SMAC = bytes.fromhex('02aa00001c02')

    def csum(data):
        if len(data) & 1:
            data += b'\0'
        s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
        while s >> 16:
            s = (s & 0xffff) + (s >> 16)
        return (~s) & 0xffff

    def eth_ip6_udp(src_str, dst_str, sport, dport, payload):
        src = socket.inet_pton(socket.AF_INET6, src_str)
        dst = socket.inet_pton(socket.AF_INET6, dst_str)
        udp = struct.pack('!HHHH', sport, dport, 8 + len(payload), 0) + payload
        pseudo = src + dst + struct.pack('!I', len(udp)) + b'\x00\x00\x00\x11'
        udp = udp[:6] + struct.pack('!H', csum(pseudo + udp) or 0xffff) + udp[8:]
        ip = struct.pack('!IHBB', (6 << 28), len(udp), 17, 64) + src + dst
        return SMAC + CMAC + b'\x86\xdd' + ip + udp

    def opt(t, v):
        return struct.pack('>HH', t, len(v)) + v

    duid = opt(1, bytes.fromhex('000100012aabbccdd00112233445'))   # clientid
    oro = opt(6, struct.pack('>HH', 23, 24))                       # req opts 23,24
    elap = opt(8, b'\x00\x00')

    solicit = b'\x01\xaa\xbb\xcc' + duid + elap + oro + opt(3, bytes(12))
    leasequery = b'\x0e\x11\x22\x33' + duid + oro
    relayfwd = (b'\x0c\x00' +
                socket.inet_pton(socket.AF_INET6, '2001:db8::1') +
                socket.inet_pton(socket.AF_INET6, 'fe80::211:22ff:fe33:4455') +
                opt(9, solicit) + opt(18, b'eth0'))

    datagrams = [
        ('fe80::9:1', 'ff02::1:2', 546, 547, solicit),
        ('fe80::9:2', 'ff02::1:2', 546, 547, leasequery),
        ('fe80::9:3', '2001:db8::99', 547, 547, relayfwd),
    ]

    out = b''
    ts = TS_START
    for src, dst, sport, dport, payload in datagrams:
        for _ in range(2):
            p = eth_ip6_udp(src, dst, sport, dport, payload)
            sec = int(ts)
            usec = int(round((ts - sec) * 1e6))
            out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
            ts += 0.05
    return out


def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/arkime_synthetic.pcap'
    out = LEGACY
    out += sec_icmpv6_haad()
    out += sec_ikev2_sa_init()
    out += sec_dns_https_empty_alpn()
    out += sec_certs_keyusage()
    out += sec_imap_crsplit()
    out += sec_krb5_biglen()
    out += sec_quic_zerotag()
    out += sec_s7comm_frameclamp()
    out += sec_sctp_interleave()
    out += sec_smb1_dialect0()
    out += sec_smb1_malformed_delete()
    out += sec_nbns_response()
    out += sec_ssh_kexinit_overrun()
    out += sec_udp_zero_ulen()
    out += sec_websocket_split()
    out += sec_tls_ja3_edge()
    out += sec_modbus_edge()
    out += sec_oracle_big_connect()
    out += sec_sip_radius_edge()
    out += sec_tls_256ext()
    out += sec_dhcpv6_relay()
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
