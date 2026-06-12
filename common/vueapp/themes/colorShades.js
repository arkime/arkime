/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/

/**
 * Color-shade computation utilities for the theme manifest.
 *
 * Used in two places:
 *   1. Manifest authoring (themes/manifest.js) -- the manifest spells
 *      out base hexes per family (primary, secondary, tertiary,
 *      quaternary, neutral) and expandShades() fills in the
 *      -light/-lighter/-dark/-darker variants at module-load time.
 *   2. Legacy custom-theme migration (themes/customTheme.js) -- the
 *      user's saved 15-hex positional string is parsed and expanded
 *      into a full Vuetify theme object using the same algorithm.
 *
 * Shade percentages match the stylus formulas in
 * viewer/views/user.styl (now deprecated): darken/lighten by 6% for
 * -dark/-light, 12% for -darker/-lighter, and 24% for -darkest/-lightest
 * (when an explicit -lightest hex isn't supplied).
 *
 * Pure functions, no external dependencies, no Vue/Vuetify import.
 */

/* ---- hex <-> HSL conversion ------------------------------------------ */

/** Parse a #RGB / #RRGGBB / #RRGGBBAA hex string into [r,g,b,a] (0-255 / 0-1). */
function parseHex (hex) {
  let s = String(hex || '').trim();
  if (s.startsWith('#')) { s = s.slice(1); }
  if (s.length === 3) { s = s.split('').map(c => c + c).join(''); }
  const r = parseInt(s.slice(0, 2), 16) || 0;
  const g = parseInt(s.slice(2, 4), 16) || 0;
  const b = parseInt(s.slice(4, 6), 16) || 0;
  return [r, g, b];
}

/** Format [r,g,b] (0-255) as #RRGGBB. */
function toHex (r, g, b) {
  const clamp = v => Math.max(0, Math.min(255, Math.round(v)));
  const h = v => clamp(v).toString(16).padStart(2, '0');
  return `#${h(r)}${h(g)}${h(b)}`;
}

/** [r,g,b] (0-255) -> [h,s,l] (h:0-360, s/l:0-100). */
function rgbToHsl (r, g, b) {
  r /= 255; g /= 255; b /= 255;
  const max = Math.max(r, g, b);
  const min = Math.min(r, g, b);
  let h = 0;
  let s = 0;
  const l = (max + min) / 2;
  if (max !== min) {
    const d = max - min;
    s = l > 0.5 ? d / (2 - max - min) : d / (max + min);
    switch (max) {
    case r: h = ((g - b) / d) + (g < b ? 6 : 0); break;
    case g: h = ((b - r) / d) + 2; break;
    case b: h = ((r - g) / d) + 4; break;
    }
    h *= 60;
  }
  return [h, s * 100, l * 100];
}

/** [h,s,l] (h:0-360, s/l:0-100) -> [r,g,b] (0-255). */
function hslToRgb (h, s, l) {
  h = ((h % 360) + 360) % 360;
  s = Math.max(0, Math.min(100, s)) / 100;
  l = Math.max(0, Math.min(100, l)) / 100;
  const c = (1 - Math.abs(2 * l - 1)) * s;
  const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
  const m = l - c / 2;
  let r;
  let g;
  let b;
  if (h < 60) { r = c; g = x; b = 0; } else if (h < 120) { r = x; g = c; b = 0; } else if (h < 180) { r = 0; g = c; b = x; } else if (h < 240) { r = 0; g = x; b = c; } else if (h < 300) { r = x; g = 0; b = c; } else { r = c; g = 0; b = x; }
  return [(r + m) * 255, (g + m) * 255, (b + m) * 255];
}

/* ---- public API ------------------------------------------------------- */

/**
 * Lighten a hex color by the given percentage (0-100, absolute lightness shift).
 * Matches stylus' lighten() behavior: adds pct to the L of HSL.
 */
export function lighten (hex, pct) {
  const [r, g, b] = parseHex(hex);
  const [h, s, l] = rgbToHsl(r, g, b);
  const [nr, ng, nb] = hslToRgb(h, s, l + pct);
  return toHex(nr, ng, nb);
}

/**
 * Darken a hex color by the given percentage (0-100, absolute lightness shift).
 * Matches stylus' darken() behavior: subtracts pct from the L of HSL.
 */
export function darken (hex, pct) {
  return lighten(hex, -pct);
}

/**
 * Returns true if the color reads as dark (i.e., the background is dark
 * enough that we should flip to a dark Vuetify theme).
 *
 * Uses perceived luminance (the same coefficients Material Design uses
 * for accessibility contrast). Threshold 0.5 -- below = dark.
 */
export function isDark (hex) {
  const [r, g, b] = parseHex(hex);
  // Relative luminance approximation (sRGB to linear-ish, weighted).
  const lum = (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255;
  return lum < 0.5;
}

/**
 * Given a base color and optional `-lightest` override, compute the full
 * set of 5 shades for a family (e.g. primary, primaryLight, primaryLighter,
 * primaryLightest, primaryDark, primaryDarker).
 *
 * The `base` argument is just the family base hex. `lightest` is optional
 * -- if supplied (as it is in legacy custom themes that explicitly save a
 * -lightest hex), it's used verbatim; otherwise computed as lighten(24%).
 *
 * Returned keys use kebab-case suffixes to match Vuetify color naming
 * conventions used by the manifest.
 *
 * Example:
 *   expandShades('primary', '#214B78')
 *   -> { primary: '#214B78', 'primary-light': '#...', 'primary-lighter': '#...',
 *        'primary-lightest': '#...', 'primary-dark': '#...', 'primary-darker': '#...' }
 */
export function expandShades (family, base, lightest) {
  return {
    [family]: base,
    [`${family}-light`]: lighten(base, 6),
    [`${family}-lighter`]: lighten(base, 12),
    [`${family}-lightest`]: lightest || lighten(base, 24),
    [`${family}-dark`]: darken(base, 6),
    [`${family}-darker`]: darken(base, 12)
  };
}
