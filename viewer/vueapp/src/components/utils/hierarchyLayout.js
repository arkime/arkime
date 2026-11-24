/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Layout helpers shared by the SPI Graph hierarchy (spigraph/Hierarchy.vue) and
the dashboard hierarchy widget (summary/widgets/HierarchyWidget.vue), so a
split pie lays out and labels the same on both. Pure JS: no d3, no DOM.
*/

/**
 * A hierarchy endpoint node's session count: a value with children has it in
 * sizeValue (its size is cleared, d3 sums the children), a leaf in size.
 */
export function nodeCount (node) {
  return node?.sizeValue ?? node?.size ?? 0;
}

/**
 * A share (0-1) as a percent label: whole percents, one decimal under 10%
 * (the mosaic's cells and the waffle's legend).
 */
export function sharePercent (share) {
  return `${(share * 100).toFixed(share < 0.1 ? 1 : 0)}%`;
}

/**
 * True when a d3 hierarchy node is an "Other" (the sessions outside a level's
 * top values) or sits under one: no value of the field, so it is drawn gray
 * rather than in a palette color.
 */
export function isUnderOther (d) {
  for (let n = d; n; n = n.parent) {
    if (n.data.other) { return true; }
  }
  return false;
}

/**
 * The columns that give n equal circles in a w×h box the largest radius, and
 * that radius (each cell keeps `titleH` px above its circle for the name).
 */
export function smallMultiplesGrid (n, w, h, titleH) {
  let best = { cols: 1, radius: 0 };
  for (let cols = 1; cols <= n; cols++) {
    const rows = Math.ceil(n / cols);
    const radius = Math.min(w / cols, h / rows - titleH) / 2;
    if (radius > best.radius) { best = { cols, radius }; }
  }
  return best;
}

/**
 * Lays out a split pie in a w×h box (top-left origin): one cell per child of
 * the d3 hierarchy root except its own "Other", in the grid with the largest
 * circles. Returns null when there is no pie to draw, else { cells: [{ pie,
 * cx, cy }] (each pie's center), radius, cellW, cellH, sliceCount (distinct
 * non-Other slice values across the pies, for the palette) }.
 */
export function splitPieLayout (root, w, h, titleH) {
  const pies = (root.children || []).filter(p => !p.data.other);
  if (!pies.length) { return null; }
  const sliceCount = new Set(pies.flatMap(p => (p.children || []).filter(c => !c.data.other).map(c => c.data.name))).size;
  const { cols, radius } = smallMultiplesGrid(pies.length, w, h, titleH);
  const cellW = w / cols;
  const cellH = h / Math.ceil(pies.length / cols);
  const cells = pies.map((pie, i) => ({
    pie,
    cx: (i % cols) * cellW + cellW / 2,
    cy: Math.floor(i / cols) * cellH + titleH + (cellH - titleH) / 2
  }));
  return { cells, radius, cellW, cellH, sliceCount };
}

/**
 * A label cut to what fits in maxPx at about pxPerChar per character: the
 * whole label when it fits, an ellipsized start otherwise, and nothing when
 * not even a few characters fit.
 */
export function fitLabel (label, maxPx, pxPerChar) {
  const text = String(label ?? '');
  const chars = Math.floor(maxPx / pxPerChar);
  if (text.length <= chars) { return text; }
  return chars >= 4 ? `${text.slice(0, chars - 1)}…` : '';
}

/**
 * Lays out a mosaic in a w×h box (top-left origin) from the hierarchy
 * endpoint's two level tree: one column per first-field value, as wide as its
 * share of the sessions, split top to bottom into its second-field values,
 * each as tall as its share of the column. The second field's maxSegments
 * values with the most sessions across the columns keep their own cells, in
 * the same order in every column; the rest of a column (its "Other" with
 * them) is one "Other" cell at its bottom. A value without children (a single
 * field, or the first level's "Other") is a column of one cell. Returns null
 * when there is nothing to draw, else { columns: [{ name, other, x, w, value,
 * share }], cells: [{ col, name, other, x, y, w, h, value, share (of its
 * column), area (of the whole) }], segments: the kept second-field values }.
 */
export function mosaicLayout (data, w, h, { gap = 2, maxSegments = 10 } = {}) {
  const cols = (data?.children || []).filter(c => nodeCount(c) > 0);
  const total = cols.reduce((sum, c) => sum + nodeCount(c), 0);
  if (!cols.length || total <= 0 || w <= 0 || h <= 0) { return null; }

  const totals = new Map();
  for (const c of cols) {
    for (const s of (c.children || [])) {
      if (!s.other) { totals.set(s.name, (totals.get(s.name) || 0) + nodeCount(s)); }
    }
  }
  const segments = [...totals.entries()].sort((a, b) => b[1] - a[1]).slice(0, maxSegments).map(([seg]) => seg);
  const rank = new Map(segments.map((seg, i) => [seg, i]));

  const usableW = Math.max(0, w - gap * (cols.length - 1));
  const columns = [];
  const cells = [];
  let x = 0;
  cols.forEach((c, ci) => {
    const value = nodeCount(c);
    const cw = usableW * value / total;
    columns.push({ name: c.name, other: !!c.other, x, w: cw, value, share: value / total });
    let parts;
    if (c.children?.length) {
      const kept = c.children.filter(s => !s.other && rank.has(s.name) && nodeCount(s) > 0)
        .sort((a, b) => rank.get(a.name) - rank.get(b.name))
        .map(s => ({ name: s.name, other: false, value: nodeCount(s) }));
      const rest = c.children.filter(s => s.other || !rank.has(s.name)).reduce((sum, s) => sum + nodeCount(s), 0);
      parts = rest > 0 ? [...kept, { name: 'Other', other: true, value: rest }] : kept;
    } else {
      parts = [{ name: c.name, other: !!c.other, value }];
    }
    const colSum = parts.reduce((sum, p) => sum + p.value, 0);
    const usableH = Math.max(0, h - gap * (parts.length - 1));
    let y = 0;
    for (const p of parts) {
      const ch = colSum > 0 ? usableH * p.value / colSum : 0;
      const share = colSum > 0 ? p.value / colSum : 0;
      cells.push({ col: ci, name: p.name, other: p.other, x, y, w: cw, h: ch, value: p.value, share, area: share * value / total });
      y += ch + gap;
    }
    x += cw + gap;
  });
  return { columns, cells, segments };
}
