#!/usr/bin/env python3
"""Regenerate tests/pcap/tcpseq_synthetic.pcap in full.

Layout:
  - LEGACY blob: all 39 packets (predate generator code; add new
    sessions as section functions below)

Run from the tests directory:  python3 gen/gen_tcpseq_synthetic.py
Each section function below documents the session(s) it generates.
"""

import base64
import struct
import sys
import zlib

# Packets accumulated before per-session generator code existed
# (zlib-compressed pcap prefix, including the 24-byte global header)
LEGACY = zlib.decompress(base64.b64decode(
    'eNqtlV1IFFEUx89+tK4rpCZkEDQLfShitilYrFD7MbuQSS2BlH1B2KcR6UNCECTRwwrmQ7QVMoGC'
    '1EsghZJLmSsiUVAoRK/qS7gPmkok9tB2zj1zLXQfxosHzu7A3Ps7//M/M3e+jr7usYMT/oUTbPT3'
    'tOZqwAA4A5xgywAsvcB7BeCGugqoE4tdADvEhWNlv7+OQ59shP8D97fBnqbGs3ZRoMtHv/59mQyV'
    'Ku3eyFKb6YdK+YjWJQqWylJ0r9rMR2U35zruNTWP9S0suyGCq2hpwHX5swc3YtqNoBbAkjO0K2b3'
    'wrtW9mbGYZFiJ5IWMIIAOeNISse2eKE/wZTxHAUtaSRNxAqQ8pgpg3loBXCuoZSblI9ZKdu8UOkG'
    'CGIQqTtfoasJpM0KPQ9ZT7xIQc+s1OOzA4QwiNRcrKhnQejpZD0nt1ugkB4HptMIaUjJG5QT7+9m'
    'ymHNIsVJJC1ohADycW9ekibe+4MpJTsVtCSR1EMd9c4zJb/EgsNjqyhDgoIOL+4FaMAg0lKZAikp'
    'ST+rAE5hEGmyQsGfHqSNiM5muLMPlRYopGcTpssIa0gpistZvfzElL5qixQXkbSQEQYobkZSO80q'
    'cYQpT/wKWtqR1EIdJWqZcveQBYdHslLQ4Sm8Oo1BpCuhjSIdjyr404K0B6KzKHd2oNYChfTkYLoN'
    'XQtnMpkpOavUdaZoxyxS3ETSwoaOCwuQNE2zSpkuu04oaJnGxYXUUeooU+bqAXBcItdQfCYlmZWC'
    'Dr+t5pNUnqbfGhQ6K0RiWmgKs6b35xQ0paWmN2V8msoT9fkFRU3iRE3VsKbOSwD0EESzUQ6alFer'
    'NC1ITbvxSQyb0WgGUVuvqWijT7zQVs7azt9Yr19EkNr68Fuqm0G0aIuiplyhaRdrqrhlgTKEhFxM'
    'jxHRdPRrfuVNqWfK1tsWKR4iaboRocbE7PBNGdaZ8ueOghacnU24PBxhyvc2Cy4PZKWgy7/xrN1v'
    'BtG+3F8vjWfGtEV8z6rMINpAXEHbsqT9ughQaQbRnnUouG6TT8AwfhX/Apybp/I='))




def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else 'pcap/tcpseq_synthetic.pcap'
    out = LEGACY
    with open(outpath, 'wb') as f:
        f.write(out)
    print('Created ' + outpath)


if __name__ == '__main__':
    main()
