#!/usr/bin/env python3
"""Regenerate tests/pcap/tcp-reassembly-synthetic.pcap in full.

Layout:
  - LEGACY blob: all 106 packets (predate generator code; add new
    sessions as section functions below)

Run from the tests directory:  python3 gen/gen_tcp-reassembly-synthetic.py
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
    'IjgI0csAfwNpGISV'))




def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/tcp-reassembly-synthetic.pcap'
    out = LEGACY
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
