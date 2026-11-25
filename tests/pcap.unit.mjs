/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for viewer/pcap.js decoding. Run with `npm run test:unit`.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
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
