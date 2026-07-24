/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import { createStore } from 'vuex';
import { THEMES, DEFAULT_THEME_ID } from '@common/themes/manifest.js';
import { postThemeSettings } from '@common/themes/persistTheme.js';
import { VUETIFY_THEME_KEY, VUETIFY_CUSTOM_THEME_KEY } from '@common/themes/customTheme.js';

const store = createStore({
  state: {
    // Theme comes from the server (user.settings.vuetifyTheme /
    // vuetifyCustomTheme) on boot via HYDRATE_THEME_FROM_SERVER; the
    // manifest default applies until that resolves.
    wiseTheme: DEFAULT_THEME_ID,
    customTheme: null,
    statsDataInterval: 30000
  },
  mutations: {
    SET_THEME (state, newTheme) {
      state.wiseTheme = newTheme;
      // Persist to the shared user.settings.vuetifyTheme so the pick
      // follows the user into viewer / cont3xt / parliament too.
      postThemeSettings('api/settings/update', { [VUETIFY_THEME_KEY]: newTheme });
    },
    SET_CUSTOM_THEME (state, value) {
      state.customTheme = value;
      postThemeSettings('api/settings/update', { [VUETIFY_CUSTOM_THEME_KEY]: value ?? null });
    },
    /* Apply theme values from user.settings on app startup. Called by
       App.vue after the initial /api/settings fetch resolves. */
    HYDRATE_THEME_FROM_SERVER (state, { themeId, customTheme }) {
      if (customTheme && typeof customTheme === 'object' && customTheme.colors) {
        state.customTheme = customTheme;
      }
      if (themeId && (themeId === 'custom1' || THEMES.some(t => t.id === themeId))) {
        state.wiseTheme = themeId;
      }
    },
    SET_STATS_DATA_INTERVAL (state, newInterval) {
      state.statsDataInterval = newInterval;
    }
  },
  getters: {
    getTheme: state => state.wiseTheme,
    getCustomTheme: state => state.customTheme,
    getStatsDataInterval: state => state.statsDataInterval
  }
});

export default store;
