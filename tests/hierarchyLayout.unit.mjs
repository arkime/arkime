/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for the split pie layout helpers shared by SPI Graph and the
dashboard hierarchy widget (viewer/vueapp/src/components/utils/hierarchyLayout.js).
Run with `npm run test:unit`.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';

const { isUnderOther, smallMultiplesGrid, fitLabel } =
  await import('../viewer/vueapp/src/components/utils/hierarchyLayout.js');

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

describe('fitLabel', () => {
  test('keeps what fits, ellipsizes the rest, drops the hopeless', () => {
    assert.equal(fitLabel('short', 100, 6), 'short');
    assert.equal(fitLabel('www.example.com', 60, 6), 'www.examp…'); // 10 chars fit
    assert.equal(fitLabel('abcdefgh', 18, 6), ''); // 3 chars: not worth drawing
    assert.equal(fitLabel(8080, 100, 6), '8080');
    assert.equal(fitLabel(undefined, 100, 6), '');
  });
});
