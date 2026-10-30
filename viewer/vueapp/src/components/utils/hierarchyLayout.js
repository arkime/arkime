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
