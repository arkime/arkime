/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/

import { isRef } from 'vue';

/**
 * Register or update a runtime Vuetify theme.
 *
 * Vuetify 3's `genCssVariables` iterates BOTH `theme.colors` AND
 * `theme.variables` for every registered theme. `createVuetify()` runs
 * each baked-in theme through `parseThemeOptions`, which fills in
 * defaults for `variables` -- including the `hover-opacity` /
 * `focus-opacity` / `border-opacity` etc. that Vuetify components rely
 * on for their interaction overlays. A direct runtime write to
 * `$vuetify.theme.themes[id] = obj` bypasses that parsing step, so we
 * must hydrate `variables` ourselves -- otherwise `--v-hover-opacity`
 * (and friends) come back empty, hover overlays default to full
 * opacity, and every hover paints the element solid `currentColor`
 * over the content.
 *
 * We inherit `variables` from any existing manifest theme (Vuetify
 * already populated those at createVuetify time), so we stay in sync
 * with whatever opacities Vuetify uses by default in this version
 * rather than hardcoding them.
 *
 * Also normalizes the Ref vs. plain-object discrepancy across Vuetify
 * versions: some builds expose `theme.themes` as a Ref, others as the
 * unwrapped reactive record.
 */

function inheritVariables (themesRecord) {
  if (!themesRecord) return {};
  for (const id of Object.keys(themesRecord)) {
    const entry = themesRecord[id];
    if (entry && entry.variables && Object.keys(entry.variables).length) {
      return { ...entry.variables };
    }
  }
  return {};
}

export function registerVuetifyTheme (vuetify, themeId, themeObj) {
  if (!vuetify || !vuetify.theme || !vuetify.theme.themes) return;

  const themes = vuetify.theme.themes;
  const themesRecord = isRef(themes) ? themes.value : themes;

  // If the caller didn't supply variables, copy them from a baked-in
  // theme so the hover/focus/border opacities stay defined.
  const callerVars = themeObj && themeObj.variables;
  const variables = callerVars && Object.keys(callerVars).length
    ? { ...callerVars }
    : inheritVariables(themesRecord);

  const fullThemeObj = {
    dark: !!(themeObj && themeObj.dark),
    colors: { ...(themeObj && themeObj.colors) },
    variables
  };

  if (isRef(themes)) {
    themes.value = { ...themes.value, [themeId]: fullThemeObj };
  } else {
    themes[themeId] = fullThemeObj;
  }
}
