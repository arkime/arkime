#!/usr/bin/env python3
"""Generate tests/pcap/opcua_synthetic.pcap exercising capture/parsers/opcua.c

Sessions (all TCP, dst port 4840, full SYN/SYN-ACK/ACK + FIN teardown):

  1. IPv4 - HEL EndpointUrl="opc.tcp://server.example.com:4840/synthetic1"
           OPN SecurityPolicyUri="...#None", SenderCertificate=null
  2. IPv6 - HEL EndpointUrl="opc.tcp://[fd00::1]:4840/synthetic2"
           OPN SecurityPolicyUri="...#Basic256Sha256",
               SenderCertificate=DER(CN=opcua-synthetic-client)
  3. IPv4 - HEL EndpointUrl="" (empty, length=0)
           OPN policy=None, cert=null
  4. IPv4 - HEL EndpointUrl="opc.tcp://hostA.example:4840/"
           OPN policy=Basic128Rsa15, cert=DER(CN=opcua-synthetic-A)
  5. IPv6 - HEL EndpointUrl length=-1 (null)
           OPN policy=None, cert=null

The two embedded DER certificates were generated once with python-cryptography
(self-signed 1024-bit RSA) and inlined as hex literals to keep the script
deterministic (same pcap bytes on every run).
"""

import socket
import struct
import sys


# ---------------------------------------------------------------------------
# OPC UA Binary message helpers
# ---------------------------------------------------------------------------

def _hdr(msgtype, chunk, body):
    """3-byte msgtype + 1-byte chunk + uint32 LE total length + body."""
    total = 8 + len(body)
    return msgtype + chunk + struct.pack('<I', total) + body


def _bytestring(b):
    """OPC UA ByteString: int32 LE length + bytes. b=None -> length=-1."""
    if b is None:
        return struct.pack('<i', -1)
    return struct.pack('<i', len(b)) + b


def hel(endpoint_url):
    """HEL Final. endpoint_url may be None (null), '' (empty) or a str."""
    if endpoint_url is None:
        url_bs = _bytestring(None)
    else:
        url_bs = _bytestring(endpoint_url.encode('utf-8'))
    body = struct.pack('<IIIII',
                       0,            # ProtocolVersion
                       65536,        # ReceiveBufferSize
                       65536,        # SendBufferSize
                       16777216,     # MaxMessageSize
                       5000)         # MaxChunkCount
    body += url_bs
    return _hdr(b'HEL', b'F', body)


def ack():
    body = struct.pack('<IIIII', 0, 65536, 65536, 16777216, 5000)
    return _hdr(b'ACK', b'F', body)


def opn(security_policy_uri, sender_cert_der):
    """OPN Final. policy uri is str. sender_cert_der is bytes or None."""
    body = struct.pack('<I', 0x12345678)             # SecureChannelId
    body += _bytestring(security_policy_uri.encode('utf-8'))
    body += _bytestring(sender_cert_der)
    body += _bytestring(None)                        # ReceiverCertThumbprint=null
    # SequenceHeader: SequenceNumber, RequestId
    body += struct.pack('<II', 1, 1)
    # TypeId: FourByteNodeId for OpenSecureChannelRequest_Encoding_DefaultBinary
    # (NodeId 446 in namespace 0). EncodingMask=0x01, namespace=0, identifier=446.
    body += struct.pack('<BBH', 0x01, 0x00, 446)
    # RequestHeader
    body += struct.pack('<BB', 0x00, 0x00)           # AuthenticationToken TwoByteNodeId
    body += struct.pack('<q', 0)                     # Timestamp
    body += struct.pack('<I', 1)                     # RequestHandle
    body += struct.pack('<I', 0)                     # ReturnDiagnostics
    body += _bytestring(None)                        # AuditEntryId null
    body += struct.pack('<I', 10000)                 # TimeoutHint
    body += struct.pack('<BBB', 0x00, 0x00, 0x00)    # AdditionalHeader ExtensionObject (no body)
    # OpenSecureChannelRequest body
    body += struct.pack('<I', 0)                     # ClientProtocolVersion
    body += struct.pack('<I', 0)                     # SecurityTokenRequestType = ISSUE
    body += struct.pack('<I', 1)                     # MessageSecurityMode = NONE
    body += _bytestring(None)                        # ClientNonce null
    body += struct.pack('<I', 3600000)               # RequestedLifetime
    return _hdr(b'OPN', b'F', body)


# ---------------------------------------------------------------------------
# Embedded self-signed DER certificates (generated once, deterministic)
# ---------------------------------------------------------------------------

CERT_CLIENT = bytes.fromhex(
    '308201b930820122a003020102020411223344300d06092a864886f70d01010b'
    '05003021311f301d06035504030c166f706375612d73796e7468657469632d63'
    '6c69656e74301e170d3234303130313030303030305a170d3334303130313030'
    '303030305a3021311f301d06035504030c166f706375612d73796e7468657469'
    '632d636c69656e7430819f300d06092a864886f70d010101050003818d003081'
    '8902818100a303f5de8427aa6a721de288d9d4a00aeabd8c05e348f19c2f5122'
    'b5b98eb3de92e53bbca4d05f920d49bb4623c847747d941ba20640550e45236e'
    'b2f416be45db38a6fc172df870ab1f1671337424a29029c06a2a58b3ab3da383'
    '2df52293a3993045d729c2357b6dee8af9c0775a409dc209832ed59e1d827747'
    '5e6cf3b1530203010001300d06092a864886f70d01010b050003818100762988'
    'e9d842d01feaae09b3684ccc8306842e84f9ca2a2775b366673a14c53490ec92'
    'e8c49ef69338ee3ecaa7a235bd0e6b4631fc3b064bda3a333c81616a9ea7aba8'
    'de739cf880c4a1116f2f287f7918ed0c7300383d1549946f950cfd02bee9ee02'
    'db9068f75704e3c70d40ec8f719a281942c07a1333b11eec1f2b008ec4')

CERT_A = bytes.fromhex(
    '308201af30820118a003020102020411223344300d06092a864886f70d01010b'
    '0500301c311a301806035504030c116f706375612d73796e7468657469632d41'
    '301e170d3234303130313030303030305a170d3334303130313030303030305a'
    '301c311a301806035504030c116f706375612d73796e7468657469632d413081'
    '9f300d06092a864886f70d010101050003818d0030818902818100bb458dd5ff'
    '9827f090aa8c5b6414ab68c367c15dfcb8cd7bd997622fe5df0e5c62879fb663'
    '7363447940e2211fd46a31444a756acaadd80c613785fb63203a049f058fffe0'
    '1e94fd28836ce66d7e8989bbb5b23b4a7370aad8a39ffd4c77cb9fcefacdcbba'
    '3f6b145673b15e52a00a994d70d33286553938ecec4d03bd776dc50203010001'
    '300d06092a864886f70d01010b0500038181007b28990e2af02bfb9e45619ab2'
    '956aa41da514edc819647f3e0e4a2248ce198e7c9c1e5146990c5d7ce86ecc44'
    '45ce8202eecd47ec64c03d6d1e07929f66770d32162c6dc82fb9edfcd9e362f9'
    '7f989c722812d20a82da33f965cd517bc648bc3042e6385e12a081c44f83d778'
    '7ce1a28ec541317ba211ca399da0acd1aca83b')


POLICY_NONE   = 'http://opcfoundation.org/UA/SecurityPolicy#None'
POLICY_B256   = 'http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256'
POLICY_B128   = 'http://opcfoundation.org/UA/SecurityPolicy#Basic128Rsa15'


# ---------------------------------------------------------------------------
# Network checksums and pcap writer (adapted from gen_ntlm_synthetic.py)
# ---------------------------------------------------------------------------

def ip_cksum(data):
    if len(data) % 2:
        data += b'\x00'
    s = sum(struct.unpack('!%dH' % (len(data) // 2), data))
    while s >> 16:
        s = (s & 0xffff) + (s >> 16)
    return ~s & 0xffff


def tcp_cksum_v4(sip, dip, tcp_hdr, payload):
    d = tcp_hdr + payload
    pseudo = struct.pack('!4s4sBBH', sip, dip, 0, 6, len(d))
    return ip_cksum(pseudo + d)


def tcp_cksum_v6(sip, dip, tcp_hdr, payload):
    d = tcp_hdr + payload
    pseudo = struct.pack('!16s16sIBBBB', sip, dip, len(d), 0, 0, 0, 6)
    return ip_cksum(pseudo + d)


class PcapWriter:
    def __init__(self, path):
        self.f = open(path, 'wb')
        # pcap global header: magic, v2.4, tz=0, sigfigs=0, snaplen=65535, ETHERNET
        self.f.write(struct.pack('<IHHiIII', 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1))
        self.ts_sec = 1700000000
        self.ts_usec = 0

    def write(self, data, advance_ms=10):
        self.ts_usec += advance_ms * 1000
        while self.ts_usec >= 1000000:
            self.ts_sec += 1
            self.ts_usec -= 1000000
        self.f.write(struct.pack('<IIII', self.ts_sec, self.ts_usec,
                                 len(data), len(data)))
        self.f.write(data)

    def advance(self, ms):
        self.ts_usec += ms * 1000
        while self.ts_usec >= 1000000:
            self.ts_sec += 1
            self.ts_usec -= 1000000

    def close(self):
        self.f.close()


class TCPSession:
    """TCP session generator supporting both IPv4 and IPv6."""

    CMAC = bytes.fromhex('001122334455')
    SMAC = bytes.fromhex('667788990011')

    def __init__(self, pw, cip_str, sip_str, cport, sport):
        self.pw = pw
        if ':' in cip_str:
            self.v6 = True
            self.cip = socket.inet_pton(socket.AF_INET6, cip_str)
            self.sip = socket.inet_pton(socket.AF_INET6, sip_str)
        else:
            self.v6 = False
            self.cip = socket.inet_aton(cip_str)
            self.sip = socket.inet_aton(sip_str)
        self.cport = cport
        self.sport = sport
        self.cseq = 1000
        self.sseq = 2000
        self.ipid = 1

    def _packet(self, is_client, flags, seq, ack_, payload=b''):
        if is_client:
            smac, dmac = self.CMAC, self.SMAC
            sip, dip = self.cip, self.sip
            sp, dp = self.cport, self.sport
        else:
            smac, dmac = self.SMAC, self.CMAC
            sip, dip = self.sip, self.cip
            sp, dp = self.sport, self.cport

        tcp = struct.pack('!HHIIBBHHH', sp, dp, seq, ack_, 0x50,
                          flags, 65535, 0, 0)
        if self.v6:
            chk = tcp_cksum_v6(sip, dip, tcp, payload)
        else:
            chk = tcp_cksum_v4(sip, dip, tcp, payload)
        tcp = struct.pack('!HHIIBBHHH', sp, dp, seq, ack_, 0x50,
                          flags, 65535, chk, 0)

        if self.v6:
            payload_len = len(tcp) + len(payload)
            ip_hdr = struct.pack('!IHBB16s16s',
                                 (6 << 28),
                                 payload_len, 6, 64,
                                 sip, dip)
            return dmac + smac + struct.pack('!H', 0x86dd) + ip_hdr + tcp + payload
        else:
            total = 20 + 20 + len(payload)
            ip_hdr = struct.pack('!BBHHHBBH4s4s', 0x45, 0, total, self.ipid,
                                 0x4000, 64, 6, 0, sip, dip)
            ic = ip_cksum(ip_hdr)
            ip_hdr = struct.pack('!BBHHHBBH4s4s', 0x45, 0, total, self.ipid,
                                 0x4000, 64, 6, ic, sip, dip)
            self.ipid += 1
            return dmac + smac + struct.pack('!H', 0x0800) + ip_hdr + tcp + payload

    def handshake(self):
        self.pw.write(self._packet(True, 0x02, self.cseq, 0))
        self.cseq += 1
        self.pw.write(self._packet(False, 0x12, self.sseq, self.cseq),
                      advance_ms=5)
        self.sseq += 1
        self.pw.write(self._packet(True, 0x10, self.cseq, self.sseq),
                      advance_ms=5)

    def csend(self, data):
        self.pw.write(self._packet(True, 0x18, self.cseq, self.sseq, data))
        self.cseq += len(data)
        self.pw.write(self._packet(False, 0x10, self.sseq, self.cseq),
                      advance_ms=2)

    def ssend(self, data):
        self.pw.write(self._packet(False, 0x18, self.sseq, self.cseq, data))
        self.sseq += len(data)
        self.pw.write(self._packet(True, 0x10, self.cseq, self.sseq),
                      advance_ms=2)

    def fin(self):
        self.pw.write(self._packet(True, 0x11, self.cseq, self.sseq))
        self.cseq += 1
        self.pw.write(self._packet(False, 0x11, self.sseq, self.cseq),
                      advance_ms=5)
        self.sseq += 1
        self.pw.write(self._packet(True, 0x10, self.cseq, self.sseq),
                      advance_ms=5)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def session(pw, cip, sip, cport, hel_url, opn_policy, opn_cert):
    s = TCPSession(pw, cip, sip, cport, 4840)
    s.handshake()
    s.csend(hel(hel_url))
    s.ssend(ack())
    s.csend(opn(opn_policy, opn_cert))
    s.fin()
    pw.advance(1000)


def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/opcua_synthetic.pcap'
    pw = PcapWriter(outpath)

    # 1. IPv4 - endpoint set, no cert, policy None
    session(pw, '10.10.10.10', '10.10.10.11', 40001,
            'opc.tcp://server.example.com:4840/synthetic1',
            POLICY_NONE, None)

    # 2. IPv6 - endpoint set, cert present, policy Basic256Sha256
    session(pw, 'fd00:10::1', 'fd00:10::2', 40002,
            'opc.tcp://[fd00::1]:4840/synthetic2',
            POLICY_B256, CERT_CLIENT)

    # 3. IPv4 - empty endpoint URL, no cert
    session(pw, '10.10.11.10', '10.10.11.11', 40003,
            '', POLICY_NONE, None)

    # 4. IPv4 - endpoint set, cert present, policy Basic128Rsa15
    session(pw, '10.10.12.10', '10.10.12.11', 40004,
            'opc.tcp://hostA.example:4840/',
            POLICY_B128, CERT_A)

    # 5. IPv6 - null endpoint URL, no cert
    session(pw, 'fd00:20::1', 'fd00:20::2', 40005,
            None, POLICY_NONE, None)

    pw.close()
    print('Created %s' % outpath)


if __name__ == '__main__':
    main()
