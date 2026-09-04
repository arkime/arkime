/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Shared categorical color palettes for the dashboard charts (bar / pie / treemap).
The palette is a dashboard-level setting; the chart components build a
d3.scaleOrdinal() from colorRange().
*/

// Palette options for the dashboard palette picker (value + display label)
export const CHART_PALETTES = [
  { value: 'rainbow', label: 'Rainbow' },
  { value: 'tableau10', label: 'Tableau' },
  { value: 'category10', label: 'Category' },
  { value: 'viridis', label: 'Viridis' },
  { value: 'cool', label: 'Cool' },
  { value: 'warm', label: 'Warm' },
  { value: 'spectral', label: 'Spectral' }
];

const PALETTE_VALUES = CHART_PALETTES.map(p => p.value);

/**
 * Returns a color range array for the given palette, suitable for
 * d3.scaleOrdinal(range). Fixed-scheme palettes return their array (scaleOrdinal
 * cycles them); interpolated palettes are quantized to the data length.
 * @param {object} d3       The (lazy-loaded) d3 module
 * @param {string} scheme   A palette value from CHART_PALETTES
 * @param {number} n        Number of data points
 * @returns {string[]}      Array of CSS colors
 */
export function colorRange (d3, scheme, n) {
  const count = Math.max(2, n || 2);
  switch (scheme) {
  case 'tableau10': return d3.schemeTableau10;
  case 'category10': return d3.schemeCategory10;
  case 'viridis': return d3.quantize(d3.interpolateViridis, count);
  case 'cool': return d3.quantize(d3.interpolateCool, count);
  case 'warm': return d3.quantize(d3.interpolateWarm, count);
  case 'spectral': return d3.quantize(d3.interpolateSpectral, count);
  case 'rainbow':
  default: return d3.quantize(d3.interpolateRainbow, count + 1);
  }
}

/** Normalize an arbitrary value to a known palette (fallback: rainbow). */
export function normalizePalette (scheme) {
  return PALETTE_VALUES.includes(scheme) ? scheme : 'rainbow';
}

// Sequential interpolators for the heatmap (intensity needs a gradient, not a
// categorical set). Categorical-only palettes have no entry -> the heatmap keeps
// its themed accent intensity.
const HEATMAP_INTERPOLATORS = {
  viridis: 'interpolateViridis',
  cool: 'interpolateCool',
  warm: 'interpolateWarm',
  spectral: 'interpolateSpectral',
  rainbow: 'interpolateTurbo' // monotonic rainbow, readable for intensity
};

/**
 * Resolve a palette to a heatmap intensity interpolator (t in [0,1] -> color).
 * Lazy-loads d3 only when a gradient palette is selected.
 * @returns {Promise<Function|null>} interpolator, or null for categorical palettes
 */
export async function heatmapInterpolator (scheme) {
  const interpName = HEATMAP_INTERPOLATORS[scheme];
  if (!interpName) { return null; }
  const d3 = await import('d3');
  return d3[interpName];
}
