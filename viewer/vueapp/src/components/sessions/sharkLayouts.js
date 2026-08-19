/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0
*/

// Every shark layout is two nested splits, so it reduces to the pair of
// directions the outer and inner split run in. 'col' stacks, 'row' sits
// side by side. Pane order is always list, fields, bytes.
export const SHARK_LAYOUTS = {
  wireshark: {
    dirs: ['col', 'row'],
    label: 'List on top, fields and bytes below',
    icon: 'mdi-view-split-horizontal'
  },
  stacked: {
    dirs: ['col', 'col'],
    label: 'All three stacked',
    icon: 'mdi-view-sequential-outline'
  },
  columns: {
    dirs: ['row', 'row'],
    label: 'All three side by side',
    icon: 'mdi-view-column-outline'
  },
  sidebar: {
    dirs: ['row', 'col'],
    label: 'List on the left, fields and bytes right',
    icon: 'mdi-view-split-vertical'
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
