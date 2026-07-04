#!/usr/bin/env python3
"""Regenerate tests/pcap/dcerpc_synthetic.pcap in full.

Layout:
  - LEGACY blob: all 125 packets (predate generator code; add new
    sessions as section functions below)

Run from the tests directory:  python3 gen/gen_dcerpc_synthetic.py
Each section function below documents the session(s) it generates.
"""

import base64
import struct
import sys
import zlib

# Packets accumulated before per-session generator code existed
# (zlib-compressed pcap prefix, including the 24-byte global header)
LEGACY = zlib.decompress(base64.b64decode(
    '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'))




def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/dcerpc_synthetic.pcap'
    out = LEGACY
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
