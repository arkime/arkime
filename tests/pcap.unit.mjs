/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for viewer/pcap.js decoding. Run with `npm run test:unit`.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { createRequire } from 'node:module';

const cjsRequire = createRequire(import.meta.url);
const pcapPath = cjsRequire.resolve('../viewer/pcap.js');
const Pcap = cjsRequire(pcapPath);

// An ip6 udp packet from fd00::1 port 1000 to fd00::99 port 53, with the given extension
// headers (next header values) between the ip6 header and udp
function ip6Packet (exts) {
  const udp = Buffer.from('03e80035000c0000' + '41424344', 'hex');
  const parts = [];
  for (let i = 0; i < exts.length; i++) {
    // 8 byte header, the first byte is the next header after this one
    const ext = Buffer.alloc(8);
    ext[0] = i + 1 < exts.length ? exts[i + 1] : 17;
    parts.push(ext);
  }
  const payload = Buffer.concat([...parts, udp]);
  const hdr = Buffer.alloc(40);
  hdr[0] = 0x60;
  hdr.writeUInt16BE(payload.length, 4);
  hdr[6] = exts.length ? exts[0] : 17;
  hdr[7] = 64;
  Buffer.from('fd000000000000000000000000000001', 'hex').copy(hdr, 8);
  Buffer.from('fd000000000000000000000000000099', 'hex').copy(hdr, 24);
  return Buffer.concat([hdr, payload]);
}

describe('Pcap.ip6 extension headers', () => {
  const chains = [[0], [60], [43], [0, 60], [0, 43, 60]];

  test('a whole packet decodes through the extension headers to udp', () => {
    for (const exts of chains) {
      const obj = {};
      new Pcap('unit').ip6(ip6Packet(exts), obj, 0);
      assert.equal(obj.udp?.sport, 1000, exts.join(','));
      assert.equal(obj.udp?.dport, 53, exts.join(','));
    }
  });

  test('a packet cut off anywhere in the extension headers does not hang', () => {
    // A hang can't be stopped from inside this process, so decode in a child with a timeout
    const script = `
      const Pcap = require(${JSON.stringify(pcapPath)});
      const packets = ${JSON.stringify(chains.map(exts => ip6Packet(exts).toString('hex')))};
      for (const hex of packets) {
        const buf = Buffer.from(hex, 'hex');
        for (let len = 40; len <= buf.length; len++) {
          try { new Pcap('unit').ip6(buf.subarray(0, len), {}, 0); } catch (e) { }
        }
      }
      console.log('done');
    `;
    const result = spawnSync(process.execPath, ['-e', script], { timeout: 10000, encoding: 'utf8' });
    assert.equal(result.signal, null, 'decoding a cut off packet hung');
    assert.equal(result.stdout.trim(), 'done', result.stderr);
  });
});

// A udp datagram from port 1000 to port 53
function udpDatagram (data) {
  const hdr = Buffer.from('03e8003500000000', 'hex');
  hdr.writeUInt16BE(8 + data.length, 4);
  return Buffer.concat([hdr, data]);
}

// An ip4 packet from 10.0.0.1 to 10.0.0.2, off is in bytes
function ip4Packet (id, off, more, payload, proto = 17) {
  const hdr = Buffer.alloc(20);
  hdr[0] = 0x45;
  hdr.writeUInt16BE(20 + payload.length, 2);
  hdr.writeUInt16BE(id, 4);
  hdr.writeUInt16BE((more ? 0x2000 : 0) + (off / 8), 6);
  hdr[8] = 64;
  hdr[9] = proto;
  Buffer.from('0a0000010a000002', 'hex').copy(hdr, 12);
  return Buffer.concat([hdr, payload]);
}

// An ip6 fragment from fd00::1 to fd00::99, optionally with destination options first
function ip6Fragment (id, off, more, payload, destopt) {
  const frag = Buffer.alloc(8);
  frag[0] = 17;
  frag.writeUInt16BE(off + (more ? 1 : 0), 2);
  frag.writeUInt32BE(id, 4);
  const parts = [frag, payload];
  if (destopt) {
    const ext = Buffer.alloc(8);
    ext[0] = 44;
    parts.unshift(ext);
  }
  const rest = Buffer.concat(parts);
  const hdr = Buffer.alloc(40);
  hdr[0] = 0x60;
  hdr.writeUInt16BE(rest.length, 4);
  hdr[6] = destopt ? 60 : 44;
  hdr[7] = 64;
  Buffer.from('fd000000000000000000000000000001', 'hex').copy(hdr, 8);
  Buffer.from('fd000000000000000000000000000099', 'hex').copy(hdr, 24);
  return Buffer.concat([hdr, rest]);
}

// Split payload every size bytes, mk(off, more, data) makes each packet
function fragments (payload, size, mk) {
  const out = [];
  for (let off = 0; off < payload.length; off += size) {
    out.push(mk(off, off + size < payload.length, payload.subarray(off, off + size)));
  }
  return out;
}

// Decode each buffer the way a session's packets are, n is kept to check the order after
function decode (buffers, v = 4) {
  return buffers.map((buf, n) => {
    const obj = { n };
    if (v === 4) {
      new Pcap('unit').ip4(buf, obj, 0);
    } else {
      new Pcap('unit').ip6(buf, obj, 0);
    }
    return obj;
  });
}

describe('Pcap.defrag', () => {
  const data = Buffer.from('The quick brown fox jumps over the lazy dog, twice over.');
  const dgram = udpDatagram(data);
  const whole = ip4Packet(1, 0, false, udpDatagram(Buffer.from('whole')));
  const frags4 = (id, size = 16) => fragments(dgram, size, (off, more, d) => ip4Packet(id, off, more, d));

  function assertUdp (packet, msg) {
    assert.equal(packet.ip.frag, undefined, msg);
    assert.equal(packet.udp?.sport, 1000, msg);
    assert.equal(packet.udp?.dport, 53, msg);
    assert.deepEqual(packet.udp?.data, data, msg);
  }

  test('packets that are not fragments are left alone', () => {
    const packets = decode([whole, whole]);
    const before = [...packets];
    assert.equal(Pcap.defrag(packets), packets);
    assert.deepEqual(packets, before);
  });

  test('ip4 fragments become the datagram, where the fragment that completed it was', () => {
    const f = frags4(7);
    assert.ok(f.length >= 3);
    const packets = Pcap.defrag(decode([whole, ...f, whole]));
    assert.deepEqual(packets.map(p => p.n), [0, f.length, f.length + 1]);
    assertUdp(packets[1]);
    assert.equal(packets[1].ip.len, 20 + dgram.length);
    assert.equal(packets[1].ip.off, 0);
    assert.equal(packets[2].udp.data.toString(), 'whole');
  });

  test('ip4 fragments in any order, with duplicates', () => {
    const f = frags4(7);
    const orders = {
      reverse: [...f].reverse(),
      'last first': [f[f.length - 1], ...f.slice(0, -1)],
      'duplicate first': [f[0], f[0], ...f.slice(1)],
      'duplicate middle': [f[0], f[1], f[1], ...f.slice(2)]
    };
    for (const [label, order] of Object.entries(orders)) {
      const packets = Pcap.defrag(decode(order));
      assert.equal(packets.length, 1, label);
      assert.equal(packets[0].n, order.length - 1, label);
      assertUdp(packets[0], label);
    }
  });

  test('a datagram sent again with the same ip id is two datagrams', () => {
    const f = frags4(0);
    const packets = Pcap.defrag(decode([...f, ...f]));
    assert.equal(packets.length, 2);
    assertUdp(packets[0]);
    assertUdp(packets[1]);
  });

  test('datagrams with different ip ids can be mixed together', () => {
    const a = frags4(1);
    const b = frags4(2);
    const mixed = [];
    for (let i = 0; i < a.length; i++) {
      mixed.push(a[i], b[i]);
    }
    const packets = Pcap.defrag(decode(mixed));
    assert.equal(packets.length, 2);
    assertUdp(packets[0]);
    assertUdp(packets[1]);
  });

  test('a fragmented tcp segment is decoded as tcp', () => {
    const tcp = Buffer.concat([Buffer.from('03e80050000000010000000250180400' + '00000000', 'hex'), data]);
    const packets = Pcap.defrag(decode(fragments(tcp, 24, (off, more, d) => ip4Packet(9, off, more, d, 6))));
    assert.equal(packets.length, 1);
    assert.equal(packets[0].ip.p, 6);
    assert.equal(packets[0].tcp?.dport, 80);
    assert.deepEqual(packets[0].tcp?.data, data);
  });

  test('ip6 fragments become the datagram, extension headers before the fragment header stay', () => {
    for (const destopt of [false, true]) {
      const f = fragments(dgram, 16, (off, more, d) => ip6Fragment(0x1234, off, more, d, destopt));
      for (const order of [f, [...f].reverse()]) {
        const packets = Pcap.defrag(decode(order, 6));
        assert.equal(packets.length, 1, `destopt ${destopt}`);
        assertUdp(packets[0], `destopt ${destopt}`);
        assert.equal(packets[0].ip.len, dgram.length + (destopt ? 8 : 0));
      }
    }
  });

  test('fragments in a tunnel that is itself fragmented', () => {
    // Each inner ip4 fragment is in gre in an outer ip4 datagram that is split in two
    const wire = [];
    frags4(7, 24).forEach((inner, i) => {
      const gre = Buffer.concat([Buffer.from('00000800', 'hex'), inner]);
      wire.push(...fragments(gre, 24, (off, more, d) => ip4Packet(100 + i, off, more, d, 47)));
    });
    const packets = Pcap.defrag(decode(wire));
    assert.equal(packets.length, 1);
    assert.equal(packets[0].n, wire.length - 1);
    assert.equal(packets[0].gre?.type, 0x0800);
    assertUdp(packets[0]);
  });

  test('a datagram with a hole is left as fragments, only the first has the transport header', () => {
    const f = frags4(7);
    f.splice(1, 1);
    const packets = Pcap.defrag(decode([whole, ...f]));
    assert.equal(packets.length, 1 + f.length);
    assert.equal(packets[1].udp?.sport, 1000);
    assert.deepEqual(packets[1].udp?.data, data.subarray(0, 8));
    for (const packet of packets.slice(2)) {
      assert.equal(packet.ip.p, 0);
      assert.equal(packet.udp, undefined);
      assert.ok(packet.ip.frag);
    }
  });

  test('when not all the packets were read the fragments at the end are dropped', () => {
    const f = frags4(7);
    const cut = [whole, ...frags4(8), whole, ...f.slice(0, -1)];
    // All of the session, the fragments are what was saved so they stay
    assert.equal(Pcap.defrag(decode(cut)).length, 3 + f.length - 1);
    // Cut off, they are the start of a datagram the rest of wasn't read
    const packets = Pcap.defrag(decode(cut), true);
    assert.deepEqual(packets.map(p => p.n), [0, f.length, f.length + 1]);
    assertUdp(packets[1]);
    // A complete datagram at the end stays
    assert.equal(Pcap.defrag(decode([whole, ...f]), true).length, 2);
    // Nothing but part of one datagram, better to show it than nothing
    assert.equal(Pcap.defrag(decode(f.slice(0, -1)), true).length, f.length - 1);
  });
});

describe('Pcap.defrag ip header', () => {
  test('the ip header comes from the first fragment, like hosts and capture', () => {
    // Later fragments say tcp, the first says udp, the datagram is udp
    const dgram = udpDatagram(Buffer.from('first fragment header wins, not the last one'));
    const f = fragments(dgram, 16, (off, more, d) => ip4Packet(3, off, more, d, off === 0 ? 17 : 6));
    const packets = Pcap.defrag(decode(f));
    assert.equal(packets.length, 1);
    assert.equal(packets[0].ip.p, 17);
    assert.equal(packets[0].udp?.dport, 53);
  });
});

describe('reassembling sessions with left over fragments', () => {
  // Decode like a session read from pcap, the reassemblers use the pcap timestamp
  function sessionPackets (buffers) {
    return Pcap.defrag(decode(buffers).map(p => ({ ...p, pcap: { ts_sec: 1700000000, ts_usec: p.n } })));
  }

  // A datagram with its middle fragment missing, as when saving stopped part way through it
  function withHole (payload, proto) {
    const f = fragments(payload, 16, (off, more, d) => ip4Packet(5, off, more, d, proto));
    assert.ok(f.length >= 3);
    f.splice(1, 1);
    return f;
  }

  const data = Buffer.from('The quick brown fox jumps over the lazy dog, twice over.');

  for (const [kind, proto, payload] of [
    ['udp', 17, udpDatagram(data)],
    ['icmp', 1, Buffer.concat([Buffer.from('0800000000010001', 'hex'), data])]
  ]) {
    test(`${kind}: the fragments stay with their side and nothing throws`, () => {
      const whole = kind === 'udp' ? ip4Packet(1, 0, false, udpDatagram(Buffer.from('whole'))) : ip4Packet(1, 0, false, Buffer.from('0800000000010002abcd', 'hex'), 1);
      const left = withHole(payload, proto);
      const packets = sessionPackets([whole, ...left, whole]);
      assert.equal(packets.length, 2 + left.length);
      assert.ok(packets.some(p => p.ip.frag && !p[kind]), 'has a left over fragment without the transport layer');
      const { err, results } = Pcap[`reassemble_${kind}`](packets, 200);
      assert.equal(err, undefined);
      // Every packet is from 10.0.0.1, so it is one side and one result
      assert.equal(results.length, 1);
      assert.ok(results[0].data.length > 0);
    });
  }

  test('esp: a left over fragment does not throw', () => {
    const esp = Buffer.concat([Buffer.from('0000000100000001', 'hex'), data]);
    const { err, results } = Pcap.reassemble_esp(sessionPackets(withHole(esp, 50)), 200);
    assert.equal(err, undefined);
    assert.equal(results.length, 1);
  });
});

describe('Pcap.defrag bounds', () => {
  // Keep the warnings out of the test output, and count them
  function quietly (fn) {
    const log = console.log;
    const trace = console.trace;
    let warnings = 0;
    console.log = console.trace = () => { warnings++; };
    try {
      return { result: fn(), warnings };
    } finally {
      console.log = log;
      console.trace = trace;
    }
  }

  test('a datagram that is all there but does not decode is left as fragments and only warned about once', () => {
    // ip6 in ip4 whose ip6 says an 8 byte ip4 follows, which can't be decoded. Each datagram
    // uses the same ip id, before every one was tried with all the ones before it.
    const ip6 = Buffer.alloc(48);
    ip6[0] = 0x60;
    ip6.writeUInt16BE(8, 4);
    ip6[6] = 4;
    const count = 16000;
    const buffers = [];
    for (let i = 0; i < count; i++) {
      buffers.push(...fragments(ip6, 24, (off, more, d) => ip4Packet(9, off, more, d, 41)));
    }
    const packets = decode(buffers);
    const start = Date.now();
    const { warnings } = quietly(() => Pcap.defrag(packets));
    assert.ok(Date.now() - start < 3000, `took ${Date.now() - start}ms`);
    assert.equal(warnings, 1);
    assert.equal(packets.length, 2 * count);
    assert.ok(packets.every(p => p.ip.frag));
  });

  test('a group of fragments bigger than one datagram can be is given up on', () => {
    // First fragments that never get a last one, capture never saves more than 256 for a datagram
    const first = ip4Packet(4, 0, true, Buffer.alloc(16, 1));
    const packets = decode(Array(5000).fill(first));
    const start = Date.now();
    quietly(() => Pcap.defrag(packets));
    assert.ok(Date.now() - start < 3000, `took ${Date.now() - start}ms`);
    assert.equal(packets.length, 5000);
  });
});

describe('Pcap.scrubPacket', () => {
  test('the first fragment of a fragmented tunnel keeps the inner headers', async () => {
    // ether, ip4 gre fragment, gre, inner ip4, udp, then data that should be scrubbed
    const data = Buffer.alloc(40, 0x41);
    const inner = ip4Packet(1, 0, false, udpDatagram(data));
    const outer = ip4Packet(77, 0, true, Buffer.concat([Buffer.from('00000800', 'hex'), inner]), 47);
    const frame = Buffer.concat([Buffer.from('020000000002020000000001' + '0800', 'hex'), outer]);
    const headersLen = 14 + 20 + 4 + 20 + 8;

    const file = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'pcapunit-')), 'scrub.pcap');
    const ghdr = Buffer.alloc(24);
    ghdr.writeUInt32LE(0xa1b2c3d4, 0);
    ghdr.writeUInt16LE(2, 4);
    ghdr.writeUInt16LE(4, 6);
    ghdr.writeUInt32LE(65535, 16);
    ghdr.writeUInt32LE(1, 20);
    const rhdr = Buffer.alloc(16);
    rhdr.writeUInt32LE(1700000000, 0);
    rhdr.writeUInt32LE(frame.length, 8);
    rhdr.writeUInt32LE(frame.length, 12);
    fs.writeFileSync(file, Buffer.concat([ghdr, rhdr, frame]));

    try {
      const pcap = new Pcap(file);
      pcap.openReadWrite({ name: file });
      const obj = {};
      pcap.decode(await pcap.readPacket(24), obj);
      assert.ok(obj.ip.frag, 'decodes as a fragment');
      pcap.scrubPacket(obj, 24, Buffer.alloc(65536, 0xee), false);
      fs.closeSync(pcap.fd);

      const after = fs.readFileSync(file).subarray(40);
      assert.deepEqual(after.subarray(0, headersLen), frame.subarray(0, headersLen), 'headers through the inner udp are kept');
      assert.ok(after.subarray(headersLen).every(b => b === 0xee), 'the inner data is scrubbed');
    } finally {
      fs.rmSync(path.dirname(file), { recursive: true });
    }
  });
});
