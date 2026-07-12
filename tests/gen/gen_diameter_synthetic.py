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
    'eNqllV1IFFEUx8+M4pqpY9qH5D6YfWCSy7qaJRs62Ae9REtEFBWxrKOuuTvTzmpraYiV9BARYWSR'
    'pi8hVir1kBGEUIIPCUY9BEH0UFLQBz4YfTzUOTN313GcaNW7DHN37rm/Oed/zrnz6un9Xh4SITr+'
    '/AHg8O4vOOvf22SDUpzT1VEgf73YVqeMDk79TIadAPlkJiZ93zfSx1XhNTY4JtgBEj4SxcPnwu7r'
    'AHXzoYwRSbAPjgHYJpD0yZOZC+IwwPGF+vIJSS88GUh5BFDPKP1I6Lei3NQp028tKdm50NmtaRMe'
    'AY4kgsySA5E7499wziWLAKt99X4pGHZIEW9AqZccPjlAVlw+rmXNfshx+CwdOHqTZpOC/1MBnGU4'
    'T8N55n5JDW/XeAAB5nk7WrZbeR5h8fMmFV8AJB4lzw8Wap57xLmep+pvJls9ClUKNUqh/0cRjDMr'
    '1eMpFCAA/2CI9Fw6HK2Qe4cB5PlQeCIJ9gdDAEIPkh5RhXRhMpWF+oJ1IfRShXTdATjBKL+Q8MuK'
    '8plRllhSUOf8Ck3nZ1ghGXhfadDZhvoV1EQKVUlV/XKw0Oksciu+ULWjuEZRSFB3GQ1Na8rDilmL'
    'EMvBcknxGR5z6+mZUnPS+Cxnrh1QxWU3YHLxxaLBtkfPP7nN3YrOQ3H2SvUhU1Z6EdBCSkwe03tF'
    'XJwS5uo0RzpHEZV5Xom7K608L9E8P5eXdzcDOLz4h52UQzz5Nl0bwKXly3Bo52AK/vA8C8+HyBNV'
    'sD/sJGJyRZqNR2IejhmibaKBEcvRptyK6GTEDUYfieJJen4JEWk+WT7ul4pcxbi7kdFcOHdZ0dYx'
    'msvoH0XZLKz7sRSPiJOMkI52dJkJHOiENQfM/vhOvDwCCfAueu7jcKDpBNb/NryvNWV9o1rqjaU9'
    '0nTKHQhIjvqw5AhKYTdGU7IZYvVvj67JihTyhuUQGbGMZ1s8pz6w16qq1Z4c6z1aT+QXFzldpU7t'
    '7aVbtpaJZrsIU+cJbnhipe8QU+e2Wd/x0fRdqM5l/XsWU6dVXJw65p74V9SWSjXFWc0dUyN9Lvw+'
    'ufjHrXp/XHHtX0n9UYxjppqzLpyaD5EnqmB/3ErENw1va6k/duCYIa5STsfZHx3TRh+J8r7x9o2Z'
    '/sCE4u7mOPvjKmf0j6K81t32nPqjhREmcfekFeE182eP2Z8tX6qCWAHDpJOhAvqwPwScJ5hPRa/P'
    'F46VABajO+hVHX5VPxU304j1R5ZhKfptEAz/qeazvF6vwUb/BtTJtUFHlSyJsQXuneEb8CE6P8Oi'
    'HsA/A1ZR97Coz5t1+/1sxyhG3Uq5NEStiIuL2lz3puhmK/AXVTDJrA=='))




def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/diameter_synthetic.pcap'
    out = LEGACY
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
