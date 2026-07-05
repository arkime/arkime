#!/usr/bin/env python3
"""Regenerate tests/pcap/dnssec_synthetic.pcap in full.

Layout:
  - LEGACY blob: all 10 packets (predate generator code; add new
    sessions as section functions below)

Run from the tests directory:  python3 gen/gen_dnssec_synthetic.py
Each section function below documents the session(s) it generates.
"""

import base64
import struct
import sys
import zlib

# Packets accumulated before per-session generator code existed
# (zlib-compressed pcap prefix, including the 24-byte global header)
LEGACY = zlib.decompress(base64.b64decode(
    'eNq7cnjTQiYGFgYY+P+fgYERSAf8n5wO4rtDMYOgkrFLaFp5x8xVuzkYXBkYLEHKHARXTD+wgjGF'
    'AwgMLBlMGVSlK4RMGCFGAAF7akVibkFOKnNyfi5IkDEQaiwnIwRDDIQYDjb2N8TY5VdBRoKMZjAF'
    'Gvxc1EDIpLEBKMeM1VhUAT2QGYw6DPYMjLxMDAx8AulTqxnSc98xGFiiKLy3dt97fBjVVH2oqVIs'
    'eakVJahybAkMDBwMzKiC2mAPNzCoXDHkZVq1+8xdeBAygHjv/iOHaRCx4T0NGCip4PC2AoU3V6qy'
    'K57wDoYamwHFGOEdBTW2FBreqcDwtmJwm3FZ2RUc3owkhDcXPLw7Q4j1zlSgnWlg71iDvPMz0CQM'
    'j3dCocYmQTGGd0KgxlZBvZMG9I41g8MlM5Mwor2jDfUOi4ElB1MYsR6ZArQtHewRG5BHXti5puPx'
    'SDjU2BIoxvBIGtTYdKhH0oEesWEI8gt0TSfaI7AUK4YlxSokMEQQ67XJQPszwF6zBXntunZYBR6v'
    'RUKNbYRiDK8VQ42NhHotA+g1W4b4dyphFSQnOWXMLI6RrQHpCjCx'))




def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/dnssec_synthetic.pcap'
    out = LEGACY
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
