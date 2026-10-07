#!/usr/bin/env python3
"""Regenerate tests/pcap/diameter_synthetic.pcap in full.

Layout:
  - LEGACY blob: all 22 packets (predate generator code; add new
    sessions as section functions below)

Run from the tests directory:  python3 gen/gen_diameter_synthetic.py
Each section function below documents the session(s) it generates.
"""

import base64
import struct
import sys
import zlib

# Packets accumulated before per-session generator code existed
# (zlib-compressed pcap prefix, including the 24-byte global header)
LEGACY = zlib.decompress(base64.b64decode(
    'eNqllVtIFGEUx8+MkWXpmHax3AezCya5rKtuyYYN3eglWiKiqIhlHW3N3Zl2Nl3tglhJDxERRhZd'
    '9CWki0o9ZAQhpOCDglIPPYUPJQVd6KHoAtU5M9+u4+dEa37LMN/Od77fnPM/53zz4tmDDhFmQXz8'
    '/g0g4D1YeDq4syEVPDinq7VQ/Xi+uUYb6P78fQ5sBSggM3n21119nUIlXoPdg5IDIOUtUXxiHmy/'
    'ClAzHcogkSRH9yBA6giS3vmy8kDuBTjyv768Q9KoLxMpjwFqGeUOEu7YUa6blC+vbCk5edB209Am'
    '2gcCSQRZpXtid4c/4VyYIwMsC9QGlXDUqcT8Ia1WcQbUEFkJBbiWPfmhIOCzDBDoTYZNGv6fD+Aq'
    'x3k6zrN2K3p0s8EDCDHPW9Cyxc7zGItf5FQcBZh1kDzfW2R47pOnej7ffDPZmlHoSqROifw7inCS'
    'WakaTqMAAcSHPaTnvN54hdzfD6BOhyISSXI87AGQ2pH0mCrkBiZT+19fsC6kDqqQG3cBjjLKDyT8'
    'sKO8Z5S5thTUuWCjoXM/Vkgm3hdbdE5F/QqrY0W6outBNVzkchV7tUCkyllSrWkkqLechqE15WHR'
    'pEVI5GChogUsj4VV9Eyrrrc+y51qB1RxOccwufhi2WLbbuaf3BZuxeeRJHulah+XlQ4EnCQlxg+Z'
    'vSLPTAm+OvlIpyiiM8834e5Ndp6XGp6fyc+/lwkCXuKjNsohnnxrr3Th0sIFOIxzMA1/eJ5Fp0MU'
    'iSo5HrURpfjXzjIRifk4JoipI8cYsQJpFXZEFyOutvpIFN/soQuISA+o6pGgUuwuwd11jObGuduO'
    'tpLR3Lx/J6SV3+bhEVHPCBloRxdPEMAkLN/D+xM4+vwApMBY/NzH4UTTEaz/DXhfwWV9je7xJ9Ie'
    'a2j0hkKKszaqOMNK1IvRlJZBov4d8TVVUyL+qBohI5bxHJvn1AeOw7putyfXfo/REwUlxS63x2W8'
    '3bNufbnM28WYOk9xw1M7fXuYOrd5fYcHMrahOhfN71lCnSZ5ZurwPfG3qG2Vakiymls/93W68fvk'
    'Fp80mf1xyb17MfVHCY6Jas4+1zgdokhUyfGkiSgXPUsrqD+24JggLtGOJ9kfrV+sPhLldd3taxP9'
    'gQnF3SeS7I/LAu/flZvNQ9QfJxlhHHeP2xFeMn928P6s+1AZxgroJZ0sFdCJ/SHhPIU/Ff2BQDRR'
    'AliM3rBfdwZ181Qso5Hoj2zLUvzbIFn+U81n+/1+i435DahRD4edlaoiJxaEMcs34E18fopF3YV/'
    'uuyibmdRn+V1+9m/ZQCjbqJcWqLW5JlFzdc9F91kBf4AlB/Kmw=='))




def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/diameter_synthetic.pcap'
    out = LEGACY
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
