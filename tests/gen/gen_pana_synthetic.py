#!/usr/bin/env python3
"""Regenerate tests/pcap/pana_synthetic.pcap in full.

Layout:
  - LEGACY blob: all 5 packets (predate generator code; add new
    sessions as section functions below)

Run from the tests directory:  python3 gen/gen_pana_synthetic.py
Each section function below documents the session(s) it generates.
"""

import base64
import struct
import sys
import zlib

# Packets accumulated before per-session generator code existed
# (zlib-compressed pcap prefix, including the 24-byte global header)
LEGACY = zlib.decompress(base64.b64decode(
    'eNq7cnjTQiYGFgYY+P+fgYERSCdfac1cpcDJYAVkg/AUrfx3PU1ZBUfXf/zJweDKwKADUuYgmLaP'
    'C6gBiJkMLJnOMEjsWAJULMAAMQQEGEEmtSpyMngBOV7YTLKBmrQOZArINKYzBpYMGr47gYoVgJhJ'
    'yCSsAkQDIQMDK1ABIwMrzI08SpwMcUB2HDaTA6Amz0Jxo418IlCxCZLJzGCTJYECjAySjCWpxSWl'
    'xalFDqkVibkFOal6yfm5UNtmKxEZIgh/SPh/hoYIM9Q2FpBJocqkh60jB9QkllW7z9wFhS0AEtRe'
    'yA=='))




def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/pana_synthetic.pcap'
    out = LEGACY
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
