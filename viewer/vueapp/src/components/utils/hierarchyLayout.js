/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Layout helpers shared by the SPI Graph hierarchy (spigraph/Hierarchy.vue) and
the dashboard hierarchy widget (summary/widgets/HierarchyWidget.vue), so a
split pie lays out and labels the same on both. Pure JS: no d3, no DOM.
*/

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
