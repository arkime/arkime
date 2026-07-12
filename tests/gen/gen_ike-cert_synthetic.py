#!/usr/bin/env python3
"""Regenerate tests/pcap/ike-cert_synthetic.pcap in full.

Layout:
  - LEGACY blob: all 8 packets (predate generator code; add new
    sessions as section functions below)

Run from the tests directory:  python3 gen/gen_ike-cert_synthetic.py
Each section function below documents the session(s) it generates.
"""

import base64
import struct
import sys
import zlib

# Packets accumulated before per-session generator code existed
# (zlib-compressed pcap prefix, including the 24-byte global header)
LEGACY = zlib.decompress(base64.b64decode(
    'eNq7cnjTQiYGFgYEYGFgBFEfg1NBVAsjBE/Ryn/X05RVcHT9x58cDK4MjGUgZQ6CSZ5cjIwpQHyC'
    '8QsQJvkVCyoZu4SmlXes2n3m7rv/DIKMAkxQkxmj2BgYeEAMCGY0YjFoYtQ0aLzCxMnw/F/OERf1'
    'q/8MeNk4tdo82r7zMjJyszIYyBhKGUiwMYeyMPMIZmanlhnqpVYk5hbkpOol5+cayInzGpkZGBkY'
    'GZqbGJhYREG5xlAuAd0xyJYxsjIwezMYeDA5MqzatLeBY/LRtftOnnj4VXjb08e/LB4pbY18cf5T'
    'ttlR0/274jyi8yZm7Px7oFY5kfvYwjSNgr730Rm9DHtYWNgXXph4kYmZkYERzSPMjgyVbamds6Zm'
    'b1wUY978/Uv3tQ9rxKUF5LObyhwsJe7vL9aTbIrXmua8oUryyYxN9k+azGNO/F/Wkjltxd4j33u4'
    'rzyyevGFuDhROLBGgMEIiFMgccK8bmZHeVqoi7GSkElYxaw99z4oKigxQOMgShUSJxyM4GhHiZPT'
    'K2JnGXs2biQUJ0YUxYkRUXFyOKqFNfHt+ex9BxZMaCnlOBV+73tK8tqv9qUVHyu7r1z2878txHv1'
    'Z9luv8uTzk1bUSR3zv3ZvAf/FK+uniz6f+fJx/044iRzZUzdB15N9tvltkd6Li9ZOCfOLGVeI+Oj'
    'bZ8377qqYzH/Vd86dhFxaTcbja2CjVn7SxfMqPhrxap1ST1zs0zExq13LvyAxMkiRgj+DwaQ9N52'
    'NwEUojmCDgqMvDsQOYyRAZXPxACOpxxfRlCegeWfmUAOUt5JQcs7NqB4MjZovAeMp9XhbnMa3Yum'
    'oceToqG8gSwkpMXAqV83s6DMjKTIIsIIXDG2o++33L5LTi4znwXeyNefZVj7eOqlQL+dCSnerdfM'
    'bAq9c1v0Wm5+i352ntXNuuegAFt23j/1SAXmKdyy9TPyUgRW4oixxbmdH3rvRUaL3inQKOlv5u9O'
    '+ijaV7BYn7t8S0QET6ODm8+5nQ0qPj1n1PlPvq79xdzitdilzCWi2M72777I315aO/6QHmMMAegx'
    'CImxTa7gIg4pxpByVgpazkKOsaOfjl1kK/jvSSjGjCiPMSPiY+yE9NIKU3OelW3HTkSFzfM32HKq'
    'cJnuzHCuD1HqnLtCNreU/JZMOxYlYeqyX1h/xqmdMew7J66UD2N4IeC3Wt22JpsdR4xtv5jfvVC4'
    'aE1l+83LzK/D+iqNHh+9OMVWpbG6S95+6k7d/Pdf1wgKbjuom3Vm8+4tNatWpT84vPT4u3eKzFop'
    'r/RYJy5gCAHHmBcUg/MEE5hkBJV7DDaQci/NgcucgRGImY5bCE5h0Cg5dG/tvvcgVatwAJDJL5hJ'
    'N9k5A6REEAqUoIABnlsZZEAmX2BHM5kRYj6GyUwg0wWnHLcg1uQd3KS7ef+VU/927QOp3Y0DAABl'
    'qlbG'))




def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/ike-cert_synthetic.pcap'
    out = LEGACY
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
