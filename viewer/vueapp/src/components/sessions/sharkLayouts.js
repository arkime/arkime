/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0
*/

// Every shark layout is two nested splits, so it reduces to the pair of
// directions the outer and inner split run in. 'col' stacks, 'row' sits
// side by side. `reverse` puts the Details/Bytes split before the List
// instead of after it.
//
// `panes` draws the arrangement itself as [x, y, w, h] rects in a 24x24 box --
// mdi has nothing that depicts a three-pane split, and the closest candidates
// (view-split-*, view-quilt) show the wrong shape. First rect is the packet
// list and is drawn solid; the other two are dimmed.
export const SHARK_LAYOUTS = {
  wireshark: {
    dirs: ['col', 'row'],
    label: 'List on top, Details and Bytes below',
    panes: [[2, 2, 20, 8], [2, 12, 9, 10], [13, 12, 9, 10]]
  },
  stacked: {
    dirs: ['col', 'col'],
    label: 'List, Details, Bytes stacked',
    panes: [[2, 2, 20, 5.5], [2, 9.25, 20, 5.5], [2, 16.5, 20, 5.5]]
  },
  columns: {
    dirs: ['row', 'row'],
    label: 'List, Details, Bytes side by side',
    panes: [[2, 2, 5.5, 20], [9.25, 2, 5.5, 20], [16.5, 2, 5.5, 20]]
  },
  sidebar: {
    dirs: ['row', 'col'],
    label: 'List on the left, Details and Bytes right',
    panes: [[2, 2, 9, 20], [13, 2, 9, 9], [13, 13, 9, 9]]
  },
  listbottom: {
    dirs: ['col', 'row'],
    reverse: true,
    label: 'Details and Bytes on top, List below',
    panes: [[2, 14, 20, 8], [2, 2, 9, 10], [13, 2, 9, 10]]
  }
};

export const SHARK_DEFAULT_LAYOUT = 'wireshark';
export const SHARK_DEFAULT_SIZES = [0.45, 0.5];
export const SHARK_DEFAULT_HEIGHT = 560;

const LAYOUT_KEY = 'arkime-shark-layout';
const SIZES_KEY = 'arkime-shark-sizes';
const HEIGHT_KEY = 'arkime-shark-height';

const clamp = (n, lo, hi, dflt) => (Number.isFinite(n) ? Math.max(lo, Math.min(hi, n)) : dflt);

export function loadSharkLayout () {
  try {
    const v = localStorage?.[LAYOUT_KEY];
    return SHARK_LAYOUTS[v] ? v : SHARK_DEFAULT_LAYOUT;
  } catch (e) {
    return SHARK_DEFAULT_LAYOUT;
  }
}

export function saveSharkLayout (layout) {
  try { localStorage[LAYOUT_KEY] = layout; } catch (e) { /* private mode */ }
}

// Sizes are kept per layout -- a 45% top row makes no sense as a 45% left column.
export function loadSharkSizes (layout) {
  try {
    const all = JSON.parse(localStorage?.[SIZES_KEY] || '{}');
    const s = all[layout];
    if (!Array.isArray(s)) { return SHARK_DEFAULT_SIZES.slice(); }
    return [clamp(s[0], 0.1, 0.9, SHARK_DEFAULT_SIZES[0]), clamp(s[1], 0.1, 0.9, SHARK_DEFAULT_SIZES[1])];
  } catch (e) {
    return SHARK_DEFAULT_SIZES.slice();
  }
}

export function saveSharkSizes (layout, sizes) {
  try {
    const all = JSON.parse(localStorage[SIZES_KEY] || '{}');
    all[layout] = sizes;
    localStorage[SIZES_KEY] = JSON.stringify(all);
  } catch (e) { /* private mode */ }
}

export function loadSharkHeight () {
  try {
    return clamp(parseInt(localStorage?.[HEIGHT_KEY]), 240, 4000, SHARK_DEFAULT_HEIGHT);
  } catch (e) {
    return SHARK_DEFAULT_HEIGHT;
  }
}

export function saveSharkHeight (height) {
  try { localStorage[HEIGHT_KEY] = String(height); } catch (e) { /* private mode */ }
}
