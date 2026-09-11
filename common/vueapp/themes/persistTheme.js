/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import setReqHeaders from '@common/setReqHeaders';
import { VUETIFY_THEME_KEY, VUETIFY_CUSTOM_THEME_KEY } from '@common/themes/customTheme.js';

/**
 * Fire-and-forget POST of the shared Vuetify theme keys to an app's
 * /api/settings/update endpoint. Used by cont3xt / parliament / wise
 * stores to persist a theme pick to `user.settings.vuetifyTheme` /
 * `vuetifyCustomTheme` -- the same keys every Arkime app reads/writes,
 * so the pick (including a custom palette) follows the user across apps,
 * browsers, and devices. `user.settings` is the single source of truth.
 * Failures are silently ignored -- the next theme change retries.
 *
 * @param {string} url - The /api/settings/update POST endpoint URL.
 * @param {object} payload - Settings keys to persist (e.g.
 *                           { vuetifyTheme: 'arkime-dark' }).
 */
export function postThemeSettings (url, payload) {
  try {
    fetch(url, {
      method: 'POST',
      headers: setReqHeaders({ 'Content-Type': 'application/json' }),
      body: JSON.stringify(payload)
    }).catch(() => { /* ignore */ });
  } catch (e) { /* ignore */ }
}

/**
 * On app startup, apply the user's server-saved Vuetify theme. The
 * server (`user.settings.vuetifyTheme` / `vuetifyCustomTheme`) is the
 * single source of truth, so a theme set in any app -- including a
 * custom palette -- follows the user everywhere. Apps render the
 * default theme until this resolves (same as viewer).
 *
 * @param {object} settings - The user.settings blob (viewer / cont3xt /
 *        parliament) or the wise /api/settings response, holding the
 *        shared theme keys.
 * @param {function} applyHydrated - (themeId, customTheme) => void;
 *        invoked with the server values when present.
 */
export function applyServerTheme (settings, applyHydrated) {
  const themeId = settings?.[VUETIFY_THEME_KEY];
  const customTheme = settings?.[VUETIFY_CUSTOM_THEME_KEY];
  if (themeId || (customTheme && customTheme.colors)) {
    applyHydrated(themeId, customTheme);
  }
}
