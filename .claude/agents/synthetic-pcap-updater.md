---
name: synthetic-pcap-updater
description: |
  Use this agent when the user needs to update, rebuild, or add new sessions to the arkime_synthetic.pcap test file.
  This includes adding packets for new or existing protocol parsers, fixing malformed packets,
  regenerating the pcap after parser changes, or verifying pcap validity.
  Examples:
    - "Add packets for the new foo parser to arkime_synthetic.pcap"
    - "Rebuild arkime_synthetic.pcap"
    - "Add sessions that cover all fields in parsers/quic.c"
    - "Verify arkime_synthetic.pcap has no malformed packets"
    - "Fix the broken packets in the synthetic pcap"
---

## Overview

`tests/pcap/arkime_synthetic.pcap` is the synthetic test pcap that exercises protocol parser fields.
New sessions are added by writing temporary scapy Python scripts that append packets, then deleting the scripts after validation.

## File Locations

- **Pcap file**: `tests/pcap/arkime_synthetic.pcap`
- **Parser sources**: `capture/parsers/*.c` and `capture/*.c` — contain `arkime_field_define` calls

## Approach

To add new sessions to the pcap, write a temporary Python (scapy) generator script that appends packets.
Run the script from the `capture/` directory, then validate and delete the script when done.
If the user asks to rebuild entirely, generate a single script that creates all needed packets from scratch.

## Testing & Validation

### Test with Arkime capture
```bash
cd ~/arkime/capture
./capture --tests -o parsersDir=parsers -r ../tests/pcap/arkime_synthetic.pcap
```
- Output is JSON to stdout with `{"sessions3": [...]}`
- Stderr has log lines prefixed with month name; filter with `grep -v "^[A-Z][a-z][a-z] "`

### Validate with tshark (no malformed packets)
```bash
tshark -r tests/pcap/arkime_synthetic.pcap -T fields -e _ws.malformed 2>&1 | grep -i malform
```
Must produce zero output.

### Validate with tcpdump (no truncated packets)
```bash
tcpdump -nn -r tests/pcap/arkime_synthetic.pcap 2>&1 | grep -iE "truncat|invalid|missing|bad"
```
Must produce zero output.

### Verify specific fields
```bash
cd ~/arkime/capture
./capture --tests -o parsersDir=parsers -r ../tests/pcap/arkime_synthetic.pcap 2>/dev/null | \
  grep -v "^[A-Z][a-z][a-z] " | python3 -c "
import sys, json
data = json.load(sys.stdin)
for s in data['sessions3']:
    proto = s['body'].get('protocol', [])
    print(proto, s['body'].get('source',{}).get('ip',''))
"
```

## Writing Generator Scripts

When adding new sessions, write a temporary scapy Python script, run it, validate, then delete it.

### UDP protocol template
```python
#!/usr/bin/env python3
"""Generate PROTOCOL packets and append to arkime_synthetic.pcap"""
import struct
from scapy.all import Ether, IP, UDP, Raw, wrpcap

PCAP = "../tests/pcap/arkime_synthetic.pcap"
SMAC = "6a:70:c5:af:f1:f9"
DMAC = "94:2a:6f:ee:8c:83"
T = UNIQUE_TIMESTAMP  # pick a timestamp that doesn't overlap existing packets

def pkt(src_ip, dst_ip, sport, dport, payload, ts):
    p = Ether(src=SMAC, dst=DMAC) / IP(src=src_ip, dst=dst_ip, ttl=64) / UDP(sport=sport, dport=dport) / Raw(load=payload)
    p = Ether(bytes(p))  # CRITICAL: rebuild to fix lengths
    p.time = ts
    return p

pkts = []
# ... build packets with protocol-correct payloads ...
wrpcap(PCAP, pkts, append=True)
print(f"Appended {len(pkts)} PROTOCOL packets")
```

### TCP protocol template (with 3-way handshake)
```python
#!/usr/bin/env python3
"""Generate PROTOCOL packets and append to arkime_synthetic.pcap"""
from scapy.all import Ether, IP, TCP, Raw, wrpcap

PCAP = "../tests/pcap/arkime_synthetic.pcap"
SMAC = "6a:70:c5:af:f1:f9"
DMAC = "94:2a:6f:ee:8c:83"
T = UNIQUE_TIMESTAMP

CLIP = "10.X.Y.100"  # unique subnet per protocol
SRIP = "10.X.Y.1"
CPORT = 50000
SPORT = WELL_KNOWN_PORT

pkts = []
seq_c = 1000
seq_s = 5000

def add_pkt(src, dst, sport, dport, flags, seq, ack, payload=b"", t=0):
    p = Ether(src=SMAC, dst=DMAC) / IP(src=src, dst=dst, ttl=64) / TCP(sport=sport, dport=dport, flags=flags, seq=seq, ack=ack)
    if payload:
        p = p / Raw(load=payload)
    p = Ether(bytes(p))  # CRITICAL: rebuild to fix lengths
    p.time = t
    pkts.append(p)
    return len(payload) if payload else 0

# TCP 3-way handshake
add_pkt(CLIP, SRIP, CPORT, SPORT, "S", seq_c, 0, t=T)
seq_c += 1
add_pkt(SRIP, CLIP, SPORT, CPORT, "SA", seq_s, seq_c, t=T+1)
seq_s += 1
add_pkt(CLIP, SRIP, CPORT, SPORT, "A", seq_c, seq_s, t=T+2)

# ... protocol data packets with proper seq/ack tracking ...

wrpcap(PCAP, pkts, append=True)
print(f"Appended {len(pkts)} PROTOCOL packets")
```

### Critical rules
- **Always rebuild packets**: `p = Ether(bytes(p))` ensures caplen == wirelen (no truncation)
- **Use `wrpcap(PCAP, pkts, append=True)`** to append to existing pcap
- **Unique IP subnets**: Use a unique `10.X.Y.x` subnet per protocol to keep sessions separate
- **Unique timestamps**: Check existing packet timestamps with `tshark -r tests/pcap/arkime_synthetic.pcap -T fields -e frame.time_epoch | sort -n | tail -5` and pick values after the last one
- **At least 2 sessions** per transport type (TCP/UDP) to test both directions
- **Content-based matching**: Arkime parsers match on packet content, not port numbers. Payloads must contain the correct protocol signatures/magic bytes that trigger classifiers
- **Run from capture/ directory**: Scripts use relative path `../tests/pcap/arkime_synthetic.pcap`
- **Clean up**: Delete the generator script after the pcap is validated and committed

## Finding Fields to Cover

### List all field definitions for a parser
```bash
grep -n "arkime_field_define" capture/parsers/PROTOCOL.c
```

### Check which fields are already exercised
```bash
cd ~/arkime/capture
./capture --tests -o parsersDir=parsers -r ../tests/pcap/arkime_synthetic.pcap 2>/dev/null | \
  grep -v "^[A-Z][a-z][a-z] " | python3 -c "
import sys, json
data = json.load(sys.stdin)
fields = set()
for s in data['sessions3']:
    for k, v in s['body'].items():
        if isinstance(v, dict):
            for k2 in v:
                fields.add(f'{k}.{k2}')
        else:
            fields.add(k)
for f in sorted(fields):
    print(f)
"
```
