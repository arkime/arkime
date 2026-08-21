/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/

/**
 * Cont3xt Vuetify theme configuration.
 *
 * Cont3xt now consumes the same shared theme manifest as viewer
 * (common/vueapp/themes/manifest.js) -- 10 baked-in themes with
 * unified semantic tokens (primary/secondary/success/info, neutral
 * ramp, surface-card/surface-well/input-bg etc.).
 *
 * The legacy cont3xt-specific token names (cont3xt-card, textarea,
 * well, side-stub, integration-panel) have been generalized into
 * shared surface-card / input-bg / surface-well / surface-sidebar /
 * surface-panel respectively. See cont3xt/vueapp/src/index.scss +
 * cont3xt.css for the renames at the call sites.
 *
 * Per-user Custom themes are registered at runtime in cont3xt's
 * App.vue / store boot path via the shared common/vueapp/themes/customTheme.js
 * helper.
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
