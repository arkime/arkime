/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Layout helpers for the waffle chart (summary/SummaryWaffle.vue), drawn by the
dashboard widget and the SPI Graph waffle: a 10x10 grid of squares, each 1% of
the whole, given out to the values by their share, and a split waffle's small
multiples. Pure JS: no d3, no DOM.
*/

import { smallMultiplesGrid, nodeCount } from './hierarchyLayout.js';

/**
 * Gives `cells` squares to the values in proportion to their share (largest
 * remainder: each gets the whole squares of its share, then the ones left go
 * to the biggest remainders, earlier values first on a tie), so the counts
 * always add up to `cells`. Values of 0 or less get none.
 * @param {number[]} values - each value's size
 * @returns {number[]} the squares each value gets
 */
export function apportionCells (values, cells = 100) {
  const total = values.reduce((sum, v) => sum + (v > 0 ? v : 0), 0);
  if (total <= 0) { return values.map(() => 0); }
  const exact = values.map(v => (v > 0 ? v * cells / total : 0));
  const counts = exact.map(Math.floor);
  let left = cells - counts.reduce((a, b) => a + b, 0);
  const order = exact.map((e, i) => ({ i, r: e - counts[i] }))
    .filter(o => o.r > 0)
    .sort((a, b) => b.r - a.r || a.i - b.i);
  for (const o of order) {
    if (left <= 0) { break; }
    counts[o.i]++;
    left--;
  }
  return counts;
}

/**
 * The squares of a side x side grid in a w x h box (top-left origin),
 * centered, filled row by row from the top left by the values in order.
 * @param {number[]} counts - each value's squares (apportionCells)
 * @returns {object|null} { size (a square's side), cells: [{ x, y, index (the
 *   value's) }] }, null when the box is too small for one pixel squares
 */
export function waffleGrid (counts, w, h, { side = 10, gap = 2 } = {}) {
  const grid = Math.min(w, h);
  const size = (grid - gap * (side - 1)) / side;
  if (size < 1) { return null; }
  const x0 = (w - grid) / 2;
  const y0 = (h - grid) / 2;
  const cells = [];
  counts.forEach((n, index) => {
    for (let k = 0; k < n && cells.length < side * side; k++) {
      const pos = cells.length;
      cells.push({ x: x0 + (pos % side) * (size + gap), y: y0 + Math.floor(pos / side) * (size + gap), index });
    }
  });
  return { size, cells };
}

/**
 * Lays out n waffles (a split waffle) in a w x h box (top-left origin): the
 * grid of cells with the largest squares, each cell keeping `titleH` px above
 * its waffle for the split value's name.
 * @returns {object[]} per waffle { x, y (its grid's top left), side (its
 *   grid's size), titleX, titleY, cellW }
 */
export function splitWaffleLayout (n, w, h, titleH) {
  if (n <= 0 || w <= 0 || h <= 0) { return []; }
  const { cols, radius } = smallMultiplesGrid(n, w, h, titleH);
  const side = Math.max(0, 2 * radius);
  const cellW = w / cols;
  const cellH = h / Math.ceil(n / cols);
  return Array.from({ length: n }, (_, i) => {
    const cx = (i % cols) * cellW;
    const cy = Math.floor(i / cols) * cellH;
    const y = cy + titleH + (cellH - titleH - side) / 2;
    return { x: cx + (cellW - side) / 2, y, side, titleX: cx + cellW / 2, titleY: y - 4, cellW };
  });
}

/**
 * A split waffle's groups from the hierarchy endpoint's two level tree (split
 * field, then the drawn field): one per split value, its "Other" left out,
 * each with its own top values and its "Other" (when fetched).
 * @returns {object[]} [{ name, items: [{ item, sessions, value }], other: { sessions, value } | null }]
 */
export function waffleGroups (hierarchy) {
  return (hierarchy?.children || []).filter(g => !g.other).map((g) => {
    const children = g.children || [];
    const other = children.find(c => c.other);
    return {
      name: g.name,
      items: children.filter(c => !c.other).map(c => ({ item: c.name, sessions: nodeCount(c), value: nodeCount(c) })),
      other: other ? { sessions: nodeCount(other), value: nodeCount(other) } : null
    };
  });
}
