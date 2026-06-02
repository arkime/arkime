/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import setReqHeaders from '@common/setReqHeaders';
import { VUETIFY_THEME_KEY, VUETIFY_CUSTOM_THEME_KEY } from '@common/themes/customTheme.js';

/**
 * Fire-and-forget POST of one or more theme keys to an app's
 * /api/user/settings endpoint. Used by cont3xt / parliament / wise
 * stores to persist Vuetify theme picks to the shared `vuetifyTheme` /
 * `vuetifyCustomTheme` keys on `user.settings` -- the same keys every
 * Arkime app reads/writes, so a pick follows the user across all apps
 * and browsers. localStorage is still the source of truth for the
 * immediate paint after reload; this server save lets another app or
 * browser/device pick up the same preference. Failures are silently
 * ignored -- the next theme change retries.
 *
 * @param {string} url - The /api/user/settings POST endpoint URL.
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
 * On app startup: apply the user's server-saved theme if present, else
 * migrate the values they already have in localStorage up to the server
 * so the preference follows them to other browsers/devices on next load.
 *
 * Read order:
 *   1. settings[serverThemeKey] / settings[serverCustomKey] -> applyHydrated()
 *   2. localStorage[localThemeKey] / localStorage[localCustomKey] -> POST to url
 *   3. nothing at all -> caller's existing default applies
 *
 * @param {object} opts
 * @param {string} opts.url - POST endpoint for the server save
 * @param {object} opts.settings - The user.settings blob from /api/user
 * @param {string} [opts.serverThemeKey] - server settings key for the theme
 *        id; defaults to the shared cross-app 'vuetifyTheme' key.
 * @param {string} [opts.serverCustomKey] - server settings key for the
 *        custom theme; defaults to the shared 'vuetifyCustomTheme' key.
 * @param {string} opts.localThemeKey - localStorage key for the theme id
 * @param {string} opts.localCustomKey - localStorage key for the custom theme
 * @param {boolean} [opts.themeIsJsonEncoded] - true if the theme value in
 *        localStorage is JSON-stringified (cont3xt) vs a raw string
 *        (parliament, wise). Custom theme is always JSON.
 * @param {function} applyHydrated - (themeId, customTheme) => void; invoked
 *        with the server values when found, so the caller can commit them.
 */
export function hydrateOrMigrateTheme (opts, applyHydrated) {
  const settings = opts.settings ?? {};
  const serverThemeKey = opts.serverThemeKey ?? VUETIFY_THEME_KEY;
  const serverCustomKey = opts.serverCustomKey ?? VUETIFY_CUSTOM_THEME_KEY;
  const themeId = settings[serverThemeKey];
  const customTheme = settings[serverCustomKey];
  if (themeId || (customTheme && customTheme.colors)) {
    applyHydrated(themeId, customTheme);
    return;
  }

  const payload = {};
  const lsTheme = localStorage.getItem(opts.localThemeKey);
  if (lsTheme) {
    try {
      payload[serverThemeKey] = opts.themeIsJsonEncoded ? JSON.parse(lsTheme) : lsTheme;
    } catch (e) { /* ignore */ }
  }
  const lsCustom = localStorage.getItem(opts.localCustomKey);
  if (lsCustom) {
    try { payload[serverCustomKey] = JSON.parse(lsCustom); } catch (e) { /* ignore */ }
  }
  if (Object.keys(payload).length > 0) {
    postThemeSettings(opts.url, payload);
  }
}
