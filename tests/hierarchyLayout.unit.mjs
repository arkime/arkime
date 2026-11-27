/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for the split pie layout helpers shared by SPI Graph and the
dashboard hierarchy widget (viewer/vueapp/src/components/utils/hierarchyLayout.js).
Run with `npm run test:unit`.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';

const { isUnderOther, smallMultiplesGrid, splitPieLayout, fitLabel, mosaicLayout, icicleLayout, iciclePathKey, nodeCount, sharePercent } =
  await import('../viewer/vueapp/src/components/utils/hierarchyLayout.js');

describe('nodeCount', () => {
  test('a parent\'s count is its sizeValue, a leaf\'s its size', () => {
    assert.equal(nodeCount({ name: 'a', sizeValue: 9, children: [] }), 9);
    assert.equal(nodeCount({ name: 'b', size: 4 }), 4);
    assert.equal(nodeCount({ name: 'c' }), 0);
    assert.equal(nodeCount(null), 0);
  });
});

describe('sharePercent', () => {
  test('whole percents, one decimal under 10%', () => {
    assert.equal(sharePercent(0.5), '50%');
    assert.equal(sharePercent(0.125), '13%');
    assert.equal(sharePercent(0.0625), '6.3%');
    assert.equal(sharePercent(0), '0.0%');
  });
});

describe('isUnderOther', () => {
  test('an Other node, and anything under it', () => {
    const root = { data: { name: 'root' }, parent: null };
    const other = { data: { name: 'Other', other: true }, parent: root };
    const underOther = { data: { name: '80' }, parent: other };
    const value = { data: { name: '10.0.0.1' }, parent: root };
    assert.equal(isUnderOther(other), true);
    assert.equal(isUnderOther(underOther), true);
    assert.equal(isUnderOther(value), false);
    assert.equal(isUnderOther(root), false);
  });
});

describe('smallMultiplesGrid', () => {
  test('picks the column count with the largest circles', () => {
    assert.deepEqual(smallMultiplesGrid(1, 400, 300, 20), { cols: 1, radius: 140 });
    // 4 pies in a wide box: 4 across gives r=50, 2x2 gives min(200, 130)/2=65
    assert.deepEqual(smallMultiplesGrid(4, 400, 300, 20), { cols: 2, radius: 65 });
    // a very wide strip lays them all in one row
    assert.equal(smallMultiplesGrid(6, 1200, 100, 20).cols, 6);
  });
});

describe('splitPieLayout', () => {
  const node = (key, children, other) => ({ data: { name: key, ...(other ? { other: true } : {}) }, children });

  test('one cell per pie, Other left out, centers in a top-left box', () => {
    const root = node('root', [
      node('a', [node('80'), node('443'), node('Other', undefined, true)]),
      node('b', [node('443'), node('22')]),
      node('c', [node('80')]),
      node('d', [node('53')]),
      node('Other', undefined, true)
    ]);
    const layout = splitPieLayout(root, 400, 300, 20);
    // same grid as smallMultiplesGrid: 2x2 cells of 200x150
    assert.equal(layout.radius, 65);
    assert.equal(layout.cellW, 200);
    assert.equal(layout.cellH, 150);
    assert.deepEqual(layout.cells.map(c => [c.pie.data.name, c.cx, c.cy]),
      [['a', 100, 85], ['b', 300, 85], ['c', 100, 235], ['d', 300, 235]]);
    // 80, 443, 22, 53: Other takes no palette color
    assert.equal(layout.sliceCount, 4);
  });

  test('nothing to draw', () => {
    assert.equal(splitPieLayout(node('root', [node('Other', undefined, true)]), 400, 300, 20), null);
    assert.equal(splitPieLayout(node('root'), 400, 300, 20), null);
  });
});

describe('fitLabel', () => {
  test('keeps what fits, ellipsizes the rest, drops the hopeless', () => {
    assert.equal(fitLabel('short', 100, 6), 'short');
    assert.equal(fitLabel('www.example.com', 60, 6), 'www.examp…'); // 10 chars fit
    assert.equal(fitLabel('abcdefgh', 18, 6), ''); // 3 chars: not worth drawing
    assert.equal(fitLabel(8080, 100, 6), '8080');
    assert.equal(fitLabel(undefined, 100, 6), '');
  });
});

describe('mosaicLayout', () => {
  test('columns sized by sessions, cells by their share of the column', () => {
    const data = {
      children: [
        { name: 'US', sizeValue: 300, children: [{ name: 'tls', size: 200 }, { name: 'dns', size: 100 }] },
        { name: 'DE', sizeValue: 100, children: [{ name: 'dns', size: 50 }, { name: 'tls', size: 25 }, { name: 'Other', other: true, size: 25 }] }
      ]
    };
    const m = mosaicLayout(data, 402, 202, { gap: 2 });
    // 400px across the two columns, 3:1
    assert.deepEqual(m.columns.map(c => [c.name, c.x, c.w, c.share]), [['US', 0, 300, 0.75], ['DE', 302, 100, 0.25]]);
    // tls has the most sessions overall, so it is on top in every column
    assert.deepEqual(m.segments, ['tls', 'dns']);
    const de = m.cells.filter(c => c.col === 1);
    assert.deepEqual(de.map(c => [c.name, c.other]), [['tls', false], ['dns', false], ['Other', true]]);
    // 198px of cells (2 gaps) split 25:50:25
    assert.deepEqual(de.map(c => [c.y, c.h]), [[0, 49.5], [51.5, 99], [152.5, 49.5]]);
    assert.equal(de[1].share, 0.5);
    assert.equal(de[1].area, 0.125);
  });

  test('values past maxSegments join the column Other', () => {
    const data = {
      children: [
        { name: 'a', sizeValue: 10, children: [{ name: 'x', size: 6 }, { name: 'y', size: 3 }, { name: 'z', size: 1 }] }
      ]
    };
    const m = mosaicLayout(data, 100, 100, { gap: 0, maxSegments: 1 });
    assert.deepEqual(m.segments, ['x']);
    assert.deepEqual(m.cells.map(c => [c.name, c.value, c.other]), [['x', 6, false], ['Other', 4, true]]);
  });

  test('a value without children is one cell, and a first level Other is a column', () => {
    const data = { children: [{ name: 'tls', size: 30 }, { name: 'Other', other: true, size: 10 }] };
    const m = mosaicLayout(data, 40, 50, { gap: 0 });
    assert.deepEqual(m.segments, []);
    assert.deepEqual(m.cells.map(c => [c.col, c.name, c.other, c.w, c.h]), [[0, 'tls', false, 30, 50], [1, 'Other', true, 10, 50]]);
  });

  test('nothing to draw', () => {
    assert.equal(mosaicLayout(null, 100, 100), null);
    assert.equal(mosaicLayout({ children: [] }, 100, 100), null);
    assert.equal(mosaicLayout({ children: [{ name: 'a', size: 0 }] }, 100, 100), null);
    assert.equal(mosaicLayout({ children: [{ name: 'a', size: 5 }] }, 0, 100), null);
  });
});

describe('icicleLayout', () => {
  // protocols, then port: tcp has a top port and an "Other" for the rest
  const data = {
    name: 'Top Talkers',
    children: [
      { name: 'tcp', sizeValue: 60, children: [{ name: '80', size: 30 }, { name: '443', size: 20 }, { name: 'Other', other: true, size: 10 }] },
      { name: 'udp', sizeValue: 40, children: [{ name: '53', size: 10 }] }
    ]
  };
  const at = (layout, ...path) => layout.cells.find(c => c.key === iciclePathKey(path));

  test('a row per level, each value as wide as its share of its parent', () => {
    const l = icicleLayout(data, 100, 40);
    assert.equal(l.rows, 2);
    assert.equal(l.rowH, 20);
    assert.deepEqual(l.focus, []);
    const tcp = at(l, 'tcp');
    assert.deepEqual([tcp.x, tcp.y, tcp.w, tcp.h, tcp.depth, tcp.value, tcp.share], [0, 0, 60, 20, 1, 60, 0.6]);
    const udp = at(l, 'udp');
    assert.deepEqual([udp.x, udp.w], [60, 40]);
    const p80 = at(l, 'tcp', '80');
    assert.deepEqual([p80.x, p80.y, p80.w, p80.depth, p80.top], [0, 20, 30, 2, 'tcp']);
    // udp's one top port leaves the rest of it empty
    const p53 = at(l, 'udp', '53');
    assert.deepEqual([p53.x, p53.w, p53.top], [60, 10, 'udp']);
  });

  test('an "Other" has its own key and is marked under Other', () => {
    const l = icicleLayout(data, 100, 40);
    const other = l.cells.find(c => c.other);
    assert.equal(other.key, iciclePathKey(['tcp', '\u0000other']));
    assert.equal(other.underOther, true);
    assert.equal(at(l, 'tcp', '80').underOther, false);
  });

  test('children adding up to more than their parent are scaled to fit it', () => {
    const tags = { children: [{ name: 'a', sizeValue: 10, children: [{ name: 'x', size: 10 }, { name: 'y', size: 10 }] }] };
    const l = icicleLayout(tags, 100, 20);
    assert.equal(at(l, 'a').w, 100);
    assert.equal(at(l, 'a').value, 10);
    assert.deepEqual([at(l, 'a', 'x').w, at(l, 'a', 'y').x], [50, 50]);
  });

  test('a focus fills the width, its path as crumb rows above its children', () => {
    const l = icicleLayout(data, 100, 40, { focus: ['tcp'] });
    assert.deepEqual(l.focus, ['tcp']);
    assert.equal(l.rows, 2);
    const tcp = at(l, 'tcp');
    assert.deepEqual([tcp.x, tcp.w, tcp.crumb], [0, 100, true]);
    assert.equal(at(l, 'udp'), undefined);
    const p80 = at(l, 'tcp', '80');
    assert.deepEqual([p80.x, p80.y, p80.w, p80.crumb, p80.top], [0, 20, 50, false, 'tcp']);
  });

  test('a focus not in the tree zooms as deep as it matches', () => {
    assert.deepEqual(icicleLayout(data, 100, 40, { focus: ['nope'] }).focus, []);
    // a leaf has nothing below it to zoom into
    assert.deepEqual(icicleLayout(data, 100, 40, { focus: ['tcp', '80'] }).focus, ['tcp']);
  });

  test('cells too thin to see are left out', () => {
    const l = icicleLayout({ children: [{ name: 'big', size: 999 }, { name: 'tiny', size: 1 }] }, 100, 20, { minW: 0.5 });
    assert.equal(at(l, 'tiny'), undefined);
  });

  test('nothing to draw is null', () => {
    assert.equal(icicleLayout(null, 100, 100), null);
    assert.equal(icicleLayout({ children: [] }, 100, 100), null);
    assert.equal(icicleLayout({ children: [{ name: 'a', size: 0 }] }, 100, 100), null);
    assert.equal(icicleLayout(data, 0, 100), null);
  });
});
