#!/usr/bin/env python3
"""Append an MQTT oversized-PUBLISH regression session to mqtt_synthetic.pcap

Session (192.168.1.61:55557 -> 10.0.0.100:1883):
  SYN, SYN-ACK, ACK, CONNECT (v3.1.1), ACK, then a PUBLISH whose variable
  header (2-byte topic len + 8190-byte topic = 8192 = bufMax) passes the
  headerNeeded > bufMax check, but fixed header (3 bytes) + variable header
  = 8195 > bufMax, so the parser buffer truncates and the header can never
  complete. Sent in two TCP segments (4096 + 4099).

Regression: the PUBLISH need-more-data path previously ignored the truncated
flag and stalled forever; it must tag mqtt:message-too-long and unregister.

Idempotent: skips appending if the pcap already has > BASE_PACKETS packets.
"""

import struct
import sys

PCAP = 'pcap/mqtt_synthetic.pcap'
BASE_PACKETS = 54  # packets before this script's additions

CLI_IP = '192.168.1.61'
SRV_IP = '10.0.0.100'
CLI_PORT = 55557
SRV_PORT = 1883
TS_START = 1700000100.0

# Same CONNECT payload as existing sessions (MQTT 3.1.1, clientid iot-gateway-west)
CONNECT = bytes.fromhex('102a00044d5154540482003c0010696f742d676174657761792d77657374000c676174657761795f75736572')


def mqtt_oversized_publish():
    # topic length 8190 -> headerNeeded = 2 + 8190 = 8192 (== bufMax, passes)
    # remainingLen = 8192, fixed header = 1 + 2 varint = 3 bytes
    # total 8195 > bufMax 8192 -> buffer truncates -> old code stalls
    topic = b'oversize/' + b't' * 8181  # 8190 bytes
    remaining_len = 2 + len(topic)
    assert remaining_len == 8192
    varint = bytes([remaining_len % 128 | 0x80, remaining_len // 128])
    return bytes([0x30]) + varint + struct.pack('>H', len(topic)) + topic


def csum(data):
    if len(data) & 1:
        data += b'\0'
    s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
    while s >> 16:
        s = (s & 0xffff) + (s >> 16)
    return (~s) & 0xffff


def ip_tcp(src, dst, sport, dport, seq, ack, flags, payload=b''):
    iplen = 20 + 20 + len(payload)
    ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, iplen, 1, 0, 64, 6, 0,
                     bytes(map(int, src.split('.'))), bytes(map(int, dst.split('.'))))
    ip = ip[:10] + struct.pack('>H', csum(ip)) + ip[12:]
    tcp = struct.pack('>HHIIBBHHH', sport, dport, seq, ack, 0x50, flags, 8192, 0, 0)
    pseudo = ip[12:20] + struct.pack('>BBH', 0, 6, 20 + len(payload))
    tcp = tcp[:16] + struct.pack('>H', csum(pseudo + tcp + payload)) + tcp[18:]
    return ip + tcp + payload  # linktype 228 = raw IPv4


def main():
    data = open(PCAP, 'rb').read()
    off = 24
    n = 0
    while off < len(data):
        _, _, cl, _ = struct.unpack('<IIII', data[off:off + 16])
        off += 16 + cl
        n += 1
    if n > BASE_PACKETS:
        print('%s already has %d packets, not appending' % (PCAP, n))
        return

    cseq, sseq = 0x3000, 0x4000
    pkts = []

    def add(p):
        pkts.append(p)

    add(ip_tcp(CLI_IP, SRV_IP, CLI_PORT, SRV_PORT, cseq, 0, 0x02))            # SYN
    add(ip_tcp(SRV_IP, CLI_IP, SRV_PORT, CLI_PORT, sseq, cseq + 1, 0x12))     # SYN-ACK
    cseq += 1
    sseq += 1
    add(ip_tcp(CLI_IP, SRV_IP, CLI_PORT, SRV_PORT, cseq, sseq, 0x10))         # ACK
    add(ip_tcp(CLI_IP, SRV_IP, CLI_PORT, SRV_PORT, cseq, sseq, 0x18, CONNECT))
    cseq += len(CONNECT)
    add(ip_tcp(SRV_IP, CLI_IP, SRV_PORT, CLI_PORT, sseq, cseq, 0x10))         # ACK
    publish = mqtt_oversized_publish()
    assert len(publish) == 8195
    add(ip_tcp(CLI_IP, SRV_IP, CLI_PORT, SRV_PORT, cseq, sseq, 0x18, publish[:4096]))
    cseq += 4096
    add(ip_tcp(SRV_IP, CLI_IP, SRV_PORT, CLI_PORT, sseq, cseq, 0x10))         # ACK
    add(ip_tcp(CLI_IP, SRV_IP, CLI_PORT, SRV_PORT, cseq, sseq, 0x18, publish[4096:]))
    cseq += len(publish) - 4096
    add(ip_tcp(SRV_IP, CLI_IP, SRV_PORT, CLI_PORT, sseq, cseq, 0x10))         # ACK

    out = b''
    ts = TS_START
    for p in pkts:
        sec = int(ts)
        usec = int(round((ts - sec) * 1e6))
        out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
        ts += 0.05

    with open(PCAP, 'ab') as f:
        f.write(out)
    print('appended %d packets to %s' % (len(pkts), PCAP))


if __name__ == '__main__':
    main()
