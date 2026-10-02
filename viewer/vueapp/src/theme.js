/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/

/**
 * Viewer Vuetify theme configuration.
 *
 * Consumes the shared theme manifest at common/vueapp/themes/manifest.js
 * (10 baked-in themes: 8 ports of the original CSS files + 2 new v7
 * themes). The user's per-user custom theme is registered at runtime
 * by App.vue's settings-load path (see common/vueapp/themes/customTheme.js).
 *
 * Vuetify 3 emits --v-theme-{key}: r,g,b custom properties for every
 * color in each theme. All viewer + common Vue components consume
 * these via rgb(var(--v-theme-X)). The legacy --color-* token system
 * + per-theme CSS files (viewer/vueapp/src/themes/*.css) are gone.
 */

import { THEMES, DEFAULT_THEME_ID } from '@common/themes/manifest.js';

export function buildVuetifyThemes () {
  return {
    defaultTheme: DEFAULT_THEME_ID,
    // We hand-author shade variants in the manifest; disable
    // Vuetify's algorithmic variation generation.
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
