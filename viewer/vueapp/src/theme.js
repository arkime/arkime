// Minimal Vuetify theme scaffold for the viewer.
//
// During the Vuetify migration the 8 existing CSS theme files in src/themes/*
// (purp, blue, green, cotton-candy, dark-2, dark-3, arkime-light, arkime-dark)
// continue to provide the actual visual treatment via --color-* CSS variables
// and a body class toggle (see App.vue). Vuetify's theme system here is a
// minimal light/dark scaffolding those CSS overlays sit on top of -- a proper
// Vuetify-native theme rework is deferred to a post-migration redesign pass.
//
// When that redesign happens, this file is the place to translate the 8
// existing themes into Vuetify theme objects. Until then, leave the colors
// block empty and let the legacy CSS drive the look.

/**
 * @param {'light' | 'dark'} variant
 * @returns {{ dark: boolean, colors: Object<string, string> }}
 */
export function createViewerTheme (variant) {
  return {
    dark: variant === 'dark',
    colors: {
      // Add overrides here only when needed. The src/themes/*.css files
      // continue to drive the actual look via --color-* custom properties.
    }
  };
}
