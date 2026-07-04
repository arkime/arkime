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




def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/snmpv3_synthetic.pcap'
    out = LEGACY
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
