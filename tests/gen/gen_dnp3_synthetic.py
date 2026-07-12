#!/usr/bin/env python3
"""Regenerate tests/pcap/dnp3_synthetic.pcap in full.

Layout:
  - LEGACY blob: all 20 packets (predate generator code; add new
    sessions as section functions below)

Run from the tests directory:  python3 gen/gen_dnp3_synthetic.py
Each section function below documents the session(s) it generates.
"""

import base64
import struct
import sys
import zlib

# Packets accumulated before per-session generator code existed
# (zlib-compressed pcap prefix, including the 24-byte global header)
LEGACY = zlib.decompress(base64.b64decode(
    'eNqd1D9oE1EcB/DvXe6u6aVXkzZt0j+Bo6AU/5GiFWKDVrmiQ7mGgCJKByEWYlYXwSW6iZOjSkE3'
    'JYtS/xEtlybdtFjI6mCndik6OJQO9fcuvwzxZSh38CMv3N3nvu+9H69Zf/dShYb2dXAAKGLw5GFR'
    '/Jzjwpvqt5+7iE2cca6FMQdMisdmjaXvJr1ApaYzrg2EtsXDOdXG3jLd+F/x329ZkqIKybXTGaDn'
    'B0k7uQEbf77SjaBZdkjazEVJWaV/rFzlkpTzrFS7KkkbiwlAL0QaClQUl72aklWNfF0LOsdNsvf9'
    'dBVyWbnCJSkZVr50VSjd7XWRznRUejD626uVgXvHjaBLt0/0lh/uOY1YcbkkZZaV910VCjd5R4Q7'
    '5i/dquWtUUg1pOlGT7jXDLO+wCXpl1hf6dS1wbbu0CJE+koPrP4j0djAYHxoOJEcGR0bHzuamlns'
    'Dbo/W/SJk/4S3AXMoPvTUihkqdaxP2u0P9N2hNkslzT30z4bW/pEZIhKa07R3Ed/lUQjmogDT726'
    'klWMv9N9bF3gkiKm2fogHOG5dnMK46m8XrCcOEysXPbqFIu25tENi7UMl5TsBGvVjmSJjQm9EPaT'
    '5RSvYV1/28/QDJcU6xRDnztijTx7JVZLxHr8wmtQrI9n44ft5nVidCqjPC86pXWJQ6lSAoYO2xBC'
    'MYTk2uV5RhRxKFVuAsNBsyi+Qm1VuQUkWMlzSYrDyuuuCrXV7n3RVjps18RGmLrCEaPiXPt4Sgad'
    'rfhCys95EfgHX0ksjA=='))




def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/dnp3_synthetic.pcap'
    out = LEGACY
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
