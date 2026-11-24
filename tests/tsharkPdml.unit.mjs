/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for building the session tshark api's proto/field tree from
tshark's PDML (viewer/tsharkPdml.js). Run with `npm run test:unit`.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const cjsRequire = createRequire(import.meta.url);
const sax = cjsRequire('sax');
const { pdmlTreeBuilder, MAX_TREE_DEPTH } = cjsRequire('../viewer/tsharkPdml.js');

// parse a PDML document the way the tshark api does, returning its packets
const parse = (xml, includeHidden = false) => {
  const packets = [];
  const tree = pdmlTreeBuilder(includeHidden, (p) => packets.push(p));
  const parser = sax.parser(true, { trim: false, normalize: false, lowercase: true });
  parser.onopentag = tree.opentag;
  parser.onclosetag = tree.closetag;
  parser.write(xml).close();
  return packets;
};

const field = (fname, inner = '', attrs = '') => `<field name="${fname}" showname="${fname}"${attrs}>${inner}</field>`;
const pdml = (protoInner) => `<pdml><packet><proto name="xml" showname="XML" pos="0" size="10">${protoInner}</proto></packet></pdml>`;

// levels below a node, protos are level 1
const depth = (n) => 1 + Math.max(0, ...(n.fields || []).map(depth));
const names = (n) => [n.name, ...(n.fields || []).flatMap(names)];

describe('pdmlTreeBuilder', () => {
  test('builds the proto/field tree', () => {
    const [p] = parse(pdml(field('a', field('b', '', ' show="1" value="01" pos="2" size="1"')) + field('c')));
    assert.deepEqual(p, {
      layers: [{
        name: 'xml',
        label: 'XML',
        pos: 0,
        size: 10,
        fields: [
          { name: 'a', label: 'a', fields: [{ name: 'b', label: 'b', show: '1', value: '01', pos: 2, size: 1 }] },
          { name: 'c', label: 'c' }
        ]
      }]
    });
  });

  test('hidden fields and their children are dropped unless asked for', () => {
    const xml = pdml(field('h', field('hc'), ' hide="yes"') + field('v'));
    assert.deepEqual(names(parse(xml)[0].layers[0]), ['xml', 'v']);
    assert.deepEqual(names(parse(xml, true)[0].layers[0]), ['xml', 'h', 'hc', 'v']);
  });

  test('deep nesting is capped and no field is lost', () => {
    for (const n of [MAX_TREE_DEPTH - 2, MAX_TREE_DEPTH - 1, MAX_TREE_DEPTH, 600]) {
      let inner = '';
      for (let i = n - 1; i >= 0; i--) { inner = field(`f${i}`, inner); }
      const layer = parse(pdml(inner + field('after')))[0].layers[0];
      assert.equal(depth(layer), Math.min(n + 1, MAX_TREE_DEPTH), `n=${n}`);
      assert.deepEqual(names(layer), ['xml', ...Array.from({ length: n }, (_, i) => `f${i}`), 'after'], `n=${n}`);
    }
  });

  test('a hidden field past the cap still hides its children', () => {
    let inner = field('deepHidden', field('secret'), ' hide="yes"') + field('shown');
    for (let i = MAX_TREE_DEPTH + 5; i >= 0; i--) { inner = field(`f${i}`, inner); }
    const all = names(parse(pdml(inner))[0].layers[0]);
    assert.ok(all.includes('shown'));
    assert.ok(!all.includes('deepHidden') && !all.includes('secret'));
  });

  test('a proto nested deep in fields starts back at level 1', () => {
    let inner = `<proto name="inner">${field('g0', field('g1', field('g2', field('g3', field('g4')))))}</proto>`;
    for (let i = MAX_TREE_DEPTH - 3; i >= 0; i--) { inner = field(`f${i}`, inner); }
    const [outer, nested] = parse(pdml(inner))[0].layers;
    assert.equal(nested.name, 'inner');
    assert.equal(depth(nested), 6);
    assert.deepEqual(names(nested), ['inner', 'g0', 'g1', 'g2', 'g3', 'g4']);
    assert.equal(depth(outer), MAX_TREE_DEPTH - 1);
  });

  test('one packet at a time', () => {
    const xml = `<pdml><packet><proto name="a"></proto></packet><packet><proto name="b"></proto></packet></pdml>`;
    assert.deepEqual(parse(xml).map(p => p.layers[0].name), ['a', 'b']);
  });
});
