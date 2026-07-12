#!/usr/bin/env python3
"""Regenerate tests/pcap/mqtt_synthetic.pcap in full.

Layout:
  - LEGACY blob: packets 1-47 (original mqtt sessions, raw-IPv4 linktype 228)
  - mqtt_unsub (7 packets)
  - mqtt_oversized_pub (9 packets)

Run from the tests directory:  python3 gen/gen_mqtt_synthetic.py
Each section function below documents the session(s) it generates.
"""

import base64
import struct
import sys
import zlib

# Packets accumulated before per-session generator code existed
# (zlib-compressed pcap prefix, including the 24-byte global header)
LEGACY = zlib.decompress(base64.b64decode(
    'eNqNll1oHFUUx89+pGuSTQ0mm9gYks2WSrrrfpqNoVqyISZ0CYFtDNG40DqduU2H7syuM7NZlhqE'
    '2Oij1g98aCipWBCkFvogCArSgk9aWgi+tA8+CFp9UXwShHru3DtrdpiLWRhmZ3d+c35zz58zs3v7'
    '5lU/BMH5PHoE8DP98udLhO7G+TZHdz6AwoEvvvr2M19XF4Cymw2VAQK/AoTulvxRePAO40q3vTnK'
    'UDZU3s1SBtmHpSeQW2fcztu+/dR7iOy9Ui9yDQAf55aQWWLcEufe9eSejMLNBYDecQgunlxeDt6C'
    'F6DLJLpZNZKZTBY6JEVTdeg0iWwQK5t7llb4o+jfzx3dAwj22WaLAH7OzTGGbnOc2243C/Y5Zp9/'
    'CLD5FJ7VyYzM9GGALrmqaZKumOmEj1600BPYr0zGlsngzXNuFplZxs1y7opLJuPI/P0jQOYQ9Dkq'
    'FtFqxJCsukFy+VSeXvK9u/tWWbBVIviNczPIzDBuhnM7LpUFR+X+KYDcIPQ6KufqmqqoVhP8k5kj'
    '9II/fRDcr4hCRe7/g+sqjvgNZPope327PeKBPsYJIn6D1+sPla9v7414wM84QcTd9VoRDwQAujmn'
    'IqMyTuWc7snhgn3/O0Z8Gg4sniyaSi0wgCHvqZA1SW4mFbKuygT6TQtbaabbfw0rqilXdZ3IFlG6'
    'xcl33yhd2DgV9t8CCHNuHpl5xs1z7lK7cDDuCH/5ADs8DD2KZElpg0iKqq+Z4LsQo6mLHcvlNsLi'
    '6HvZnLBtPgXoF7f5GjIjlN3Mt7c5VWOcoM3XeL2RUHkzv7fNqdOME7TZXa/V5tRrABHOLSOzzLhl'
    'zjU9OVy151/ENsf5JNvEJveqVSu5JlmkITWTDWJaEOZHp+smMSLijrrvia5hxHabABjgHBa3tzm6'
    'Y5zR7haMOG4bz+Esy+JZj5+VZKtqNNMVVSdZOtHafsnRXx6TKsSwcMLZpQRt9lK025zC/wc5N4nM'
    'JOMmOfexS/GEozj2MsAnYfD/V35QPNS8qq/Q6sm/AEbEIbuMzDhlV+60h2z1B8YJQnaZ1xsPlVfu'
    '7A3Z6teME4TMXa8VstVvAEY5h7mxtzm6Y9ybnhyu0m8fYciGeMj8GLKDkl7Vk7X6mYpqniPGqDhU'
    '7nuga9Zhu1wFiHLu/12CHY7Lq+/jM2kIuu3iclonDfMMTovzOC2i9CAqTo+Xy5jtsgUwxrlpZKYZ'
    'N825iy6XMcfl8Fl0icBB7tIgkoWrYdZ1vTkmjpGXxpStoQPEOFdApsC4AufecmlMtUY9nZwD0MM1'
    'TKsqn8fJWa9Fc0di4kejl0fJ9ljBk8RxphM8R9nEWnuct+KME8T5Eq+XC5UTa3vjvHWIcYI4u+u1'
    '4rw1BHCUcx5z6ZQnh+u19ArGOcvi3EHf/iCsvW5Z+aRcUYluQbdWVYih2/MSOu2/apJpHhWn3H1r'
    'dCmHqeLFXwDinCsiU2RckXO1dsXgsKP4Bi7J5ig9q3s9n3Ze/9gRWUdJ+l4YF0fdS+i4LfQdQIJz'
    'JWRKjCtxTnUJHXeE1ncwY09DGOtbpEI0Yhn47gUXYnKtHjs2kX8mphENn9KZiamNhDj7Xl7E9roC'
    '8C+Lwre/'))


def sec_mqtt_unsub():
    # Append an MQTT UNSUBSCRIBE regression session to mqtt_synthetic.pcap
    # 
    # Session (192.168.1.60:55556 -> 10.0.0.100:1883):
    #   SYN, SYN-ACK, ACK, CONNECT (v3.1.1), ACK, UNSUBSCRIBE with TWO topic
    #   filters ("unsub/first", "unsub/second"), ACK.
    # 


    CLI_IP = '192.168.1.60'
    SRV_IP = '10.0.0.100'
    CLI_PORT = 55556
    SRV_PORT = 1883
    TS_START = 1700000050.0

    # Same CONNECT payload as existing sessions (MQTT 3.1.1, clientid iot-gateway-west)
    CONNECT = bytes.fromhex('102a00044d5154540482003c0010696f742d676174657761792d77657374000c676174657761795f75736572')


    def mqtt_unsubscribe(pktid, topics):
        payload = struct.pack('>H', pktid)
        for t in topics:
            payload += struct.pack('>H', len(t)) + t.encode()
        # No per-topic QoS byte in UNSUBSCRIBE (unlike SUBSCRIBE)
        assert len(payload) < 128
        return bytes([0xa2, len(payload)]) + payload


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


    def build():
        cseq, sseq = 0x1000, 0x2000
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
        unsub = mqtt_unsubscribe(5, ['unsub/first', 'unsub/second'])
        add(ip_tcp(CLI_IP, SRV_IP, CLI_PORT, SRV_PORT, cseq, sseq, 0x18, unsub))
        cseq += len(unsub)
        add(ip_tcp(SRV_IP, CLI_IP, SRV_PORT, CLI_PORT, sseq, cseq, 0x10))         # ACK

        out = b''
        ts = TS_START
        for p in pkts:
            sec = int(ts)
            usec = int(round((ts - sec) * 1e6))
            out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
            ts += 0.05

        return out
    return build()


def sec_mqtt_oversized_pub():
    # Append an MQTT oversized-PUBLISH regression session to mqtt_synthetic.pcap
    # 
    # Session (192.168.1.61:55557 -> 10.0.0.100:1883):
    #   SYN, SYN-ACK, ACK, CONNECT (v3.1.1), ACK, then a PUBLISH whose variable
    #   header (2-byte topic len + 8190-byte topic = 8192 = bufMax) passes the
    #   headerNeeded > bufMax check, but fixed header (3 bytes) + variable header


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


    def build():
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

        return out
    return build()


def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/mqtt_synthetic.pcap'
    out = LEGACY
    out += sec_mqtt_unsub()
    out += sec_mqtt_oversized_pub()
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
