/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for the waffle chart layout helpers shared by SPI Graph and the
dashboard waffle widget (viewer/vueapp/src/components/utils/waffleLayout.js).
Run with `npm run test:unit`.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';

const { apportionCells, waffleGrid, splitWaffleLayout, waffleGroups } =
  await import('../viewer/vueapp/src/components/utils/waffleLayout.js');

describe('apportionCells', () => {
  test('always 100 squares, the leftovers to the biggest remainders', () => {
    // 33.3 each: one left over, the first value gets it
    assert.deepEqual(apportionCells([1, 1, 1]), [34, 33, 33]);
    // 62.5 / 25 / 12.5: the remainders tie, earlier first
    assert.deepEqual(apportionCells([5, 2, 1]), [63, 25, 12]);
    assert.deepEqual(apportionCells([999, 1]), [100, 0]);
    assert.equal(apportionCells([7, 3, 11, 13, 2]).reduce((a, b) => a + b, 0), 100);
  });

  test('empty, zero and negative values get none', () => {
    assert.deepEqual(apportionCells([]), []);
    assert.deepEqual(apportionCells([0, 0]), [0, 0]);
    assert.deepEqual(apportionCells([3, 0, -2, 1]), [75, 0, 0, 25]);
  });

  test('another number of squares', () => {
    assert.deepEqual(apportionCells([1, 1], 9), [5, 4]);
  });
});

describe('waffleGrid', () => {
  test('a centered 10x10 grid filled row by row', () => {
    const g = waffleGrid([12, 88], 200, 118, { gap: 2 });
    // the grid is the box's short side: (118 - 9 * 2) / 10
    assert.equal(g.size, 10);
    assert.equal(g.cells.length, 100);
    // centered across: (200 - 118) / 2
    assert.deepEqual(g.cells[0], { x: 41, y: 0, index: 0 });
    // the 12th square starts the second row's 2nd column, the 13th is the next value
    assert.deepEqual(g.cells[11], { x: 53, y: 12, index: 0 });
    assert.equal(g.cells[12].index, 1);
    assert.deepEqual(g.cells[99], { x: 149, y: 108, index: 1 });
  });

  test('too small to draw', () => {
    assert.equal(waffleGrid([100], 25, 25, { gap: 2 }), null);
  });
});

describe('splitWaffleLayout', () => {
  test('the grid with the largest waffles, each under its title', () => {
    // 4 in 300x300 with 20px titles: 2x2 gives min(150, 130) = 130px waffles
    // (4 across only 75px, 3 across 100px)
    const boxes = splitWaffleLayout(4, 300, 300, 20);
    assert.equal(boxes.length, 4);
    assert.deepEqual(boxes.map(b => [b.x, b.y, b.side]), [[10, 20, 130], [160, 20, 130], [10, 170, 130], [160, 170, 130]]);
    assert.deepEqual([boxes[1].titleX, boxes[1].titleY, boxes[1].cellW], [225, 16, 150]);
    // in a wide strip they go in one row
    assert.equal(new Set(splitWaffleLayout(3, 900, 200, 20).map(b => b.y)).size, 1);
  });

  test('nothing to lay out', () => {
    assert.deepEqual(splitWaffleLayout(0, 400, 200, 20), []);
    assert.deepEqual(splitWaffleLayout(2, 0, 200, 20), []);
  });
});

describe('waffleGroups', () => {
  test('one group per split value, its Other kept, the split Other left out', () => {
    const groups = waffleGroups({
      children: [
        { name: 'node1', sizeValue: 10, children: [{ name: 'tcp', size: 6 }, { name: 'udp', size: 3 }, { name: 'Other', other: true, size: 1 }] },
        { name: 'node2', sizeValue: 4, children: [{ name: 'udp', size: 4 }] },
        { name: 'Other', other: true, size: 7 }
      ]
    });
    assert.deepEqual(groups, [
      { name: 'node1', items: [{ item: 'tcp', sessions: 6, value: 6 }, { item: 'udp', sessions: 3, value: 3 }], other: { sessions: 1, value: 1 } },
      { name: 'node2', items: [{ item: 'udp', sessions: 4, value: 4 }], other: null }
    ]);
    assert.deepEqual(waffleGroups(null), []);
  });
});
