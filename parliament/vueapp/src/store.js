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
    user: undefined,
    roles: [],
    notifiers: [],
    // Theme comes from the server (user.settings.vuetifyTheme /
    // vuetifyCustomTheme) on boot via hydrateThemeFromServer; the
    // manifest default applies until that resolves.
    theme: DEFAULT_THEME_ID,
    customTheme: null,
    isUser: false,
    isAdmin: false,
    parliament: {},
    refreshInterval: 15000,
    stats: {},
    scrollToClusterId: null
  },
  mutations: {
    setUser (state, value) {
      state.user = value;
    },
    setTheme (state, value) {
      state.theme = value;
      // Persist to the shared user.settings.vuetifyTheme so the pick
      // follows the user into viewer / cont3xt / wise too.
      postThemeSettings('api/settings/update', { [VUETIFY_THEME_KEY]: value });
    },
    setCustomTheme (state, value) {
      state.customTheme = value;
      postThemeSettings('api/settings/update', { [VUETIFY_CUSTOM_THEME_KEY]: value ?? null });
    },
    /* Apply theme values from user.settings on app startup. Called by
       App.vue after the initial /parliament/api/user fetch resolves. */
    hydrateThemeFromServer (state, { themeId, customTheme }) {
      if (customTheme && typeof customTheme === 'object' && customTheme.colors) {
        state.customTheme = customTheme;
      }
      if (themeId && (themeId === 'custom1' || THEMES.some(t => t.id === themeId))) {
        state.theme = themeId;
      }
    },
    setIsUser (state, value) {
      state.isUser = value;
    },
    setIsAdmin (state, value) {
      state.isAdmin = value;
    },
    setRefreshInterval (state, value) {
      value = parseInt(value) || 0;
      localStorage.setItem('refreshInterval', value);
      state.refreshInterval = value;
    },
    setRoles (state, value) {
      state.roles = value || [];
    },
    setNotifiers (state, value) {
      state.notifiers = value;
    },
    setParliament (state, value) {
      state.parliament = value;
    },
    setSettings (state, value) {
      state.parliament.settings = value;
    },
    setStats (state, value) {
      state.stats = value;
    },
    setScrollToClusterId (state, value) {
      state.scrollToClusterId = value;
    }
  },
  getters: {
    getUser: state => state.user,
    getRoles: state => state.roles,
    getTheme: state => state.theme,
    getCustomTheme: state => state.customTheme
  }
});

export default store;
