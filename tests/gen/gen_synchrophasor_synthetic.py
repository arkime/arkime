#!/usr/bin/env python3
"""Regenerate tests/pcap/synchrophasor_synthetic.pcap in full.

Layout:
  - LEGACY blob: all 34 packets (predate generator code; add new
    sessions as section functions below)

Run from the tests directory:  python3 gen/gen_synchrophasor_synthetic.py
Each section function below documents the session(s) it generates.
"""

import base64
import struct
import sys
import zlib

# Packets accumulated before per-session generator code existed
# (zlib-compressed pcap prefix, including the 24-byte global header)
LEGACY = zlib.decompress(base64.b64decode(
    'eNrtlm9sU1UUwM997f51/1sY+wPjTWNQUme7sVENsnWOgGRooUjGEsGSdrTQrYurDD84CFkUkhFh'
    '6AcU2UbERIImmqCSkWiyDyNxGEnMDIkf1BhxJn5YFiMJifPce0/f2vtqGl/4aJPT19P37u/8fefe'
    '76Y/ndTADqnP0hIA4z8WghF+aSW5MjULoIlvVghbAB7lj7Xn995y4D8o2nTAGQWw/cZXBTQdLrXh'
    'jWwUJlkmisZJzuh0AKDgWyTNB5w6vLMPb1j1ZR5JtwMVSNmPGlG2kZgoTxFlKiulWoeJcVzlByeE'
    'I8EFm0hX3r2zduLWalLUGLUSyQ3PKzHeRvZ9zp17BVd52S+CK0tR3tEOWjAZSsYS/f74QDSk4wfC'
    'UA42pLFANDSYeNmrpz5Sb1L0ZkP394fiiYPLz3fG0hSpNyl6s6JvUPQWRW9V9I2K7lP0JxXd61F0'
    'r6I3KXqzom9Q9BZFT3cQZJMbsrTE+x4YZbwjkgzJ5yKYcY0/kyvjaoZVC1C/syvPagfeByi6xjul'
    'bz6tA/NEp2iPBfOJO0piesuGiTuU2YFF1wDspZz7fgxXMdgjuPlyEnQs0HUR4OnZ2VsdfwJs7sLr'
    'gPy/7fi/PJd2/9hogUXfHMUp3y4UpPmGv0+0PBjf8g4UWvUtlPLtGKT5VgjMfujB+Db+QxH51kti'
    '8q2bfHtP8e3zlG/d23FVJXiEb0WctTsymNQDO17Qqc11MVmeEN1+xNvogVcdFjvUXoqW73KrWl1a'
    'hzpEQOzcneIsE5wJLsuY4GGc4Ay5TJvxcW7xF6nd5K03AEqyUZhkmSgaJzmjMz4cphNIus53k5E5'
    'gFKrvlxH0iTfTUa+ByjLkqkMCmUqPJWVgpn68S/K1E3MVJnIlO1SZzlxb5CYYvyYuFuVGCeR/TXn'
    'lrhxVQO8Lrjlxm7Ctob6+kK8+qkJdVPMNo3PtozNAGdbxmaAs82r6Jn3TbOOy9HQoQqKJUpiiqWH'
    'Ynk3M5aKdalYhhfF+9UkYqnI/d60fQXwyYlKq3b3GnbH0+xWAuinc9s9W+Uku5tIuEVN9IQme6JR'
    '2K08cMOB/6BozqgzBrWNe3gfMDfacspTxdhlF7GSJJIiiYLVS6wRzuE8Zwz7a9f5oSte2C5YruXa'
    'd0biyfTaMzfWXhzz1H3NqN8mz8YVFuN57W0jnhVyj3rm4EpidZGY4tlMrKsZ8ay/+xHW4SHBWqnU'
    'n/If/LvKInu312BXAfjrzOwP9q2yyB79yWCvwj2r1cx2j1VbZB+eNNjVANM7zeySz2os1m7d70bt'
    'auT0Pl9Wm4VlEyxbBuslZNmQZdNcJ12noNb3sGCdQVat7OsRdx2xJpgUSZFEzmJjkrW/m3M4z3XK'
    'dZIN73oWT8hHBKtuua+3DAzG4ol+o7PZGTqvsf96Xvv/RGw+DUPN+sXVVK/nSdR6gZ9q/2F6veDx'
    'y1fx7PGIqNdqvp4qJY4fOxLhSFzv3tszVLrGYmt986XRWmvkiPnjzXpi9ZCYXO0i1sUMV30zR/F1'
    '8ghWfe4tBuDn59ZatDRxz7C0FuBCey5L8dO6RUsvHjYs6cB8x3NZct1psFiJX/1GJRrkwJhz/AM1'
    'DTf7'))




def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/synchrophasor_synthetic.pcap'
    out = LEGACY
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
