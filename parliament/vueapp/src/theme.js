/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/

/**
 * Parliament Vuetify theme configuration.
 *
 * Consumes the shared theme manifest at common/vueapp/themes/manifest.js
 * (same 10 baked-in themes used by viewer + cont3xt).
 */

import { THEMES, DEFAULT_THEME_ID } from '@common/themes/manifest.js';

export function buildVuetifyThemes () {
  return {
    defaultTheme: DEFAULT_THEME_ID,
    variations: { colors: [], lighten: 0, darken: 0 },
    themes: Object.fromEntries(
      THEMES.map(t => [t.id, {
        dark: t.dark,
        colors: {
          // Tooltips + some Vuetify built-ins read these; fall back to
          // foreground / background so we get an inverted-contrast
          // tooltip in every theme instead of a transparent one.
          'surface-variant': t.colors.foreground,
          'on-surface-variant': t.colors.background,
          ...t.colors
        }
      }])
    )
  };
}
