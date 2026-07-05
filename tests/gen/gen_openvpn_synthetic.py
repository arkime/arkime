#!/usr/bin/env python3
"""Regenerate tests/pcap/openvpn_synthetic.pcap in full.

Layout:
  - LEGACY blob: all 22 packets (predate generator code; add new
    sessions as section functions below)

Run from the tests directory:  python3 gen/gen_openvpn_synthetic.py
Each section function below documents the session(s) it generates.
"""

import base64
import struct
import sys
import zlib

# Packets accumulated before per-session generator code existed
# (zlib-compressed pcap prefix, including the 24-byte global header)
LEGACY = zlib.decompress(base64.b64decode(
    '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'
    'dUp2SHfGc2c893v13L8Av6uS/w=='))




def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/openvpn_synthetic.pcap'
    out = LEGACY
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
