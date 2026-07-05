#!/usr/bin/env python3
"""Regenerate tests/pcap/snmpv3_synthetic.pcap in full.

Layout:
  - LEGACY blob: all 8 packets (predate generator code; add new
    sessions as section functions below)

Run from the tests directory:  python3 gen/gen_snmpv3_synthetic.py
Each section function below documents the session(s) it generates.
"""

import base64
import struct
import sys
import zlib

# Packets accumulated before per-session generator code existed
# (zlib-compressed pcap prefix, including the 24-byte global header)
LEGACY = zlib.decompress(base64.b64decode(
    'eNp1kr0vA2Ecx5/n6eNK66UsXYjEa6oJvypi0+BiM5AIMdCXizShba4vdCsTk0kkgrQVm8lgstnE'
    'SIwGVhF/gPD7uUd6aC/3vd/dk7vPfb65u7+5LAkm2c/2+cnYC528zxs08io6Dc5YqNXYdjE2hyn5'
    '91mJGasPsCy4AzyCKFwID5Nc4orsBZ90F1jnbqFtZGHr6Fpwhrt0hWMb8cRKNm2YEt8MHXQsegXn'
    '1g3QpDX4NbzgnNUxrkQmVXQalkgIJW5J5MqLIoMwDT34kNRS2ch6PFqTKBQxp6LTsIgXSBsgovsN'
    'iZGJFlisVBO2al3QV62acyOZiGeSf3qJahYOZTGlotOwLAposEcW3gO0GDKfoJd6OVNmPBfOGGWr'
    'mPiHlAq5qaLTsJAniBsn5GkBkdGFNViqFHPYinVDf7Vi9aYRjiUT6/nfzRzVmtUpDb0SXWk8ogKQ'
    'xuwHagS8d9BPzRrSRtQ0MoHh4Fmtj6Yp6IyKTsOClhEYJejOKEKDz4fgs/0G5+1CUpVvqAuaoVGr'
    'r2CdtbElRMa+sWOEfT3+hS3asYyw0ob9AiEFf/Q='))




def _snmp_pkt(src, dst, sport, payload, ts):
    # Raw-IPv4 linktype (228) packet record
    def csum(data):
        if len(data) & 1:
            data += b'\0'
        s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
        while s >> 16:
            s = (s & 0xffff) + (s >> 16)
        return (~s) & 0xffff

    udp = struct.pack('>HHHH', sport, 161, 8 + len(payload), 0) + payload
    iplen = 20 + len(udp)
    ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, iplen, 1, 0, 64, 17, 0,
                     bytes(map(int, src.split('.'))), bytes(map(int, dst.split('.'))))
    ip = ip[:10] + struct.pack('>H', csum(ip)) + ip[12:]
    pkt = ip + udp
    sec = int(ts)
    usec = int(round((ts - sec) * 1e6))
    return struct.pack('<IIII', sec, usec, len(pkt), len(pkt)) + pkt


def sec_snmp_version2():
    # SNMP message with wire version byte 2 (historic SNMPv2u), which the
    # parser rejects. Regression for the classifier accepting version 2 and
    # tagging the session snmp with zero parsed fields.
    # Session 10.0.1.10:40001 -> 10.0.0.161:161.
    getreq = (b'\x02\x01\x01' + b'\x02\x01\x00' + b'\x02\x01\x00' +  # id, err, idx
              b'\x30\x00')                        # empty varbind list
    msg = (b'\x02\x01\x02' +                      # version = 2
           b'\x04\x06public' +                    # community
           b'\xa0' + bytes([len(getreq)]) + getreq)
    body = b'\x30' + bytes([len(msg)]) + msg
    return _snmp_pkt('10.0.1.10', '10.0.0.161', 40001, body, 1781050000.0) * 2


def sec_snmp_primitive_type():
    # SNMPv1 message whose third TLV is a primitive INTEGER instead of a
    # constructed PDU. Regression for the parser recording snmp.type ==
    # "GetResponse" (types[2]) from the primitive tag before the apc check.
    # Session 10.0.1.11:40002 -> 10.0.0.161:161.
    msg = (b'\x02\x01\x00' +                      # version = 0 (v1)
           b'\x04\x06public' +                    # community
           b'\x02\x04\x00\x00\x00\x2a')           # primitive INTEGER 42
    body = b'\x30' + bytes([len(msg)]) + msg
    return _snmp_pkt('10.0.1.11', '10.0.0.161', 40002, body, 1781050001.0) * 2


def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/snmpv3_synthetic.pcap'
    out = LEGACY
    out += sec_snmp_version2()
    out += sec_snmp_primitive_type()
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
