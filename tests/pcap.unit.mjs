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

  test('fragments are put together with fragments from the same ip layer', () => {
    // An ip4 in ip4 tunnel where the outer and inner headers have the same addresses and id.
    // The inner datagram is fragmented, its first fragment is in a fragmented outer packet,
    // its second in one that isn't, like hosts each layer is reassembled on its own.
    const inner = fragments(dgram, 32, (off, more, d) => ip4Packet(7, off, more, d));
    assert.equal(inner.length, 2);
    const outer1 = fragments(ip4Packet(7, 0, false, inner[0], 4).subarray(20), 32, (off, more, d) => ip4Packet(7, off, more, d, 4));
    const outer2 = ip4Packet(8, 0, false, inner[1], 4);
    const packets = Pcap.defrag(decode([...outer1, outer2]));
    assert.equal(packets.length, 1);
    assertUdp(packets[0]);

    // Both inner fragments in fragmented outer packets with the same id
    const outer2f = fragments(ip4Packet(7, 0, false, inner[1], 4).subarray(20), 16, (off, more, d) => ip4Packet(7, off, more, d, 4));
    const both = Pcap.defrag(decode([...outer1, ...outer2f]));
    assert.equal(both.length, 1);
    assertUdp(both[0]);
  });

  test('two datagrams with the same id saved mixed together are kept apart', () => {
    // Capture merges the fragments of outer datagrams in time order when an inner datagram
    // is made from them, so another datagram's fragments can come before this one is done.
    // When their bytes conflict, like capture they aren't one datagram.
    const other = udpDatagram(Buffer.from('Some other words that are just as long as the dog!'));
    const a = fragments(dgram, 32, (off, more, d) => ip4Packet(7, off, more, d));
    const b = fragments(other, 16, (off, more, d) => ip4Packet(7, off, more, d));
    const packets = Pcap.defrag(decode([b[1], ...a, b[0], ...b.slice(2)]));
    assert.equal(packets.length, 2);
    assertUdp(packets[0]);
    assert.deepEqual(packets[1].udp?.data, other.subarray(8));
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
    // Every datagram uses the same ip id, before each one was tried with all the ones before it
    const count = 16000;
    const dgram = udpDatagram(Buffer.alloc(40, 0x41));
    const buffers = [];
    for (let i = 0; i < count; i++) {
      buffers.push(...fragments(dgram, 24, (off, more, d) => ip4Packet(9, off, more, d)));
    }
    const packets = decode(buffers);
    // Putting them back together throws, as a decoder bug would
    const broken = { ip4 () { throw new Error('broken'); }, ip6 () { throw new Error('broken'); } };
    for (const packet of packets) { packet.ip.frag.pcap = broken; }

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
  const ether = Buffer.from('020000000002020000000001', 'hex');

  // Writes frame to a pcap file, scrubs it with 0xee and returns the frame as it is in the file after
  async function scrub (frame, check) {
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
      check?.(obj);
      pcap.scrubPacket(obj, 24, Buffer.alloc(65536, 0xee), false);
      fs.closeSync(pcap.fd);

      const after = fs.readFileSync(file);
      assert.deepEqual(after.subarray(0, 40), Buffer.concat([ghdr, rhdr]), 'the file and record headers are kept');
      return after.subarray(40);
    } finally {
      fs.rmSync(path.dirname(file), { recursive: true });
    }
  }

  // The first headersLen bytes of frame are kept and everything after is scrubbed
  async function assertKeeps (frame, headersLen, msg, check) {
    assert.ok(frame.length > headersLen, msg);
    const after = await scrub(frame, check);
    assert.equal(after.length, frame.length, msg);
    assert.deepEqual(after.subarray(0, headersLen), frame.subarray(0, headersLen), msg + ' headers kept');
    assert.ok(after.subarray(headersLen).every(b => b === 0xee), msg + ' rest scrubbed');
  }

  test('the first fragment of a fragmented tunnel keeps the inner headers', async () => {
    // ether, ip4 gre fragment, gre, inner ip4, udp, then data that should be scrubbed
    const data = Buffer.alloc(40, 0x41);
    const inner = ip4Packet(1, 0, false, udpDatagram(data));
    const outer = ip4Packet(77, 0, true, Buffer.concat([Buffer.from('00000800', 'hex'), inner]), 47);
    const frame = Buffer.concat([ether, Buffer.from('0800', 'hex'), outer]);
    await assertKeeps(frame, 14 + 20 + 4 + 20 + 8, 'tunnel fragment', (obj) => assert.ok(obj.ip.frag, 'decodes as a fragment'));
  });

  test('icmp6 and esp keep their first 8 bytes', async () => {
    // The ip6 udp packet with its next header changed, 8 bytes of icmp6 header then 4 of data
    const icmp6 = ip6Packet([]);
    icmp6[6] = 58;
    await assertKeeps(Buffer.concat([ether, Buffer.from('86dd', 'hex'), icmp6]), 14 + 40 + 8, 'icmp6');

    // spi and sequence number, then what is encrypted
    const esp = ip4Packet(1, 0, false, Buffer.alloc(40, 0x41), 50);
    await assertKeeps(Buffer.concat([ether, Buffer.from('0800', 'hex'), esp]), 14 + 20 + 8, 'esp');
  });

  test('with no transport header to keep everything after the ip header is scrubbed', async () => {
    const data = Buffer.from('abcdefg');
    for (const proto of [1, 6, 17, 132, 99]) {
      // too short for any of the transport headers, or a protocol that isn't decoded
      const frame = Buffer.concat([ether, Buffer.from('0800', 'hex'), ip4Packet(1, 0, false, data.subarray(0, proto === 1 ? 3 : 7), proto)]);
      await assertKeeps(frame, 14 + 20, 'ip4 protocol ' + proto);
    }
    const ip6 = ip6Packet([]);
    ip6[6] = 99;
    await assertKeeps(Buffer.concat([ether, Buffer.from('86dd', 'hex'), ip6]), 14 + 40, 'ip6 protocol 99');
  });

  test('a packet with no ip layer is all scrubbed', async () => {
    const noIp = {
      arp: Buffer.concat([ether, Buffer.from('0806', 'hex'), Buffer.alloc(28, 0x41)]),
      'unknown ethertype': Buffer.concat([ether, Buffer.from('1234', 'hex'), Buffer.alloc(28, 0x41)]),
      'ip4 cut short': Buffer.concat([ether, Buffer.from('0800', 'hex'), ip4Packet(1, 0, false, Buffer.alloc(8)).subarray(0, 12)])
    };
    for (const [what, frame] of Object.entries(noIp)) {
      await assertKeeps(frame, 0, what, (obj) => assert.equal(obj.ip, undefined, what));
    }
  });
});

describe('Pcap decoding', () => {
  const v6src = Buffer.from('fd000000000000000000000000000001', 'hex');
  const v6dst = Buffer.from('fd000000000000000000000000000099', 'hex');

  // An ip6 header in front of payload, len is the payload length field
  function ip6Header (nxt, payload, len = payload.length) {
    const hdr = Buffer.alloc(40);
    hdr[0] = 0x60;
    hdr.writeUInt16BE(len, 4);
    hdr[6] = nxt;
    hdr[7] = 64;
    v6src.copy(hdr, 8);
    v6dst.copy(hdr, 24);
    return Buffer.concat([hdr, payload]);
  }

  function decode6 (buf) {
    const obj = {};
    new Pcap('unit').ip6(buf, obj, 0);
    return obj;
  }

  function decode4 (buf) {
    const obj = {};
    new Pcap('unit').ip4(buf, obj, 0);
    return obj;
  }

  const data = Buffer.from('payload bytes');
  const dgram = udpDatagram(data);

  test('ip6 payload ends where the payload length says, after extension headers too', () => {
    // 8 byte hop by hop header then udp, with junk after the packet like an ethernet trailer
    const hop = Buffer.alloc(8);
    hop[0] = 17;
    const packet = ip6Header(0, Buffer.concat([hop, dgram]));
    const obj = decode6(Buffer.concat([packet, Buffer.from('trailing junk!!')]));
    assert.deepEqual(obj.udp?.data, data);
  });

  test('udp data ends where the udp length says', () => {
    const padded = Buffer.concat([dgram, Buffer.alloc(6, 0xee)]);
    const obj = decode4(ip4Packet(1, 0, false, padded));
    assert.deepEqual(obj.udp?.data, data);
    // A udp length bigger than the packet is ignored, like capture
    const bad = Buffer.from(dgram);
    bad.writeUInt16BE(1000, 4);
    assert.equal(decode4(ip4Packet(1, 0, false, bad)).udp?.data.length, data.length);
  });

  test('ip4 in ip4 and ip6 in ip6 decode to the inner packet', () => {
    const inner4 = ip4Packet(2, 0, false, dgram);
    const outer4 = decode4(ip4Packet(1, 0, false, inner4, 4));
    assert.deepEqual(outer4.udp?.data, data);

    const inner6 = ip6Header(17, dgram);
    const outer6 = decode6(ip6Header(41, inner6));
    assert.deepEqual(outer6.udp?.data, data);
  });

  test('link type 229 is raw ip6', () => {
    const pkt = ip6Header(17, dgram);
    const rec = Buffer.alloc(16);
    rec.writeUInt32LE(pkt.length, 8);
    rec.writeUInt32LE(pkt.length, 12);
    const pcap = new Pcap('unit');
    pcap.linkType = 229;
    const obj = {};
    pcap.pcap(Buffer.concat([rec, pkt]), obj);
    assert.deepEqual(obj.udp?.data, data);
  });

  test('a transport header that is cut short is ip data, not a throw', () => {
    for (const [proto, layer] of [[1, 'icmp'], [6, 'tcp'], [17, 'udp'], [132, 'sctp']]) {
      const short = Buffer.from('abc');
      const obj = decode4(ip4Packet(1, 0, false, short, proto));
      assert.equal(obj[layer], undefined, layer);
      assert.deepEqual(obj.ip.data, short, layer);
      // The things that look at decoded packets cope with no transport layer
      assert.equal(Pcap.key(obj), '10.0.0.1', layer);
      const withTs = { ...obj, pcap: { ts_sec: 1, ts_usec: 0, incl_len: 0 } };
      const { results } = Pcap.packetFlow({ ipProtocol: proto, source: { ip: '10.0.0.1', port: 1000 }, destination: { ip: '10.0.0.2', port: 53 } }, [withTs], 10);
      assert.deepEqual(results[0].data, short, layer);
    }
  });

  // An ip4 packet carrying a gre payload of type
  function gre (type, payload) {
    const hdr = Buffer.alloc(4);
    hdr.writeUInt16BE(type, 2);
    return decode4(ip4Packet(1, 0, false, Buffer.concat([hdr, payload]), 47));
  }

  test('ppp in gre with and without address, control and full protocol fields', () => {
    const ip = ip4Packet(2, 0, false, dgram);
    for (const hdr of ['ff030021', 'ff0321', '0021', '21']) {
      const obj = gre(0x880b, Buffer.concat([Buffer.from(hdr, 'hex'), ip]));
      assert.equal(obj.ppp?.type, 0x21, hdr);
      assert.deepEqual(obj.udp?.data, data, hdr);
    }
    const obj6 = gre(0x880b, Buffer.concat([Buffer.from('ff030057', 'hex'), ip6Header(17, dgram)]));
    assert.deepEqual(obj6.udp?.data, data);
    // Too short for a protocol
    assert.equal(gre(0x880b, Buffer.from('ff03', 'hex')).udp, undefined);
  });

  test('pppoe session packets are checked like capture', () => {
    const ip = ip4Packet(2, 0, false, dgram);
    const pppoe = (verType, len) => {
      const hdr = Buffer.from('1100000100000021', 'hex');
      hdr[0] = verType;
      hdr.writeUInt16BE(len, 4);
      return gre(0x8864, Buffer.concat([hdr, ip]));
    };
    assert.deepEqual(pppoe(0x11, ip.length + 2).udp?.data, data);
    assert.equal(pppoe(0x12, ip.length + 2).udp, undefined);
    assert.equal(pppoe(0x11, ip.length + 100).udp, undefined);
    assert.equal(pppoe(0x11, 1).udp, undefined);
    assert.equal(gre(0x8864, Buffer.from('1100', 'hex')).udp, undefined);
  });

  test('nsh in vxlan-gpe and gre', () => {
    const ip = ip4Packet(2, 0, false, dgram);
    // base header with a length of 2 words, then 4 more bytes of metadata
    const nsh = (next, len = 3) => Buffer.from([0x00, len, 0x02, next, 0, 0, 0, 0xff, 1, 2, 3, 4]).subarray(0, len * 4);
    const gpe = Buffer.from('0c00000400000100', 'hex'); // next protocol 4 is nsh
    const udp = Buffer.concat([Buffer.from('03e812b600000000', 'hex'), gpe, nsh(1), ip]);
    udp.writeUInt16BE(udp.length, 4);
    assert.deepEqual(decode4(ip4Packet(1, 0, false, udp)).udp?.data, data);

    assert.deepEqual(gre(0x894f, Buffer.concat([nsh(1, 2), ip])).udp?.data, data);
    assert.deepEqual(gre(0x894f, Buffer.concat([nsh(2, 2), ip6Header(17, dgram)])).udp?.data, data);
    // nsh in nsh
    assert.deepEqual(gre(0x894f, Buffer.concat([nsh(4, 2), nsh(1), ip])).udp?.data, data);
    // a length less than the base header, or more than there is
    assert.equal(gre(0x894f, Buffer.concat([nsh(1, 1), ip])).udp, undefined);
    assert.equal(gre(0x894f, Buffer.from([0, 0x3f, 2, 1])).udp, undefined);
  });

  test('unknown types are logged at most once a minute', () => {
    const logged = [];
    const log = console.log;
    const now = Date.now;
    let clock = now();
    console.log = (...args) => logged.push(args.join(' '));
    Date.now = () => clock;
    const decodeAll = () => {
      for (let i = 0; i < 5; i++) {
        gre(0x1234, Buffer.from('abcd', 'hex'));
        gre(0x1235, Buffer.from('abcd', 'hex'));
        gre(0x880b, Buffer.from('c021abcd', 'hex'));
      }
    };
    try {
      decodeAll();
      clock += 30000;
      decodeAll();
      assert.equal(logged.filter(l => l.includes('gre.type 4660')).length, 1);
      assert.equal(logged.filter(l => l.includes('gre.type 4661')).length, 1);
      assert.equal(logged.filter(l => l.includes('ppp.type 49185')).length, 1);
      // A minute later it is logged again, with how many weren't
      clock += 30000;
      decodeAll();
      const again = logged.filter(l => l.includes('gre.type 4660'));
      assert.equal(again.length, 2);
      assert.match(again[1], /\(9 more since last logged\)/);
    } finally {
      console.log = log;
      Date.now = now;
    }
    // what couldn't be decoded is still the packet's data
    assert.deepEqual(gre(0x1234, Buffer.from('abcd', 'hex')).ip.data, Buffer.from('abcd', 'hex'));
  });

  test('unknown types are still logged after a lot of different ones', () => {
    // Only so many types are remembered, the ones not logged for a minute make room
    const logged = [];
    const log = console.log;
    const now = Date.now;
    let clock = now() + 600000;
    console.log = (...args) => logged.push(args.join(' '));
    Date.now = () => clock;
    try {
      for (let type = 0x9000; type < 0x9000 + 1100; type++) {
        gre(type, Buffer.from('abcd', 'hex'));
      }
      assert.ok(logged.length <= 1000, `logged ${logged.length}`);
      gre(0xa000, Buffer.from('abcd', 'hex'));
      assert.equal(logged.filter(l => l.includes('gre.type 40960')).length, 0);
      clock += 60000;
      gre(0xa000, Buffer.from('abcd', 'hex'));
      assert.equal(logged.filter(l => l.includes('gre.type 40960')).length, 1);
    } finally {
      console.log = log;
      Date.now = now;
    }
  });

  test('decoding mutated packets never hangs or throws', () => {
    // Every link type, every decoder, mutated a lot. The callers catch a throw, but a decoder
    // should cope with any bytes, and a hang can't be caught, so this runs in a child with a timeout.
    const seeds = [
      ip4Packet(1, 0, false, dgram),
      ip4Packet(1, 0, false, Buffer.concat([Buffer.from('03e80050000000010000000250180400' + '00000000', 'hex'), data]), 6),
      ip4Packet(1, 0, false, Buffer.concat([Buffer.from('00000800', 'hex'), ip4Packet(2, 0, false, dgram)]), 47),
      ip4Packet(1, 0, false, ip4Packet(2, 0, false, dgram), 4),
      ip6Header(41, ip6Header(17, dgram)),
      ip6Header(0, Buffer.concat([Buffer.from('3c00000000000000', 'hex'), Buffer.from('1100000000000000', 'hex'), dgram])),
      ip6Header(44, Buffer.concat([Buffer.from('1100000100000001', 'hex'), dgram])),
      ip4Packet(1, 0, false, Buffer.concat([Buffer.from('0000880bff0321', 'hex'), ip4Packet(2, 0, false, dgram)]), 47),
      ip4Packet(1, 0, false, Buffer.concat([Buffer.from('000088641100000100000021', 'hex'), ip4Packet(2, 0, false, dgram)]), 47),
      ip4Packet(1, 0, false, Buffer.concat([Buffer.from('0000894f000302010000000000000000', 'hex'), ip4Packet(2, 0, false, dgram)]), 47),
      ip4Packet(1, 0, false, Buffer.concat([Buffer.from('000088470001410000000000', 'hex'), ip4Packet(2, 0, false, dgram)]), 47)
    ].map(b => b.toString('hex'));
    const script = `
      const Pcap = require(${JSON.stringify(pcapPath)});
      // Only a hang should fail this, not how much the decoders log
      console.log = console.trace = () => {};
      let seed = 1;
      const rand = (n) => { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed % n; };
      const seeds = ${JSON.stringify(seeds)}.map(h => Buffer.from(h, 'hex'));
      const ether = Buffer.from('020000000002020000000001', 'hex');
      const links = [[1, (ip) => Buffer.concat([ether, Buffer.from([0x08, 0x00]), ip])],
                     [1, (ip) => Buffer.concat([ether, Buffer.from('8100000586dd', 'hex'), ip])],
                     [101, (ip) => ip], [228, (ip) => ip], [229, (ip) => ip], [0, (ip) => Buffer.concat([Buffer.from([30, 0, 0, 0]), ip])],
                     [113, (ip) => Buffer.concat([Buffer.alloc(14), Buffer.from([0x08, 0x00]), ip])],
                     [276, (ip) => Buffer.concat([Buffer.from([0x86, 0xdd]), Buffer.alloc(18), ip])],
                     [239, (ip) => Buffer.concat([Buffer.from([2, 0, 0, 0]), Buffer.from([ip.length + 4, 0, 9, 0]), ip])],
                     [107, (ip) => Buffer.concat([Buffer.from([0, 0, 0x03, 0xcc]), ip])],
                     [127, (ip) => Buffer.concat([Buffer.from([0, 0, 8, 0]), Buffer.alloc(4), Buffer.from([8]), Buffer.alloc(23), Buffer.from('aaaa03000000', 'hex'), Buffer.from([0x08, 0x00]), ip])]];
      let throws = 0;
      for (let i = 0; i < 40000; i++) {
        const [linkType, wrap] = links[rand(links.length)];
        const pkt = Buffer.from(wrap(seeds[rand(seeds.length)]));
        for (let m = rand(8); m >= 0; m--) { pkt[rand(pkt.length)] = rand(256); }
        const cut = pkt.subarray(0, rand(pkt.length + 1));
        const rec = Buffer.alloc(16);
        rec.writeUInt32LE(cut.length, 8);
        rec.writeUInt32LE(cut.length, 12);
        const pcap = new Pcap('fuzz');
        pcap.linkType = linkType;
        try { pcap.pcap(Buffer.concat([rec, cut]), {}); } catch (e) { throws++; }
      }
      process.stdout.write('done ' + throws + '\\n');
    `;
    const result = spawnSync(process.execPath, ['-e', script], { timeout: 60000, encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });
    assert.equal(result.signal, null, 'decoding a mutated packet hung');
    assert.match(result.stdout, /^done 0$/m, result.stdout + result.stderr.slice(0, 2000));
  });
});
