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

`tests/pcap/arkime_synthetic.pcap` is regenerated **in full** from a committed generator,
`tests/gen/gen_arkime_synthetic.py`. This is the source of truth.

Do **NOT** hand-edit the `.pcap`, and do **NOT** append with throwaway scapy scripts. To add or
change a session you edit a section function in the generator and re-run it, so the pcap stays
reproducible and reviewable. Each session (or small group) is produced by a `sec_<name>()` function
that returns raw pcap-record bytes; `main()` concatenates a `LEGACY` blob plus every section in
order and writes the file.

The golden expected output, `tests/pcap/arkime_synthetic.test`, is regenerated separately with
`./tests.pl --make` (see below) — never by hand and never with a raw capture pipe.

## File Locations

- **Generator (edit this)**: `tests/gen/gen_arkime_synthetic.py`
- **Pcap (generated artifact)**: `tests/pcap/arkime_synthetic.pcap`
- **Golden output (generated artifact)**: `tests/pcap/arkime_synthetic.test`
- **Parser sources**: `capture/parsers/*.c` and `capture/*.c` — contain `arkime_field_define` calls

## Workflow: add or modify a session

Run all `python3`/`./tests.pl` commands from the `tests/` directory.

1. **Add a section function** `def sec_<name>():` in `gen/gen_arkime_synthetic.py`, modeled on an
   existing one (e.g. `sec_dns_https_empty_alpn`). It must return the concatenated per-packet pcap
   records — each is `struct.pack('<IIII', sec, usec, caplen, wirelen) + packet_bytes`. Sections use
   pure `struct` (no scapy) with local `csum()` / `build_udp()` / `eth_ip_tcp()` helpers that fill in
   IP total length, UDP/TCP length, and checksums.

2. **Register it in `main()`.** **Append the call LAST**, after the current final section. This keeps
   every existing packet at the same byte offset, so the golden `.test` diff is just your new session
   instead of a reshuffle of every `filePos`/`packetPos`.

3. **Update the header docstring** section list with a one-line entry for your section.

4. **Regenerate the pcap:**
   ```bash
   python3 gen/gen_arkime_synthetic.py
   ```

5. **Rebuild any parser you changed** so the golden reflects current behavior:
   ```bash
   (cd ../capture && make)
   # if make says "Nothing to be done", force it: touch ../capture/parsers/<x>.c then make
   ```

6. **Regenerate the golden `.test`** — use `--make`, NOT a manual pipe and NOT `--fix` directly
   (those produce the wrong, non-canonical format):
   ```bash
   ./tests.pl --make pcap/arkime_synthetic.pcap
   ```

7. **Validate the test passes:**
   ```bash
   ./tests.pl pcap/arkime_synthetic.pcap        # expect: ok 1 - pcap/arkime_synthetic
   ```

8. **Sanity-check the golden diff is minimal** (only your new session; deletions should be 0):
   ```bash
   git -C .. diff --numstat tests/pcap/arkime_synthetic.test
   ```

### Proving the test guards a parser fix (recommended)

If the session exists to lock in a parser fix, confirm it actually fails on the OLD behavior:
temporarily revert the parser line, `make`, run `./tests.pl pcap/arkime_synthetic.pcap` (expect
`not ok` with the exact field diff), then restore the fix and rebuild.

## Section function template (committed struct style)

```python
def sec_<name>():
    # One-line description of the session and what parser path / field it exercises.
    # Session 10.0.NN.1:PORT -> 10.0.NN.2:DPORT (UDP):
    TS_START = 1700009300.0                       # unique, after existing timestamps
    CMAC = bytes.fromhex('001122334466')
    SMAC = bytes.fromhex('66778899aabc')

    def csum(data):
        if len(data) & 1:
            data += b'\0'
        s = sum(struct.unpack('>%dH' % (len(data) // 2), data))
        while s >> 16:
            s = (s & 0xffff) + (s >> 16)
        return (~s) & 0xffff

    def build_udp(src_str, dst_str, sport, dport, payload):
        src = bytes(map(int, src_str.split('.')))
        dst = bytes(map(int, dst_str.split('.')))
        udp_len = 8 + len(payload)
        udp = struct.pack('!HHHH', sport, dport, udp_len, 0)
        pseudo = src + dst + struct.pack('!BBH', 0, 17, udp_len)
        udp = udp[:6] + struct.pack('!H', csum(pseudo + udp + payload))
        total_len = 20 + udp_len
        ip = struct.pack('!BBHHHBBH4s4s', 0x45, 0, total_len, 0x9101, 0, 64, 17, 0, src, dst)
        ip = ip[:10] + struct.pack('!H', csum(ip)) + ip[12:]
        return SMAC + CMAC + b'\x08\x00' + ip + udp + payload

    def build():
        pkts = [ build_udp('10.0.NN.1', '10.0.NN.2', PORT, DPORT, PAYLOAD) ]  # add both directions
        out = b''
        ts = TS_START
        for p in pkts:
            sec = int(ts); usec = int(round((ts - sec) * 1e6))
            out += struct.pack('<IIII', sec, usec, len(p), len(p)) + p
            ts += 0.1
        return out
    return build()
```

For TCP sessions, mirror the existing `eth_ip_tcp()`-based sections (e.g. `sec_certs_keyusage`,
`sec_imap_crsplit`) — they include a 3-way handshake and track seq/ack.

## Critical rules

- **Edit the generator, never the `.pcap` by hand;** re-run `python3 gen/gen_arkime_synthetic.py`.
- **Append new sections LAST** in `main()` to avoid churning existing `filePos`/`packetPos` in the golden.
- **Regenerate the golden with `./tests.pl --make`** (canonical, pretty-printed, sorted). A raw
  `capture | ./tests.pl --fix` redirect produces the wrong format and will clobber the golden.
- **Unique 5-tuple per session**: a unique `10.X.Y.z` subnet + ports so sessions stay separate.
- **Unique, non-overlapping timestamps** after existing ones.
- **Content-based matching**: parsers classify on payload content/magic bytes, not port numbers.
- **At least 2 packets** (both directions) per session to exercise request and response.

## Validate no malformed / truncated packets

```bash
tshark -r tests/pcap/arkime_synthetic.pcap -T fields -e _ws.malformed 2>&1 | grep -i malform    # zero output
tcpdump -nn -r tests/pcap/arkime_synthetic.pcap 2>&1 | grep -iE "truncat|invalid|missing|bad"    # zero output
```

## Finding fields to cover

### List all field definitions for a parser
```bash
grep -n "arkime_field_define" capture/parsers/PROTOCOL.c
```

### Check which fields are already exercised
```bash
cd capture
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
