/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/

/**
 * WISE Vuetify theme configuration.
 *
 * Consumes the shared theme manifest at common/vueapp/themes/manifest.js
 * (same 10 baked-in themes used by viewer + cont3xt + parliament).
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
          'surface-variant': t.colors.foreground,
          'on-surface-variant': t.colors.background,
          ...t.colors
        }
      }])
    )
  };
}
