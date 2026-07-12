#!/usr/bin/env python3
"""Regenerate tests/pcap/s7comm_synthetic.pcap in full.

Layout:
  - LEGACY blob: all 40 packets (predate generator code; add new
    sessions as section functions below)

Run from the tests directory:  python3 gen/gen_s7comm_synthetic.py
Each section function below documents the session(s) it generates.
"""

import base64
import struct
import sys
import zlib

# Packets accumulated before per-session generator code existed
# (zlib-compressed pcap prefix, including the 24-byte global header)
LEGACY = zlib.decompress(base64.b64decode(
    'eNqd119sU1UcB/DfPfe2ve0d0m04JvNP1eDgjY0wYhTslMVsOBxMAgQfeND6yGpEjU81IUPetvhC'
    '4gxdcFEhuDKBgZKsA3wwipSoER4wfdDxJzEZxhcTk/n7nXNP+Z1zqw9dOGV06+f3/d2e37nlp0sz'
    'kwI80F9LSwAOfXNv+HX6qydc0PzE+i1CPjo+9AGsoV/LxnNXUvgCXKIMkANwb9MvD4kMzI4r5bZr'
    'KY6yIoogCXLoQKKC0p2hlgxMl5RSSTSQ5Q5K14bSqJxSSroT4CVQK6JsDpXZukp7Blq/w38BtDVX'
    'gZKWndS8cOCicATZmW2WzfvkNu/zGopL0r4c2hX8WcRedwBgENSK5H4utM9aufGdTNwie9/T0l4p'
    'FgvdjoweB1gEp+pUCc9OALwMakWC94b4aSv4LQCvjfCejMQfItzVOPHgK37oEsBOUCuS/cWQ/8LM'
    '7rXhy4H45AuSf0xnF/AA/thpSaUFvfRguFX33cVNB2pFeng+LPKl2YOPy9tARfJ/yyKrdA9CdeEt'
    'eeCfuEL+2Ixvbj9PPrrG9vsWbReXV3aoieCcHoXpCyrlu2ctxVVWRPFIwpT4zPIiSudpFIpB2OtX'
    'DWQ5j9IkjUKxSSnFG765XbmymV2xqIJXTDzFRkGo7Spwu+KeRbsESdPmfXKb9zmJ/lVpP1obBRG1'
    'y51JcxR4bj0KM1buq+jPkr39Q2MU8I8xCpVtSXMUeHA9CiUr+CwWWCC88z1jFBTOR6F6IAl78Zm9'
    '9bIPhvwnVvYFLFEhvvuU5Ffr7A6OQjzGR8HHg9z/9SSVWpxIwlassrVeJ5vYUPNO8NRtCqiUV5Kl'
    'HtSd0JHkvO/FUvK2cDkJA6gO1KOfDekzFv27pleod3cF0QJpoW43MYdC9wtzZ8flY8zY2d+gikEg'
    'XhZ0fVo/0FP2KZ5JpFwftJSYsiJKnCTMhzFWjqB0mKZsLKeU8vYGshxGKU9TNvaGUiAvzEngip6E'
    '6boKXqu/CmzKXHZTkHb6iGXzPrnN+8yjf1Ta79SmzI3amTlhbh+eexMbBJ77KPqHyH4lWds+b7qQ'
    '8o5//f3NLpKIXrdg0Tw2p3nsQ8ifJnq006Tppd2azi5zoR+/769HP8POBk5PaPrtdkm3KtoXO3Pr'
    'NTzyc8rcDL58TBibYR7RBC6/7NLl6NitN2axRSm7b1hKQlkRxScJ49GNL4vSHtqYhfmwyZsNZNmD'
    'Ui9tzMJFpRT+SZmbhyt685yoq+Cl+u0625ge/7RC9tiTgWnzPrnN++xFf7+0f6htTC9qFwcC8/jn'
    'ufXxf9zKvR/9XWR3DfznJyHCS/nAPP55cH38f2YF34UFRglPdP3PJyF5pBwJYBifGa6XvY8d/zz7'
    'KJYYIf6jHyX/CPEJedel495p8Zrxw04q3KiVuQAwkFyRDvrDEkWrAzz7Oo5RiVWrZYnHeQmflXB6'
    '+oY3UpnqQgA78Bd21OtkS1hmyurkGJYapzJ3P5ZlHtbvggsB7dzal7wVLGsybzK8EX2TOWk1gv/j'
    '6JijCn+O37/JuLKCvMmskUfyhibAj+pyRbJnQ/lzK/sc6lMk//GWlNt1dg+Lw9r7udOvNpB7Civ8'
    'Qvq918zcnsq99l9JkwFh'))




def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/s7comm_synthetic.pcap'
    out = LEGACY
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
