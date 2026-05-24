#!/usr/bin/env python3
"""Generate adb_synthetic.pcap testing all features in capture/parsers/adb.c

Sessions:
  1 (port 5555): CNXN + AUTH(token,sig,rsa) + OPEN shell + WRTE + OKAY + CLSE
  2 (port 5555): Sync protocol - STAT,LIST,SEND,RECV,DATA,DONE,OKAY,FAIL,QUIT
  3 (port 5555): Shell v2 - STDOUT,STDERR,EXIT with exit code
  4 (port 5555): Port forwarding - forward:, reverse:forward:, reverse:list
  5 (port 5555): TLS transition - A_SYNC + A_STLS both sides → unregister
  6 (port 5037): Client-server protocol - hex4+service, host:forward:
"""

import struct
import socket
import sys

# ADB transport commands (little-endian)
A_SYNC = 0x434e5953
A_CNXN = 0x4e584e43
A_OPEN = 0x4e45504f
A_OKAY = 0x59414b4f
A_CLSE = 0x45534c43
A_WRTE = 0x45545257
A_AUTH = 0x48545541
A_STLS = 0x534c5453

# Sync protocol IDs
SYNC_STAT = 0x54415453
SYNC_LIST = 0x5453494c
SYNC_SEND = 0x444e4553
SYNC_RECV = 0x56434552
SYNC_DATA = 0x41544144
SYNC_DONE = 0x454e4f44
SYNC_OKAY = 0x59414b4f
SYNC_FAIL = 0x4c494146
SYNC_QUIT = 0x54495551


def adb_msg(cmd, arg0, arg1, payload=b''):
    """Build ADB transport message: 24-byte header + payload."""
    dlen = len(payload)
    dchk = sum(payload) & 0xFFFFFFFF
    return struct.pack('<6I', cmd, arg0, arg1, dlen, dchk, cmd ^ 0xFFFFFFFF) + payload


def sync_msg(sid, arg, data=b''):
    """Build sync protocol message: 4-byte ID + 4-byte arg + optional data."""
    return struct.pack('<II', sid, arg) + data


def shell_v2_pkt(pid, payload=b''):
    """Build shell v2 packet: 1-byte ID + 4-byte LE length + payload."""
    return struct.pack('<BI', pid, len(payload)) + payload


def tls_client_hello():
    """Build a minimal TLS 1.2 ClientHello record that triggers JA3/JA4."""
    # Handshake body: ClientHello
    client_random = b'\xaa' * 32
    session_id = b''
    # Cipher suites: TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 (0xc02f),
    #                TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384 (0xc030),
    #                TLS_RSA_WITH_AES_128_GCM_SHA256 (0x009c)
    ciphers = struct.pack('!HHH', 0xc02f, 0xc030, 0x009c)
    compressions = b'\x00'  # null compression

    # Extensions
    # SNI extension (type=0x0000): server_name = "adb-device.local"
    hostname = b'adb-device.local'
    sni_entry = struct.pack('!BH', 0, len(hostname)) + hostname  # host_name type + len + name
    sni_list = struct.pack('!H', len(sni_entry)) + sni_entry
    ext_sni = struct.pack('!HH', 0x0000, len(sni_list)) + sni_list

    # Supported versions extension (type=0x002b): TLS 1.2
    ext_versions = struct.pack('!HH', 0x002b, 3) + b'\x02\x03\x03'

    # Signature algorithms extension (type=0x000d)
    algos = struct.pack('!HH', 0x0401, 0x0501)  # rsa_pkcs1_sha256, rsa_pkcs1_sha384
    ext_sigalgs = struct.pack('!HHH', 0x000d, 2 + len(algos), len(algos)) + algos

    # Supported groups / elliptic curves (type=0x000a): x25519(0x001d), secp256r1(0x0017)
    groups = struct.pack('!HH', 0x001d, 0x0017)
    ext_groups = struct.pack('!HHH', 0x000a, 2 + len(groups), len(groups)) + groups

    extensions = ext_sni + ext_versions + ext_sigalgs + ext_groups
    extensions_blob = struct.pack('!H', len(extensions)) + extensions

    hello_body = (
        struct.pack('!H', 0x0303) +            # client_version TLS 1.2
        client_random +                          # 32 bytes random
        struct.pack('!B', len(session_id)) + session_id +
        struct.pack('!H', len(ciphers)) + ciphers +
        struct.pack('!B', len(compressions)) + compressions +
        extensions_blob
    )

    # Handshake header: type=1 (ClientHello), 3-byte length
    handshake = struct.pack('!B', 1) + struct.pack('!I', len(hello_body))[1:] + hello_body

    # TLS record: ContentType=0x16, Version=0x0303, length
    return struct.pack('!BHH', 0x16, 0x0303, len(handshake)) + handshake


def tls_server_hello():
    """Build a minimal TLS 1.2 ServerHello record that triggers JA3S."""
    server_random = b'\xbb' * 32
    session_id = b'\xcc' * 32
    # Chosen cipher: TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 (0xc02f)
    cipher = struct.pack('!H', 0xc02f)
    compression = b'\x00'

    # Extensions: supported_versions (type=0x002b): TLS 1.2
    ext_versions = struct.pack('!HH', 0x002b, 2) + struct.pack('!H', 0x0303)
    extensions_blob = struct.pack('!H', len(ext_versions)) + ext_versions

    hello_body = (
        struct.pack('!H', 0x0303) +
        server_random +
        struct.pack('!B', len(session_id)) + session_id +
        cipher +
        compression +
        extensions_blob
    )

    handshake = struct.pack('!B', 2) + struct.pack('!I', len(hello_body))[1:] + hello_body
    return struct.pack('!BHH', 0x16, 0x0303, len(handshake)) + handshake


def ip_cksum(data):
    if len(data) % 2:
        data += b'\x00'
    s = sum(struct.unpack('!%dH' % (len(data) // 2), data))
    while s >> 16:
        s = (s & 0xFFFF) + (s >> 16)
    return ~s & 0xFFFF


def tcp_cksum(sip, dip, tcp_hdr, payload):
    d = tcp_hdr + payload
    pseudo = struct.pack('!4s4sBBH', sip, dip, 0, 6, len(d))
    return ip_cksum(pseudo + d)


class PcapWriter:
    def __init__(self, path):
        self.f = open(path, 'wb')
        # pcap global header: magic, v2.4, tz=0, sigfigs=0, snaplen=65535, linktype=ETHERNET
        self.f.write(struct.pack('<IHHiIII', 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1))
        self.ts_sec = 1000000000
        self.ts_usec = 0

    def write(self, data, advance_ms=10):
        self.ts_usec += advance_ms * 1000
        while self.ts_usec >= 1000000:
            self.ts_sec += 1
            self.ts_usec -= 1000000
        self.f.write(struct.pack('<IIII', self.ts_sec, self.ts_usec, len(data), len(data)))
        self.f.write(data)

    def advance(self, ms):
        """Advance time without writing a packet (gap between sessions)."""
        self.ts_usec += ms * 1000
        while self.ts_usec >= 1000000:
            self.ts_sec += 1
            self.ts_usec -= 1000000

    def close(self):
        self.f.close()


class TCPSession:
    CMAC = bytes.fromhex('001122334455')
    SMAC = bytes.fromhex('667788990011')

    def __init__(self, pw, cip_str, sip_str, cport, sport):
        self.pw = pw
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

        # TCP header (20 bytes, data_offset=5)
        tcp = struct.pack('!HHIIBBHHH', sp, dp, seq, ack, 0x50, flags, 65535, 0, 0)
        chk = tcp_cksum(sip, dip, tcp, payload)
        tcp = struct.pack('!HHIIBBHHH', sp, dp, seq, ack, 0x50, flags, 65535, chk, 0)

        # IP header (20 bytes)
        total = 20 + 20 + len(payload)
        ip_hdr = struct.pack('!BBHHHBBH4s4s', 0x45, 0, total, self.ipid, 0x4000, 64, 6, 0, sip, dip)
        ic = ip_cksum(ip_hdr)
        ip_hdr = struct.pack('!BBHHHBBH4s4s', 0x45, 0, total, self.ipid, 0x4000, 64, 6, ic, sip, dip)
        self.ipid += 1

        # Ethernet: dst(6) + src(6) + ethertype(2) + payload
        return dmac + smac + struct.pack('!H', 0x0800) + ip_hdr + tcp + payload

    def handshake(self):
        self.pw.write(self._packet(True, 0x02, self.cseq, 0))            # SYN
        self.cseq += 1
        self.pw.write(self._packet(False, 0x12, self.sseq, self.cseq), advance_ms=5)  # SYN-ACK
        self.sseq += 1
        self.pw.write(self._packet(True, 0x10, self.cseq, self.sseq), advance_ms=5)   # ACK

    def csend(self, data):
        """Client → Server data with PSH+ACK."""
        self.pw.write(self._packet(True, 0x18, self.cseq, self.sseq, data))
        self.cseq += len(data)

    def ssend(self, data):
        """Server → Client data with PSH+ACK."""
        self.pw.write(self._packet(False, 0x18, self.sseq, self.cseq, data))
        self.sseq += len(data)

    def fin(self):
        self.pw.write(self._packet(True, 0x11, self.cseq, self.sseq))                 # FIN+ACK
        self.cseq += 1
        self.pw.write(self._packet(False, 0x11, self.sseq, self.cseq), advance_ms=5)  # FIN+ACK
        self.sseq += 1
        self.pw.write(self._packet(True, 0x10, self.cseq, self.sseq), advance_ms=5)   # ACK


def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'adb_synthetic.pcap'
    pw = PcapWriter(outpath)

    # ==========================================================================
    # Session 1: Basic ADB (port 5555)
    # Tests: CNXN (version, maxpayload, systemtype, serial), AUTH (all 3 types),
    #        OPEN (shell service, stream IDs), OKAY, WRTE, CLSE
    # ==========================================================================
    s = TCPSession(pw, '10.0.0.1', '10.0.0.2', 40001, 5555)
    s.handshake()

    # Client CNXN: version=0x01000001, maxdata=262144, banner="host::"
    s.csend(adb_msg(A_CNXN, 0x01000001, 262144, b'host::\x00'))
    # Server AUTH token challenge
    s.ssend(adb_msg(A_AUTH, 1, 0, b'A' * 20))
    # Client AUTH signature response
    s.csend(adb_msg(A_AUTH, 2, 0, b'S' * 32))
    # Client AUTH rsapublickey
    s.csend(adb_msg(A_AUTH, 3, 0, b'ssh-rsa AAAA user@host\x00'))
    # Server CNXN: banner="device:HVA12345:features=shell_v2"
    s.ssend(adb_msg(A_CNXN, 0x01000001, 262144, b'device:HVA12345:features=shell_v2\x00'))
    # Client OPEN shell service
    s.csend(adb_msg(A_OPEN, 1, 0, b'shell:ls -la\x00'))
    # Server OKAY (local-id=2, remote-id=1)
    s.ssend(adb_msg(A_OKAY, 2, 1))
    # Server WRTE data
    s.ssend(adb_msg(A_WRTE, 2, 1, b'total 42\ndrwxr-xr-x root root 4096\n'))
    # Client OKAY (ack the WRTE)
    s.csend(adb_msg(A_OKAY, 1, 2))
    # Server CLSE
    s.ssend(adb_msg(A_CLSE, 2, 1))
    # Client CLSE
    s.csend(adb_msg(A_CLSE, 1, 2))
    s.fin()

    pw.advance(2000)

    # ==========================================================================
    # Session 2: Sync protocol (port 5555)
    # Tests: sync mode detection, STAT+path, LIST+path, SEND+path(with ,mode),
    #        RECV+path, DATA+payload, DONE, OKAY, FAIL+message, QUIT
    #        Also tests multi-message sync parsing within single WRTE
    # ==========================================================================
    s = TCPSession(pw, '10.0.0.1', '10.0.0.2', 40002, 5555)
    s.handshake()

    s.csend(adb_msg(A_CNXN, 0x01000001, 262144, b'host::\x00'))
    s.ssend(adb_msg(A_CNXN, 0x01000001, 262144, b'device:SYNC001:\x00'))
    # OPEN sync: (triggers syncMode + "adb:sync" tag)
    s.csend(adb_msg(A_OPEN, 1, 0, b'sync:\x00'))
    s.ssend(adb_msg(A_OKAY, 2, 1))

    # WRTE with STAT + LIST in one payload (tests sync loop)
    p1 = b'/sdcard/test.txt'
    p2 = b'/sdcard/'
    s.csend(adb_msg(A_WRTE, 1, 2,
        sync_msg(SYNC_STAT, len(p1), p1) +
        sync_msg(SYNC_LIST, len(p2), p2)))
    s.ssend(adb_msg(A_OKAY, 2, 1))

    # WRTE with SEND (path has ,mode suffix) + DATA + DONE
    p3 = b'/sdcard/upload.txt,0644'
    filedata = b'Hello from ADB sync!'
    s.csend(adb_msg(A_WRTE, 1, 2,
        sync_msg(SYNC_SEND, len(p3), p3) +
        sync_msg(SYNC_DATA, len(filedata), filedata) +
        sync_msg(SYNC_DONE, 0)))
    s.ssend(adb_msg(A_OKAY, 2, 1))

    # Server sync OKAY response
    s.ssend(adb_msg(A_WRTE, 2, 1, sync_msg(SYNC_OKAY, 0)))
    s.csend(adb_msg(A_OKAY, 1, 2))

    # RECV request
    p4 = b'/sdcard/download.txt'
    s.csend(adb_msg(A_WRTE, 1, 2, sync_msg(SYNC_RECV, len(p4), p4)))
    s.ssend(adb_msg(A_OKAY, 2, 1))

    # Server FAIL response
    errmsg = b'No such file'
    s.ssend(adb_msg(A_WRTE, 2, 1, sync_msg(SYNC_FAIL, len(errmsg), errmsg)))
    s.csend(adb_msg(A_OKAY, 1, 2))

    # QUIT
    s.csend(adb_msg(A_WRTE, 1, 2, sync_msg(SYNC_QUIT, 0)))
    s.ssend(adb_msg(A_OKAY, 2, 1))

    s.csend(adb_msg(A_CLSE, 1, 2))
    s.ssend(adb_msg(A_CLSE, 2, 1))
    s.fin()

    pw.advance(2000)

    # ==========================================================================
    # Session 3: Shell v2 protocol (port 5555)
    # Tests: shell,v2: detection, "adb:shell-v2" tag, STDOUT, STDERR,
    #        EXIT with exit code, multi-message v2 parsing in one WRTE
    # ==========================================================================
    s = TCPSession(pw, '10.0.0.1', '10.0.0.2', 40003, 5555)
    s.handshake()

    s.csend(adb_msg(A_CNXN, 0x01000001, 262144, b'host::\x00'))
    s.ssend(adb_msg(A_CNXN, 0x01000001, 262144, b'device:SHELL01:features=shell_v2\x00'))
    # OPEN shell v2 (triggers shellV2 + "adb:shell-v2" tag)
    s.csend(adb_msg(A_OPEN, 1, 0, b'shell,v2:echo hello\x00'))
    s.ssend(adb_msg(A_OKAY, 2, 1))

    # WRTE with STDOUT + STDERR in one payload (tests shell v2 loop)
    s.ssend(adb_msg(A_WRTE, 2, 1,
        shell_v2_pkt(1, b'hello\n') +
        shell_v2_pkt(2, b'warning: something\n')))
    s.csend(adb_msg(A_OKAY, 1, 2))

    # WRTE with EXIT code 42
    s.ssend(adb_msg(A_WRTE, 2, 1, shell_v2_pkt(3, b'\x2a')))
    s.csend(adb_msg(A_OKAY, 1, 2))

    s.ssend(adb_msg(A_CLSE, 2, 1))
    s.csend(adb_msg(A_CLSE, 1, 2))
    s.fin()

    pw.advance(2000)

    # ==========================================================================
    # Session 4: Port forwarding (port 5555)
    # Tests: forward: (direction=forward, local=tcp:8080, remote=tcp:80),
    #        reverse:forward: (direction=reverse, endpoints),
    #        reverse:list (direction=reverse, no endpoints)
    # ==========================================================================
    s = TCPSession(pw, '10.0.0.1', '10.0.0.2', 40004, 5555)
    s.handshake()

    s.csend(adb_msg(A_CNXN, 0x01000001, 262144, b'host::\x00'))
    s.ssend(adb_msg(A_CNXN, 0x01000001, 262144, b'device:FWD001:\x00'))

    # forward:tcp:8080;tcp:80
    s.csend(adb_msg(A_OPEN, 1, 0, b'forward:tcp:8080;tcp:80\x00'))
    s.ssend(adb_msg(A_OKAY, 2, 1))

    # reverse:forward:tcp:9090;tcp:443
    s.csend(adb_msg(A_OPEN, 3, 0, b'reverse:forward:tcp:9090;tcp:443\x00'))
    s.ssend(adb_msg(A_OKAY, 4, 3))

    # reverse:list (hits "reverse:" branch without "forward:")
    s.csend(adb_msg(A_OPEN, 5, 0, b'reverse:list\x00'))
    s.ssend(adb_msg(A_OKAY, 6, 5))

    s.fin()

    pw.advance(2000)

    # ==========================================================================
    # Session 5: TLS transition + A_SYNC command (port 5555)
    # Tests: A_SYNC (logged), A_STLS (both sides, "adb:tls" tag,
    #        "adb:tls-encrypted" tag, ARKIME_PARSER_UNREGISTER),
    #        then TLS ClientHello + ServerHello → JA3/JA4/JA3S
    # ==========================================================================
    s = TCPSession(pw, '10.0.0.1', '10.0.0.2', 40005, 5555)
    s.handshake()

    s.csend(adb_msg(A_CNXN, 0x01000001, 262144, b'host::\x00'))
    s.ssend(adb_msg(A_CNXN, 0x01000001, 262144, b'device:TLS001:\x00'))

    # A_SYNC command (just gets logged as "sync" command)
    s.csend(adb_msg(A_SYNC, 1, 0))

    # Client STLS → sets tls[0]=1, adds "adb:tls" tag
    s.csend(adb_msg(A_STLS, 0x01000000, 0))
    # Server STLS → both sides TLS, "adb:tls-encrypted" tag, unregister
    s.ssend(adb_msg(A_STLS, 0x01000000, 0))

    # Now ADB parser is unregistered; TLS handshake follows
    s.csend(tls_client_hello())
    s.ssend(tls_server_hello())

    s.fin()

    pw.advance(2000)

    # ==========================================================================
    # Session 6: Client-server protocol (port 5037)
    # Tests: hex4+service format, base service name extraction,
    #        host:forward: forwarding detection in client-server mode
    # ==========================================================================
    s = TCPSession(pw, '10.0.0.3', '10.0.0.4', 40006, 5037)
    s.handshake()

    # "host:version" → base service "host"
    svc1 = b'host:version'
    s.csend(('%04x' % len(svc1)).encode() + svc1)

    # "host:forward:tcp:8080;tcp:80" → forwarding detection + base service "host"
    svc2 = b'host:forward:tcp:8080;tcp:80'
    s.csend(('%04x' % len(svc2)).encode() + svc2)

    s.fin()

    pw.close()
    print(f'Created {outpath}')


if __name__ == '__main__':
    main()
