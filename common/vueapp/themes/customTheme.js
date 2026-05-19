/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/

/**
 * Per-user custom theme helpers.
 *
 * In the new theming system, a user's custom theme is a first-class
 * Vuetify theme object with the same { dark, colors } shape as the
 * baked-in entries in manifest.js. It's stored as
 * `settings.vuetifyCustomTheme` (an object) and activated by setting
 * `settings.vuetifyTheme = 'custom1'`.
 *
 * The legacy keys (`settings.theme` / `settings.customTheme`) are
 * left untouched so a user who flips between v7 and legacy arkime
 * keeps a working preference in each version. The boot path reads
 * the new vuetify-prefixed keys first; only if both are absent does
 * it import from the legacy keys as a one-shot fallback, persisting
 * the result into the new keys. Once a user has set anything in v7
 * the new keys are authoritative and the legacy keys are never
 * mutated by v7 code again.
 *
 * For back-compat with the legacy custom-theme format -- where
 * `settings.theme = 'custom1:hex,hex,...'` packed 15 positional
 * colors into a colon-separated string -- this module provides a
 * one-shot migration helper. The App.vue boot path detects the legacy
 * format on first load, calls migrateLegacyCustomTheme() to produce
 * the new shape, and writes the result into the new vuetify keys.
 *
 * Legacy positional schema (15 hexes, ',' separated):
 *   [0]  background        -- was --color-background
 *   [1]  foreground        -- was --color-foreground
 *   [2]  foreground-accent  -- was --color-foreground-accent
 *   [3]  primary            -- was --color-primary
 *   [4]  primary-lightest   -- was --color-primary-lightest
 *   [5]  secondary          -- was --color-secondary
 *   [6]  secondary-lightest -- was --color-secondary-lightest
 *   [7]  tertiary           -- was --color-tertiary
 *   [8]  tertiary-lightest  -- was --color-tertiary-lightest
 *   [9]  quaternary         -- was --color-quaternary
 *   [10] quaternary-lightest -- was --color-quaternary-lightest
 *   [11] water              -- was --color-water
 *   [12] land               -- was --color-land
 *   [13] src                -- was --color-src
 *   [14] dst                -- was --color-dst
 *
 * The intermediate `-light/-lighter/-dark/-darker` shades are
 * computed via colorShades.expandShades() using the same 6%/12%
 * percentages the legacy viewer/views/user.styl stylus rendering
 * used. The `-lightest` variant is supplied verbatim.
 */

import { expandShades, isDark } from './colorShades.js';

const LIGHT_WARNING = '#D97706';
const LIGHT_ERROR = '#DC2626';
const DARK_WARNING = '#FBBF24';
const DARK_ERROR = '#F87171';

const HEX_RE = /^#[0-9a-fA-F]{3,8}$/;

function sanitize (hex, fallback = '#000000') {
  return (hex && HEX_RE.test(String(hex).trim())) ? String(hex).trim() : fallback;
}

/** Returns the sanitized hex if valid, otherwise undefined (so the
 *  caller can fall through to an algorithmic default). */
function maybeHex (hex) {
  return (hex && HEX_RE.test(String(hex).trim())) ? String(hex).trim() : undefined;
}

/**
 * Build a full Vuetify theme object ({dark, colors}) from a 15-color
 * positional schema (whether parsed from a legacy 'custom1:...' string
 * or supplied directly).
 *
 * Computes shade variants via colorShades.expandShades() and fills in
 * the surface / on-* / outline / neutral keys with sensible defaults
 * derived from background/foreground luminance.
 */
export function buildCustomTheme ({
  background, foreground, foregroundAccent,
  primary, primaryLightest,
  secondary, secondaryLightest,
  tertiary, tertiaryLightest,
  quaternary, quaternaryLightest,
  water, land, src, dst
}) {
  const bg = sanitize(background, '#FFFFFF');
  const fg = sanitize(foreground, '#303030');
  const fgAccent = sanitize(foregroundAccent, fg);
  const dark = isDark(bg);

  const buttonFg = dark ? '#0F1115' : '#FFFFFF';
  const outline = dark ? '#555555' : '#CCCCCC';

  const primaryShades = expandShades('primary', sanitize(primary, '#214B78'), maybeHex(primaryLightest));
  const secondaryShades = expandShades('secondary', sanitize(secondary, '#3D7B7E'), maybeHex(secondaryLightest));
  const tertiaryBase = sanitize(tertiary, '#66B689');
  const tertiaryLight = maybeHex(tertiaryLightest);
  const tertiaryShades = expandShades('tertiary', tertiaryBase, tertiaryLight);
  const successShades = expandShades('success', tertiaryBase, tertiaryLight); // alias
  const quaternaryBase = sanitize(quaternary, '#2F7D86');
  const quaternaryLight = maybeHex(quaternaryLightest);
  const quaternaryShades = expandShades('quaternary', quaternaryBase, quaternaryLight);
  const infoShades = expandShades('info', quaternaryBase, quaternaryLight); // alias

  return {
    dark,
    colors: {
      background: bg,
      surface: dark ? '#1A1D23' : '#FFFFFF',
      foreground: fg,
      'foreground-accent': fgAccent,
      'on-background': fg,
      'on-surface': fg,
      'on-primary': buttonFg,
      'on-secondary': buttonFg,
      'on-success': buttonFg,
      'on-info': buttonFg,
      'on-warning': buttonFg,
      'on-error': buttonFg,
      'button-fg': buttonFg,
      outline,

      neutral: '#CCCCCC',
      'neutral-dark': dark ? '#F6F6F6' : '#777777',
      'neutral-darker': dark ? '#EEEEEE' : '#555555',
      'neutral-light': dark ? '#555555' : '#EEEEEE',
      'neutral-lighter': dark ? '#444444' : '#F6F6F6',
      'neutral-lightest': dark ? '#333333' : '#FAFAFA',

      ...primaryShades,
      ...secondaryShades,
      ...tertiaryShades,
      ...successShades,
      ...quaternaryShades,
      ...infoShades,

      warning: dark ? DARK_WARNING : LIGHT_WARNING,
      error: dark ? DARK_ERROR : LIGHT_ERROR,

      // semantic surfaces -- sensible defaults; user can hand-edit
      'surface-card': dark ? '#1A1D23' : '#FAFAFA',
      'surface-card-hover': dark ? '#23272E' : '#EEEEEE',
      'surface-card-border': dark ? '#2E323A' : '#E0E0E0',
      'surface-well': dark ? '#161A1F' : '#F6F6F6',
      'surface-well-border': dark ? '#2E323A' : '#EEEEEE',
      'surface-panel': dark ? '#1A1D23' : '#FFFFFF',
      'surface-sidebar': dark ? '#0F1115' : '#F6F6F6',
      'input-bg': dark ? '#161A1F' : '#FFFFFF',
      'input-bg-disabled': dark ? '#0F1115' : '#EEEEEE',
      'input-border': outline,
      'progress-track': dark ? '#2A2E35' : '#EEEEEE',

      water: sanitize(water, dark ? '#5F7C82' : '#BCD0E0'),
      land: sanitize(land, tertiaryBase),
      'land-dark': sanitize(land, tertiaryBase),
      'land-light': sanitize(land, tertiaryBase),
      src: sanitize(src, '#C43D75'),
      dst: sanitize(dst, '#3D7B7E')
    }
  };
}

/**
 * Detect whether a settings.theme string is in the legacy
 * 'custom1:hex,hex,...' positional format.
 */
export function isLegacyCustomTheme (themeSetting) {
  return typeof themeSetting === 'string' && themeSetting.startsWith('custom1:');
}

/**
 * Parse a legacy 'custom1:h1,h2,...,h15' positional string and return
 * the full Vuetify theme object. Idempotent + fail-safe -- if the
 * string is malformed, returns null and the caller falls back to the
 * default theme.
 */
export function migrateLegacyCustomTheme (legacyString) {
  if (!isLegacyCustomTheme(legacyString)) { return null; }
  const tail = legacyString.slice('custom1:'.length);
  const hexes = tail.split(',').map(s => s.trim());
  if (hexes.length < 15) { return null; }
  return buildCustomTheme({
    background: hexes[0],
    foreground: hexes[1],
    foregroundAccent: hexes[2],
    primary: hexes[3],
    primaryLightest: hexes[4],
    secondary: hexes[5],
    secondaryLightest: hexes[6],
    tertiary: hexes[7],
    tertiaryLightest: hexes[8],
    quaternary: hexes[9],
    quaternaryLightest: hexes[10],
    water: hexes[11],
    land: hexes[12],
    src: hexes[13],
    dst: hexes[14]
  });
}

/**
 * The custom theme id. There's exactly one custom slot per user.
 */
export const CUSTOM_THEME_ID = 'custom1';

/**
 * user.settings keys where v7+ persists the theme preference. The
 * legacy keys (`theme` / `customTheme`) are deliberately NOT touched
 * so a user who flips between v7 and legacy arkime keeps a working
 * preference in each version.
 */
export const VUETIFY_THEME_KEY = 'vuetifyTheme';
export const VUETIFY_CUSTOM_THEME_KEY = 'vuetifyCustomTheme';

/**
 * Strip the legacy `-theme` suffix from a stored id (e.g.
 * `'arkime-dark-theme'` -> `'arkime-dark'`). No-op for already-clean
 * ids or empty values.
 */
export function stripLegacyThemeSuffix (themeId) {
  if (typeof themeId !== 'string') return themeId;
  return themeId.replace(/-theme$/, '');
}

/**
 * Resolve which theme to activate from a user's `settings` blob.
 *
 * Read order:
 *   1. `settings.vuetifyTheme` / `settings.vuetifyCustomTheme`  (authoritative for v7+)
 *   2. one-shot import from the legacy keys  (`settings.theme` /
 *      `settings.customTheme`, including the legacy `custom1:hex,hex,...`
 *      string format)
 *   3. nothing -> caller falls back to OS color-scheme default
 *
 * Returns `{ themeId, customTheme, fromLegacy }`. `fromLegacy` is
 * true when the values were imported from the legacy keys -- the
 * caller should then persist `themeId` + `customTheme` into the new
 * keys so subsequent loads skip the import.
 */
export function pickStoredTheme (settings) {
  if (!settings) return { themeId: null, customTheme: null, fromLegacy: false };

  // 1. Prefer the v7 keys.
  const v7Id = settings[VUETIFY_THEME_KEY];
  const v7Custom = settings[VUETIFY_CUSTOM_THEME_KEY];
  if (v7Id || (v7Custom && v7Custom.colors)) {
    return {
      themeId: v7Id || (v7Custom && v7Custom.colors ? CUSTOM_THEME_ID : null),
      customTheme: (v7Custom && v7Custom.colors) ? v7Custom : null,
      fromLegacy: false
    };
  }

  // 2. Legacy import. Three legacy shapes to consider:
  //    a. legacy custom1:hex,hex,... string
  //    b. modern { dark, colors } object stored under settings.customTheme
  //       (early-dev7 users got their custom theme written here)
  //    c. plain string id, possibly with `-theme` suffix
  const legacyTheme = settings.theme;
  if (isLegacyCustomTheme(legacyTheme)) {
    const migrated = migrateLegacyCustomTheme(legacyTheme);
    if (migrated) {
      return { themeId: CUSTOM_THEME_ID, customTheme: migrated, fromLegacy: true };
    }
  }
  if (settings.customTheme && settings.customTheme.colors) {
    return {
      themeId: stripLegacyThemeSuffix(legacyTheme) || CUSTOM_THEME_ID,
      customTheme: settings.customTheme,
      fromLegacy: true
    };
  }
  if (legacyTheme && legacyTheme !== 'default-theme') {
    return { themeId: stripLegacyThemeSuffix(legacyTheme), customTheme: null, fromLegacy: true };
  }

  return { themeId: null, customTheme: null, fromLegacy: false };
}

/**
 * The base color keys that make up a shareable custom theme. Shade
 * variants and on-* keys are recomputed on import; only these base
 * colors travel between users.
 */
export const SHAREABLE_KEYS = [
  'background', 'foreground', 'foreground-accent',
  'primary', 'secondary', 'success', 'info',
  'warning', 'error',
  'water', 'land', 'src', 'dst'
];

/**
 * Encode a custom theme into a comma-separated `key:value` share
 * string. Only SHAREABLE_KEYS are included; other keys (shades, on-*,
 * surfaces) are derived on import.
 */
export function encodeShareableTheme (customTheme) {
  if (!customTheme || !customTheme.colors) return '';
  return SHAREABLE_KEYS
    .filter(k => typeof customTheme.colors[k] === 'string' && HEX_RE.test(customTheme.colors[k]))
    .map(k => `${k}:${customTheme.colors[k]}`)
    .join(',');
}

/**
 * Parse a share string into a `{ key: hex }` overrides map. Tolerates
 * whitespace, ignores unknown keys, ignores invalid hexes. Returns
 * null if no valid pairs were parsed.
 */
export function decodeShareableTheme (shareString) {
  if (typeof shareString !== 'string' || !shareString.trim()) return null;
  const overrides = {};
  for (const pair of shareString.split(',')) {
    const idx = pair.indexOf(':');
    if (idx < 0) continue;
    const key = pair.slice(0, idx).trim();
    const value = pair.slice(idx + 1).trim();
    if (!SHAREABLE_KEYS.includes(key)) continue;
    if (!HEX_RE.test(value)) continue;
    overrides[key] = value;
  }
  return Object.keys(overrides).length ? overrides : null;
}

/**
 * Apply a share string's overrides on top of a base custom theme,
 * producing a new { dark, colors } object. Shade variants for
 * primary/secondary/success/info are recomputed via expandShades; the
 * `dark` flag is recomputed from the new background if one was
 * supplied. Mirrors ThemePicker.updateColor() so a pasted share looks
 * identical to one built by clicking through the editor.
 */
export function applyShareableTheme (baseCustomTheme, shareString) {
  const overrides = decodeShareableTheme(shareString);
  if (!overrides) return null;

  const baseColors = (baseCustomTheme && baseCustomTheme.colors) || {};
  const newColors = { ...baseColors, ...overrides };

  if (overrides.primary) Object.assign(newColors, expandShades('primary', overrides.primary));
  if (overrides.secondary) Object.assign(newColors, expandShades('secondary', overrides.secondary));
  if (overrides.success) {
    Object.assign(newColors, expandShades('success', overrides.success));
    Object.assign(newColors, expandShades('tertiary', overrides.success));
  }
  if (overrides.info) {
    Object.assign(newColors, expandShades('info', overrides.info));
    Object.assign(newColors, expandShades('quaternary', overrides.info));
  }

  const dark = overrides.background
    ? isDark(overrides.background)
    : (baseCustomTheme ? !!baseCustomTheme.dark : false);

  return { dark, colors: newColors };
}
