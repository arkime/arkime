/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/

/**
 * Resolve a Vuetify theme token (e.g. 'primary', 'tertiary-light',
 * 'foreground') to a CSS rgb() string suitable for D3 fills, canvas
 * paints, Flot/jvectormap colors, or anywhere a real color value is
 * needed at runtime.
 *
 * Vuetify 3 emits each registered theme color as `--v-theme-{key}` with
 * a comma-separated `r,g,b` triplet on :root. Wrapping in rgb() yields
 * a value that paints anywhere.
 *
 * Used by chart components that read the current theme via JS:
 *   Connections, TimelineGraph, WorldMap, CaptureStats, CaptureGraphs,
 *   Hierarchy, Summary.
 *
 * Token renames since the legacy --color-* system:
 *   --color-gray*  -> --v-theme-neutral*
 *   --color-button -> --v-theme-button-fg
 *   --color-inputs -> --v-theme-input-bg
 *   --color-danger -> --v-theme-error
 */
export function themedColor (key, fallback = '#000000') {
  if (typeof window === 'undefined' || !document || !document.documentElement) {
    return fallback;
  }
  const styles = window.getComputedStyle(document.documentElement);
  const triplet = styles.getPropertyValue(`--v-theme-${key}`).trim();
  return triplet ? `rgb(${triplet})` : fallback;
}
