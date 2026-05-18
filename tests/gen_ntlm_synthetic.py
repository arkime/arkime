#!/usr/bin/env python3
"""Generate ntlm_synthetic.pcap exercising every arkime_parsers_ntlm_decode
call path in capture/parsers.c via the SMB, HTTP, LDAP and DCE-RPC carriers,
for both IPv4 and IPv6 transports, with full TCP 3-way handshakes per session.

Sessions (>= 10):
   1. SMB1 IPv4 SetupAndX raw NTLMSSP direct security blob:
         Type1 then Type3                    -> smb.c:89 (smb_security_blob raw)
   2. SMB1 IPv4 SetupAndX SPNEGO-wrapped negTokenResp{NTLMSSP}:
         Type1, Type2, Type3                 -> smb.c:120 (smb_security_blob SPNEGO)
   3. SMB1 IPv6 SetupAndX SPNEGO-wrapped Type1/2/3 -> smb.c:120 over IPv6
   4. HTTP IPv4 GET with "Authorization: NTLM <b64 Type1>",
         server 401, retry with Type3        -> http.c:367 (Authorization NTLM)
   5. HTTP IPv6 GET with "Authorization: Negotiate <b64 Type1>",
         server 401, retry with Type3        -> http.c:367 (Authorization Negotiate)
   6. LDAP IPv4 Sicily auth: BindRequest tag 10 (Type1) then tag 11 (Type3)
                                              -> ldap.c:88 and ldap.c:92
   7. LDAP IPv4 SASL "NTLM" BindRequest Type1, BindResponse resultCode 14 with
         serverSaslCreds [7] = Type2, second BindRequest SASL credentials Type3
                                              -> ldap.c:81 (x2) and ldap.c:121
   8. LDAP IPv6 BindRequest (simple, anonymous) then BindResponse whose
         matchedDN OCTET STRING is a raw NTLMSSP Type2 challenge
                                              -> ldap.c:110
   9. DCE-RPC IPv4 BIND (pkt_type 11) with NTLMSSP Type1 auth trailer, then
         BIND_ACK (pkt_type 12) with NTLMSSP Type2 auth trailer
                                              -> dcerpc.c:237 (Type1+Type2)
  10. DCE-RPC IPv6 BIND Type1 then ALTER_CONTEXT (pkt_type 14) Type3
                                              -> dcerpc.c:237 over IPv6

Every session uses a full TCP 3-way handshake, includes both directions of
data plus server ACKs (matching the gen_adb_synthetic.py PcapWriter/TCPSession
style), and uses a distinct /16 IPv4 subnet or /64 IPv6 prefix so flows do
not collide.
"""

import base64
import socket
import struct
import sys

# -------------------------------------------------------------------------
# NTLMSSP message builders ([MS-NLMP])
# -------------------------------------------------------------------------

NTLMSSP_SIG       = b'NTLMSSP\0'
NEGOTIATE_UNICODE = 0x00000001
NEGOTIATE_OEM     = 0x00000002
REQUEST_TARGET    = 0x00000004
NEGOTIATE_NTLM    = 0x00000200
NEGOTIATE_VERSION = 0x02000000


def _secbuf(off, length):
    """8-byte security-buffer descriptor: Len(LE16) MaxLen(LE16) Offset(LE32)."""
    return struct.pack('<HHI', length, length, off)


def _version(major=10, minor=0, build=19041):
    """8-byte VERSION structure: major, minor, build(LE16), reserved(3), revision."""
    return struct.pack('<BBHBBBB', major, minor, build, 0, 0, 0, 0x0f)


def ntlm_type1(domain=b'WORKGROUP', host=b'CLIENT01', version=(10, 0, 19041)):
    """Build NTLMSSP NEGOTIATE_MESSAGE.  Type 1 strings are always OEM."""
    flags = (NEGOTIATE_UNICODE | NEGOTIATE_OEM | REQUEST_TARGET |
             NEGOTIATE_NTLM | NEGOTIATE_VERSION)
    header_len = 8 + 4 + 4 + 8 + 8 + 8     # sig + type + flags + 2 secbufs + version
    dom_off = header_len
    host_off = dom_off + len(domain)
    msg = (NTLMSSP_SIG +
           struct.pack('<I', 1) +
           struct.pack('<I', flags) +
           _secbuf(dom_off, len(domain)) +
           _secbuf(host_off, len(host)) +
           _version(*version) +
           domain + host)
    return msg


def ntlm_type2(target=b'TARGET', challenge=b'\x01\x23\x45\x67\x89\xab\xcd\xef',
               version=(10, 0, 19041)):
    """Build NTLMSSP CHALLENGE_MESSAGE.  Target name is unicode (UTF-16LE)."""
    flags = (NEGOTIATE_UNICODE | REQUEST_TARGET | NEGOTIATE_NTLM |
             NEGOTIATE_VERSION)
    tgt_u = target.decode('ascii').encode('utf-16le')
    # Header: sig(8)+type(4)+TgtSecBuf(8)+flags(4)+chall(8)+reserved(8)+TgtInfo(8)+ver(8)=56
    header_len = 8 + 4 + 8 + 4 + 8 + 8 + 8 + 8
    tgt_off = header_len
    info_off = tgt_off + len(tgt_u)
    # Minimal AV_PAIR list: just MsvAvEOL (id=0,len=0).
    av_pairs = struct.pack('<HH', 0, 0)
    msg = (NTLMSSP_SIG +
           struct.pack('<I', 2) +
           _secbuf(tgt_off, len(tgt_u)) +
           struct.pack('<I', flags) +
           challenge +
           b'\x00' * 8 +                               # reserved
           _secbuf(info_off, len(av_pairs)) +
           _version(*version) +
           tgt_u + av_pairs)
    return msg


def ntlm_type3(domain=b'CORP', user=b'alice', host=b'WS01', version=(10, 0, 19041)):
    """Build NTLMSSP AUTHENTICATE_MESSAGE.  Strings encoded as UTF-16LE."""
    flags = (NEGOTIATE_UNICODE | REQUEST_TARGET | NEGOTIATE_NTLM |
             NEGOTIATE_VERSION)
    dom_u  = domain.decode('ascii').encode('utf-16le')
    user_u = user.decode('ascii').encode('utf-16le')
    host_u = host.decode('ascii').encode('utf-16le')
    # Synthetic LM(24) + NT(24) responses + session key(16) - bytes don't matter.
    lm = b'L' * 24
    nt = b'N' * 24
    sk = b'K' * 16
    # Header layout: sig(8)+type(4) + 6*secbuf(48) + flags(4) + version(8) + MIC(16) = 88
    header_len = 8 + 4 + 6 * 8 + 4 + 8 + 16
    off = header_len
    lm_off = off;     off += len(lm)
    nt_off = off;     off += len(nt)
    dom_off = off;    off += len(dom_u)
    user_off = off;   off += len(user_u)
    host_off = off;   off += len(host_u)
    sk_off = off
    msg = (NTLMSSP_SIG +
           struct.pack('<I', 3) +
           _secbuf(lm_off, len(lm)) +
           _secbuf(nt_off, len(nt)) +
           _secbuf(dom_off, len(dom_u)) +
           _secbuf(user_off, len(user_u)) +
           _secbuf(host_off, len(host_u)) +
           _secbuf(sk_off, len(sk)) +
           struct.pack('<I', flags) +
           _version(*version) +
           b'\x00' * 16 +                              # MIC
           lm + nt + dom_u + user_u + host_u + sk)
    return msg


# -------------------------------------------------------------------------
# ASN.1 / DER helpers
# -------------------------------------------------------------------------

def _der_len(n):
    if n < 0x80:
        return bytes([n])
    out = b''
    while n:
        out = bytes([n & 0xff]) + out
        n >>= 8
    return bytes([0x80 | len(out)]) + out


def tlv(tag, value):
    return bytes([tag]) + _der_len(len(value)) + value


def ber_int(n):
    if n == 0:
        return tlv(0x02, b'\x00')
    body = b''
    v = n
    while v:
        body = bytes([v & 0xff]) + body
        v >>= 8
    # Add 0 byte if MSB set (avoid negative interpretation)
    if body[0] & 0x80:
        body = b'\x00' + body
    return tlv(0x02, body)


def ber_enum(n):
    return tlv(0x0a, struct.pack('B', n))


def ber_octet(b):
    return tlv(0x04, b)


def ber_seq(*parts):
    return tlv(0x30, b''.join(parts))


def spnego_negTokenResp_wrap(ntlmssp):
    """Wrap an NTLMSSP message in SPNEGO NegTokenResp [1] containing the
    [2] responseToken OCTET STRING that smb_security_blob() parses."""
    response_token = tlv(0xa2, ber_octet(ntlmssp))   # [2] responseToken
    inner_seq      = ber_seq(response_token)
    return tlv(0xa1, inner_seq)                       # [1] negTokenResp


def spnego_negTokenInit_wrap(ntlmssp):
    """GSS InitialContextToken -> SPNEGO negTokenInit with NTLMSSP mechToken."""
    spnego_oid = b'\x06\x06\x2b\x06\x01\x05\x05\x02'
    ntlm_oid = b'\x06\x0a\x2b\x06\x01\x04\x01\x82\x37\x02\x02\x0a'
    mech_types = tlv(0xa0, ber_seq(ntlm_oid))
    mech_token = tlv(0xa2, ber_octet(ntlmssp))
    return tlv(0x60, spnego_oid + tlv(0xa0, ber_seq(mech_types + mech_token)))


# -------------------------------------------------------------------------
# Network checksums and pcap writer
# -------------------------------------------------------------------------

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

    def _packet(self, is_client, flags, seq, ack, payload=b''):
        if is_client:
            smac, dmac = self.CMAC, self.SMAC
            sip, dip = self.cip, self.sip
            sp, dp = self.cport, self.sport
        else:
            smac, dmac = self.SMAC, self.CMAC
            sip, dip = self.sip, self.cip
            sp, dp = self.sport, self.cport

        tcp = struct.pack('!HHIIBBHHH', sp, dp, seq, ack, 0x50,
                          flags, 65535, 0, 0)
        if self.v6:
            chk = tcp_cksum_v6(sip, dip, tcp, payload)
        else:
            chk = tcp_cksum_v4(sip, dip, tcp, payload)
        tcp = struct.pack('!HHIIBBHHH', sp, dp, seq, ack, 0x50,
                          flags, 65535, chk, 0)

        if self.v6:
            payload_len = len(tcp) + len(payload)
            ip_hdr = struct.pack('!IHBB16s16s',
                                 (6 << 28),               # version=6, tc=0, fl=0
                                 payload_len, 6, 64,
                                 sip, dip)
            ethertype = 0x86dd
            return dmac + smac + struct.pack('!H', ethertype) + ip_hdr + tcp + payload
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
        """Client -> Server data, ACKed by the server."""
        self.pw.write(self._packet(True, 0x18, self.cseq, self.sseq, data))
        self.cseq += len(data)
        self.pw.write(self._packet(False, 0x10, self.sseq, self.cseq),
                      advance_ms=2)

    def ssend(self, data):
        """Server -> Client data, ACKed by the client."""
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


# -------------------------------------------------------------------------
# SMB1 frame builders
# -------------------------------------------------------------------------

def nbss(payload):
    """NetBIOS Session Service framing: type(1) + length(3, big-endian)."""
    n = len(payload)
    return struct.pack('!BBH', 0, (n >> 16) & 0xff, n & 0xffff) + payload


def smb1_header(cmd, flags=0, flags2=0x8001):
    """SMB1 header: 0xff 'SMB' cmd status flags flags2 pid_hi sig(8) reserved
       pid tid uid mid = 32 bytes."""
    return (b'\xffSMB' +
            struct.pack('<BIBH', cmd, 0, flags, flags2) +     # cmd, status, flags, flags2
            b'\x00' * 2 +                                      # PIDHigh
            b'\x00' * 8 +                                      # Signature
            b'\x00' * 2 +                                      # Reserved
            struct.pack('<HHHH', 0xabcd, 0x0001, 0x0001, 0x1234))


def smb1_setup_andx_secblob(blob):
    """SMB1 SessionSetupAndX request with wordcount=12 (extended security).
       The security blob is what smb_security_blob() ultimately parses."""
    # Word parameters (12 words = 24 bytes):
    #   AndXCommand(1) AndXReserved(1) AndXOffset(2) MaxBufferSize(2)
    #   MaxMpxCount(2) VcNumber(2) SessionKey(4) SecurityBlobLength(2)
    #   Reserved(4) Capabilities(4)
    words = struct.pack('<BBHHHHIHIA' if False else '<BBHHHHIHII',
                        0xff, 0, 0, 4356, 50, 1, 0,
                        len(blob), 0, 0x800000d4)
    # Data: SecurityBlob then native OS/LanMan/Domain (unicode, NUL-terminated).
    native = ('Windows 10\0' 'Windows 10 6.3\0' 'WORKGROUP\0').encode('utf-16le')
    # The unicode native strings must be 2-byte aligned relative to the SMB
    # header start.  Header(32) + wordcount(1) + word params(24) +
    # bytecount(2) + blob = 59 + len(blob).  Pad one byte if that's odd.
    pad = b'\x00' if ((59 + len(blob)) & 1) else b''
    payload = pad + native
    bytecount = len(blob) + len(payload)
    smb = smb1_header(0x73)            # SMB_COM_SESSION_SETUP_ANDX
    body = struct.pack('<B', 12) + words + struct.pack('<H', bytecount) + blob + payload
    return smb + body


# -------------------------------------------------------------------------
# DCE-RPC frame builders
# -------------------------------------------------------------------------

DR_LE = 0x10                       # data_representation byte 0 (LE)
def _uuid_wire_le(s):
    """Encode a UUID string 'aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee' as
       16 bytes in DCE-RPC wire format (data1/2/3 little-endian, data4 raw)."""
    a, b, c, d, e = s.split('-')
    return (struct.pack('<I', int(a, 16)) +
            struct.pack('<H', int(b, 16)) +
            struct.pack('<H', int(c, 16)) +
            bytes.fromhex(d) + bytes.fromhex(e))

LSARPC_UUID   = _uuid_wire_le('12345778-1234-abcd-ef00-0123456789ab')
NDR_XFER_UUID = _uuid_wire_le('8a885d04-1ceb-11c9-9fe8-08002b104860')


def dcerpc_bind(call_id, ntlm_blob, alter=False):
    """Build a DCE-RPC BIND (pkt_type=11) or ALTER_CONTEXT (pkt_type=14)
       packet with a single context item (lsarpc) and an NTLMSSP auth trailer.

       Header layout (16 bytes):
         ver(1) ver_minor(1) pkt_type(1) flags(1) dataRep(4)
         frag_len(2,LE) auth_len(2,LE) call_id(4,LE)
    """
    pkt_type = 14 if alter else 11
    # Body: max_xmit(2) max_recv(2) assoc_group(4) num_ctx(1) reserved(3)
    body = struct.pack('<HHIB3s', 0xb804, 0xb804, 0, 1, b'\x00\x00\x00')
    # Context item: ctx_id(2) num_trans(1) reserved(1) ifaceUUID(16) ifaceVer(2,LE) minor(2,LE)
    #               + 1 transfer syntax: UUID(16) ver(4)
    body += struct.pack('<HBB', 0, 1, 0) + LSARPC_UUID + struct.pack('<HH', 0, 0)
    body += NDR_XFER_UUID + struct.pack('<I', 2)

    # Auth trailer: auth_type(1)=10 NTLMSSP, auth_level(1)=2, pad_len(1)=0,
    #               reserved(1)=0, auth_ctx_id(4)=1, then auth_value (NTLM blob)
    auth_hdr = struct.pack('<BBBBI', 10, 2, 0, 0, 1)
    auth_blob = auth_hdr + ntlm_blob

    auth_len = len(ntlm_blob)
    frag_len = 16 + len(body) + len(auth_blob)
    hdr = struct.pack('<BBBBIHHI', 5, 0, pkt_type, 0x03, DR_LE,
                      frag_len, auth_len, call_id)
    return hdr + body + auth_blob


# -------------------------------------------------------------------------
# LDAP frame builders
# -------------------------------------------------------------------------

def ldap_bind_simple(msg_id=1, name=b'cn=anon'):
    auth = tlv(0x80, b'')                                       # [0] simple = empty
    body = ber_int(3) + ber_octet(name) + auth
    bindreq = tlv(0x60, body)                                   # [APP 0] BindRequest
    return ber_seq(ber_int(msg_id), bindreq)


def ldap_bind_sicily(msg_id, ntlm_blob, choice):
    """Microsoft "Sicily" auth: BindRequest authentication choice 10/11 with
       the NTLMSSP message carried directly as the [choice] value."""
    auth = tlv(0x80 | choice, ntlm_blob)                        # [10]/[11] PRIMITIVE
    body = ber_int(3) + ber_octet(b'') + auth
    bindreq = tlv(0x60, body)
    return ber_seq(ber_int(msg_id), bindreq)


def ldap_bind_sasl(msg_id, mech, creds):
    """SASL BindRequest: authentication choice [3] SaslCredentials ::=
         SEQUENCE { mechanism LDAPString, credentials OCTET STRING OPTIONAL }
       (tag [3] is IMPLICIT, so value is the SEQUENCE contents directly.)"""
    sasl_value = ber_octet(mech) + ber_octet(creds)
    auth = tlv(0xa3, sasl_value)                                # [3] CONSTRUCTED
    body = ber_int(3) + ber_octet(b'') + auth
    bindreq = tlv(0x60, body)
    return ber_seq(ber_int(msg_id), bindreq)


def ldap_bind_response(msg_id, result_code=0, matched_dn=b'', diag=b'',
                       server_sasl_creds=None):
    parts = [ber_enum(result_code), ber_octet(matched_dn), ber_octet(diag)]
    if server_sasl_creds is not None:
        parts.append(tlv(0x87, server_sasl_creds))              # [7] PRIMITIVE
    bindresp = tlv(0x61, b''.join(parts))                       # [APP 1]
    return ber_seq(ber_int(msg_id), bindresp)


# -------------------------------------------------------------------------
# Session generators
# -------------------------------------------------------------------------

def session_smb1_v4_raw(pw):
    """SMB1 IPv4 SetupAndX with raw NTLMSSP Type1 + raw Type3 (smb.c:89)."""
    s = TCPSession(pw, '10.10.1.100', '10.10.1.1', 50001, 445)
    s.handshake()
    blob1 = ntlm_type1(domain=b'ARKDOM1', host=b'ARKHOST1')
    s.csend(nbss(smb1_setup_andx_secblob(blob1)))
    # server response: just NetBIOS-framed SMB1 response stub (skipped by parser)
    s.ssend(nbss(smb1_header(0x73, flags=0x80) + struct.pack('<B', 4) +
                 b'\xff\x00\x00\x00' + b'\x00\x00\x00\x00' + struct.pack('<H', 0)))
    blob3 = ntlm_type3(domain=b'ARKDOM1', user=b'alice1', host=b'ARKHOST1')
    s.csend(nbss(smb1_setup_andx_secblob(blob3)))
    s.fin()


def session_smb1_v4_spnego(pw):
    """SMB1 IPv4 SPNEGO: negTokenInit Type1, response Type2, Type3."""
    s = TCPSession(pw, '10.10.2.100', '10.10.2.1', 50002, 445)
    s.handshake()
    t1 = ntlm_type1(domain=b'SPNDOM', host=b'SPNHOST', version=(10, 0, 20348))
    s.csend(nbss(smb1_setup_andx_secblob(spnego_negTokenInit_wrap(t1))))
    # Current smb.c only decodes responseToken, so include a minimal one too.
    s.csend(nbss(smb1_setup_andx_secblob(spnego_negTokenResp_wrap(t1))))
    t2 = ntlm_type2(target=b'SPNTGT', version=(10, 0, 17763))
    s.ssend(nbss(smb1_setup_andx_secblob(spnego_negTokenResp_wrap(t2))))
    t3 = ntlm_type3(domain=b'SPNDOM', user=b'bob', host=b'SPNHOST', version=(10, 0, 20348))
    s.csend(nbss(smb1_setup_andx_secblob(spnego_negTokenResp_wrap(t3))))
    s.fin()


def session_smb1_v6_spnego(pw):
    """SMB1 IPv6 SPNEGO: negTokenInit Type1, response Type2, Type3."""
    s = TCPSession(pw, '2001:db8:10::100', '2001:db8:10::1', 50003, 445)
    s.handshake()
    t1 = ntlm_type1(domain=b'V6DOM', host=b'V6HOST', version=(6, 1, 7600))
    s.csend(nbss(smb1_setup_andx_secblob(spnego_negTokenInit_wrap(t1))))
    # Current smb.c only decodes responseToken, so include a minimal one too.
    s.csend(nbss(smb1_setup_andx_secblob(spnego_negTokenResp_wrap(t1))))
    t2 = ntlm_type2(target=b'V6TGT', version=(6, 1, 7600))
    s.ssend(nbss(smb1_setup_andx_secblob(spnego_negTokenResp_wrap(t2))))
    t3 = ntlm_type3(domain=b'V6DOM', user=b'charlie', host=b'V6HOST', version=(6, 1, 7600))
    s.csend(nbss(smb1_setup_andx_secblob(spnego_negTokenResp_wrap(t3))))
    s.fin()


def http_request(method, path, host, headers):
    lines = ['%s %s HTTP/1.1' % (method, path), 'Host: %s' % host]
    lines.extend(headers)
    return ('\r\n'.join(lines) + '\r\n\r\n').encode()


def http_response(status, reason, headers, body=b''):
    lines = ['HTTP/1.1 %d %s' % (status, reason)]
    lines.extend(headers)
    lines.append('Content-Length: %d' % len(body))
    return ('\r\n'.join(lines) + '\r\n\r\n').encode() + body


def session_http_v4(pw):
    """HTTP IPv4 NTLM: Authorization NTLM Type1, 401 + WWW-Authenticate Type2,
       retry Authorization NTLM Type3 (http.c:367 Type1 and Type3)."""
    s = TCPSession(pw, '10.10.4.100', '10.10.4.1', 50004, 80)
    s.handshake()
    t1 = base64.b64encode(ntlm_type1(domain=b'HTTPDOM', host=b'HTTPHOST')).decode()
    s.csend(http_request('GET', '/protected/index.html', 'srv.example.com',
                         ['User-Agent: Mozilla/5.0',
                          'Authorization: NTLM ' + t1]))
    t2 = base64.b64encode(ntlm_type2(target=b'HTTPTGT')).decode()
    s.ssend(http_response(401, 'Unauthorized',
                          ['WWW-Authenticate: NTLM ' + t2,
                           'Connection: keep-alive']))
    t3 = base64.b64encode(ntlm_type3(domain=b'HTTPDOM', user=b'dave',
                                      host=b'HTTPHOST')).decode()
    s.csend(http_request('GET', '/protected/index.html', 'srv.example.com',
                         ['User-Agent: Mozilla/5.0',
                          'Authorization: NTLM ' + t3]))
    s.ssend(http_response(200, 'OK', ['Content-Type: text/plain'], b'OK\n'))
    s.fin()


def session_http_v6(pw):
    """HTTP IPv6 Negotiate: Type1 request, Type2 401, Type3 retry."""
    s = TCPSession(pw, '2001:db8:40::100', '2001:db8:40::1', 50005, 80)
    s.handshake()
    t1 = base64.b64encode(ntlm_type1(domain=b'HTTP6DOM', host=b'HTTP6HOST',
                                      version=(6, 3, 9600))).decode()
    s.csend(http_request('GET', '/secure/', 'v6srv.example.com',
                         ['User-Agent: Mozilla/5.0',
                          'Authorization: Negotiate ' + t1]))
    t2 = base64.b64encode(ntlm_type2(target=b'HTTP6TGT', version=(10, 0, 22621))).decode()
    s.ssend(http_response(401, 'Unauthorized',
                          ['WWW-Authenticate: Negotiate ' + t2,
                           'Connection: keep-alive']))
    t3 = base64.b64encode(ntlm_type3(domain=b'HTTP6DOM', user=b'eve',
                                      host=b'HTTP6HOST', version=(10, 0, 22621))).decode()
    s.csend(http_request('GET', '/secure/', 'v6srv.example.com',
                         ['User-Agent: Mozilla/5.0',
                          'Authorization: Negotiate ' + t3]))
    s.ssend(http_response(200, 'OK', ['Content-Type: text/plain'], b'hi\n'))
    s.fin()


def session_ldap_v4_sicily(pw):
    """LDAP IPv4 Sicily: BindRequest [10] Type1 then BindRequest [11] Type3.
       (ldap.c:88 and ldap.c:92)"""
    s = TCPSession(pw, '10.10.6.100', '10.10.6.1', 50006, 389)
    s.handshake()
    # Open with a tiny anonymous simple bind so the classifier triggers cleanly.
    s.csend(ldap_bind_simple(msg_id=1))
    s.ssend(ldap_bind_response(msg_id=1, result_code=0))
    t1 = ntlm_type1(domain=b'LDAPDOM', host=b'LDAPHOST')
    s.csend(ldap_bind_sicily(msg_id=2, ntlm_blob=t1, choice=10))
    s.ssend(ldap_bind_response(msg_id=2, result_code=14,
                                matched_dn=b'', diag=b'continue'))
    t3 = ntlm_type3(domain=b'LDAPDOM', user=b'frank', host=b'LDAPHOST')
    s.csend(ldap_bind_sicily(msg_id=3, ntlm_blob=t3, choice=11))
    s.ssend(ldap_bind_response(msg_id=3, result_code=0))
    s.fin()


def session_ldap_v4_sasl(pw):
    """LDAP IPv4 SASL NTLM: BindRequest SASL[NTLM] Type1, BindResponse
       resultCode 14 + serverSaslCreds [7] = Type2, BindRequest SASL Type3.
       (ldap.c:81 twice + ldap.c:121)"""
    s = TCPSession(pw, '10.10.7.100', '10.10.7.1', 50007, 389)
    s.handshake()
    t1 = ntlm_type1(domain=b'SASLDOM', host=b'SASLHOST')
    s.csend(ldap_bind_sasl(msg_id=1, mech=b'NTLM', creds=t1))
    t2 = ntlm_type2(target=b'SASLTGT')
    s.ssend(ldap_bind_response(msg_id=1, result_code=14,
                                matched_dn=b'', diag=b'',
                                server_sasl_creds=t2))
    t3 = ntlm_type3(domain=b'SASLDOM', user=b'grace', host=b'SASLHOST')
    s.csend(ldap_bind_sasl(msg_id=2, mech=b'NTLM', creds=t3))
    s.ssend(ldap_bind_response(msg_id=2, result_code=0))
    s.fin()


def session_ldap_v6_matched(pw):
    """LDAP IPv6 BindResponse whose matchedDN OCTET STRING is a raw NTLMSSP
       Type 2 challenge (ldap.c:110)."""
    s = TCPSession(pw, '2001:db8:60::100', '2001:db8:60::1', 50008, 389)
    s.handshake()
    s.csend(ldap_bind_simple(msg_id=1))
    t2 = ntlm_type2(target=b'LDAPV6TGT')
    s.ssend(ldap_bind_response(msg_id=1, result_code=49,
                                matched_dn=t2, diag=b'see matchedDN'))
    s.fin()


def session_dcerpc_v4(pw):
    """DCE-RPC IPv4 BIND with NTLMSSP Type1 auth trailer, BIND_ACK with Type2."""
    s = TCPSession(pw, '10.10.9.100', '10.10.9.1', 50009, 135)
    s.handshake()
    t1 = ntlm_type1(domain=b'RPCDOM', host=b'RPCHOST')
    s.csend(dcerpc_bind(call_id=1, ntlm_blob=t1, alter=False))
    # BIND_ACK: pkt_type=12. Body contains port string + result list, then auth trailer.
    body = struct.pack('<HHI', 0xb804, 0xb804, 0x3afb) \
         + struct.pack('<H', 4) + b'135\x00'  \
         + struct.pack('<BBBB', 1, 0, 0, 0)                 # num_results + pad
    body += struct.pack('<HH', 0, 0) + NDR_XFER_UUID + struct.pack('<I', 2)
    t2 = ntlm_type2(target=b'RPCTGT')
    auth_hdr = struct.pack('<BBBBI', 10, 2, 0, 0, 1)
    auth_blob = auth_hdr + t2
    frag_len = 16 + len(body) + len(auth_blob)
    ack_hdr = struct.pack('<BBBBIHHI', 5, 0, 12, 0x03, DR_LE,
                          frag_len, len(t2), 1)
    s.ssend(ack_hdr + body + auth_blob)
    s.fin()


def session_dcerpc_v6(pw):
    """DCE-RPC IPv6 BIND Type1 then ALTER_CONTEXT (pkt_type=14) Type3."""
    s = TCPSession(pw, '2001:db8:90::100', '2001:db8:90::1', 50010, 135)
    s.handshake()
    t1 = ntlm_type1(domain=b'RPC6DOM', host=b'RPC6HOST')
    s.csend(dcerpc_bind(call_id=1, ntlm_blob=t1, alter=False))
    t3 = ntlm_type3(domain=b'RPC6DOM', user=b'ivy', host=b'RPC6HOST')
    s.csend(dcerpc_bind(call_id=2, ntlm_blob=t3, alter=True))
    s.fin()


def session_smtp_v4(pw):
    """SMTP IPv4 AUTH NTLM 3-step exchange (RFC 4954 SASL)."""
    s = TCPSession(pw, '10.10.11.100', '10.10.11.1', 50011, 25)
    s.handshake()
    s.ssend(b'220 mail.example.com ESMTP\r\n')
    s.csend(b'EHLO client.example.com\r\n')
    s.ssend(b'250-mail.example.com\r\n250-AUTH NTLM\r\n250 OK\r\n')
    s.csend(b'AUTH NTLM\r\n')
    s.ssend(b'334 \r\n')
    t1 = ntlm_type1(domain=b'SMTPDOM', host=b'SMTPHOST')
    s.csend(base64.b64encode(t1) + b'\r\n')
    t2 = ntlm_type2(target=b'SMTPTGT')
    s.ssend(b'334 ' + base64.b64encode(t2) + b'\r\n')
    t3 = ntlm_type3(domain=b'SMTPDOM', user=b'jack', host=b'SMTPHOST')
    s.csend(base64.b64encode(t3) + b'\r\n')
    s.ssend(b'235 Authentication successful\r\n')
    s.csend(b'QUIT\r\n')
    s.ssend(b'221 Bye\r\n')
    s.fin()


def session_imap_v6(pw):
    """IMAP IPv6 AUTHENTICATE NTLM 3-step exchange (RFC 2595/4959)."""
    s = TCPSession(pw, '2001:db8:120::100', '2001:db8:120::1', 50012, 143)
    s.handshake()
    s.ssend(b'* OK IMAP4rev1 server ready\r\n')
    s.csend(b'a001 AUTHENTICATE NTLM\r\n')
    s.ssend(b'+ \r\n')
    t1 = ntlm_type1(domain=b'IMAPDOM', host=b'IMAPHOST')
    s.csend(base64.b64encode(t1) + b'\r\n')
    t2 = ntlm_type2(target=b'IMAPTGT')
    s.ssend(b'+ ' + base64.b64encode(t2) + b'\r\n')
    t3 = ntlm_type3(domain=b'IMAPDOM', user=b'kate', host=b'IMAPHOST')
    s.csend(base64.b64encode(t3) + b'\r\n')
    s.ssend(b'a001 OK AUTHENTICATE completed\r\n')
    s.csend(b'a002 LOGOUT\r\n')
    s.ssend(b'* BYE\r\na002 OK LOGOUT completed\r\n')
    s.fin()


def session_pop3_v4(pw):
    """POP3 IPv4 AUTH NTLM 3-step exchange (RFC 1734 / RFC 5034)."""
    s = TCPSession(pw, '10.10.13.100', '10.10.13.1', 50013, 110)
    s.handshake()
    s.ssend(b'+OK POP3 server ready\r\n')
    s.csend(b'AUTH NTLM\r\n')
    s.ssend(b'+ \r\n')
    t1 = ntlm_type1(domain=b'POP3DOM', host=b'POP3HOST')
    s.csend(base64.b64encode(t1) + b'\r\n')
    t2 = ntlm_type2(target=b'POP3TGT')
    s.ssend(b'+ ' + base64.b64encode(t2) + b'\r\n')
    t3 = ntlm_type3(domain=b'POP3DOM', user=b'liam', host=b'POP3HOST')
    s.csend(base64.b64encode(t3) + b'\r\n')
    s.ssend(b'+OK Logged in\r\n')
    s.csend(b'QUIT\r\n')
    s.ssend(b'+OK Bye\r\n')
    s.fin()


def tds_login7_with_sspi(sspi_blob):
    """Build a minimal TDS 7.4 LOGIN7 packet whose SSPI field carries
       an NTLMSSP Type 1 message.  All other fixed fields are empty."""
    # 94-byte fixed login7 portion (per MS-TDS 2.2.6.4).
    fixed_len = 94
    sspi_off = fixed_len
    total = fixed_len + len(sspi_blob)
    # Fixed header: Length(4), TDSVersion(4), PacketSize(4), ClientProgVer(4),
    # ClientPID(4), ConnectionID(4), Opt1(1), Opt2(1), Type(1), Opt3(1),
    # ClientTimeZone(4), ClientLCID(4) = 36 bytes.
    body = struct.pack('<I', total)
    body += struct.pack('<I', 0x74000004)   # TDS 7.4
    body += struct.pack('<I', 4096)         # PacketSize
    body += struct.pack('<I', 0x07000000)   # ClientProgVer
    body += struct.pack('<I', 100)          # ClientPID
    body += struct.pack('<I', 0)            # ConnectionID
    body += b'\xe0\x03\x00\x00'             # OptionFlags1/2/Type/Flags3 (defaults)
    body += struct.pack('<i', 0)            # ClientTimeZone
    body += struct.pack('<I', 0x00000409)   # ClientLCID
    # Offset/length table (each 4 bytes unless noted).
    # ibHostName/UserName/Password/AppName/ServerName: all empty at offset=fixed_len.
    for _ in range(5):
        body += struct.pack('<HH', fixed_len, 0)
    body += struct.pack('<HH', fixed_len, 0)   # ibUnused
    body += struct.pack('<HH', fixed_len, 0)   # ibCltIntName
    body += struct.pack('<HH', fixed_len, 0)   # ibLanguage
    body += struct.pack('<HH', fixed_len, 0)   # ibDatabase
    body += b'\x00' * 6                         # ClientID
    body += struct.pack('<HH', sspi_off, len(sspi_blob) if len(sspi_blob) < 0xFFFF else 0xFFFF)
    body += struct.pack('<HH', fixed_len, 0)   # ibAtchDBFile
    body += struct.pack('<HH', fixed_len, 0)   # ibChangePassword
    body += struct.pack('<I', 0 if len(sspi_blob) < 0xFFFF else len(sspi_blob))  # cbSSPILong
    body += sspi_blob
    assert len(body) == total, (len(body), total)
    # 8-byte TDS header: type=0x10, status=0x01, length(BE16), spid(BE16),
    # packet_id, window.
    pkt_len = 8 + len(body)
    return struct.pack('!BBHHBB', 0x10, 0x01, pkt_len, 0, 1, 0) + body


def tds_sspi_packet(sspi_blob):
    """TDS 0x11 SSPI message (raw NTLMSSP)."""
    pkt_len = 8 + len(sspi_blob)
    return struct.pack('!BBHHBB', 0x11, 0x01, pkt_len, 0, 1, 0) + sspi_blob


def session_tds_v6(pw):
    """MSSQL TDS 7.4 IPv6 LOGIN7 with SSPI=NTLMSSP Type1; SSPI(0x11) Type2/Type3."""
    s = TCPSession(pw, '2001:db8:140::100', '2001:db8:140::1', 50014, 1433)
    s.handshake()
    t1 = ntlm_type1(domain=b'TDSDOM', host=b'TDSHOST')
    s.csend(tds_login7_with_sspi(t1))
    t2 = ntlm_type2(target=b'TDSTGT')
    s.ssend(tds_sspi_packet(t2))
    t3 = ntlm_type3(domain=b'TDSDOM', user=b'mona', host=b'TDSHOST')
    s.csend(tds_sspi_packet(t3))
    s.fin()


# -------------------------------------------------------------------------
# Entry point
# -------------------------------------------------------------------------

def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/ntlm_synthetic.pcap'
    pw = PcapWriter(outpath)
    builders = [
        session_smb1_v4_raw,
        session_smb1_v4_spnego,
        session_smb1_v6_spnego,
        session_http_v4,
        session_http_v6,
        session_ldap_v4_sicily,
        session_ldap_v4_sasl,
        session_ldap_v6_matched,
        session_dcerpc_v4,
        session_dcerpc_v6,
        session_smtp_v4,
        session_imap_v6,
        session_pop3_v4,
        session_tds_v6,
    ]
    for fn in builders:
        fn(pw)
        pw.advance(500)
    pw.close()
    print('Wrote %s (%d sessions)' % (outpath, len(builders)))


if __name__ == '__main__':
    main()
