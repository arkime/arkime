/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import { createStore } from 'vuex';
import { THEMES, DEFAULT_THEME_ID } from '@common/themes/manifest.js';
import { postThemeSettings } from '@common/themes/persistTheme.js';
import { VUETIFY_THEME_KEY, VUETIFY_CUSTOM_THEME_KEY } from '@common/themes/customTheme.js';

/* Load the saved theme id from localStorage. Migrates the legacy
   'light' / 'dark' string format to the new manifest ids. Falls back
   to the manifest default for missing or unknown values. */
function loadSavedTheme () {
  try {
    const raw = localStorage.getItem('parliamentTheme');
    if (!raw) return DEFAULT_THEME_ID;
    if (raw === 'light') return 'arkime-light';
    if (raw === 'dark') return 'arkime-dark';
    if (typeof raw !== 'string') return DEFAULT_THEME_ID;
    if (raw === 'custom1') return raw;
    return THEMES.some(t => t.id === raw) ? raw : DEFAULT_THEME_ID;
  } catch (e) {
    return DEFAULT_THEME_ID;
  }
}

function loadSavedCustomTheme () {
  try {
    const raw = localStorage.getItem('parliamentCustomTheme');
    return raw ? JSON.parse(raw) : null;
  } catch (e) {
    return null;
  }
}

const store = createStore({
  state: {
    user: undefined,
    roles: [],
    notifiers: [],
    theme: loadSavedTheme(),
    customTheme: loadSavedCustomTheme(),
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
      // localStorage stays per-app (per-origin fast-paint cache); the
      // server save uses the shared `vuetifyTheme` key every Arkime app
      // reads/writes, so the pick follows the user into viewer / cont3xt
      // / wise too.
      localStorage.setItem('parliamentTheme', value);
      postThemeSettings('api/settings/update', { [VUETIFY_THEME_KEY]: value });
    },
    setCustomTheme (state, value) {
      state.customTheme = value;
      if (value) {
        localStorage.setItem('parliamentCustomTheme', JSON.stringify(value));
      } else {
        localStorage.removeItem('parliamentCustomTheme');
      }
      postThemeSettings('api/settings/update', { [VUETIFY_CUSTOM_THEME_KEY]: value ?? null });
    },
    /* Apply theme values loaded from user.settings on app startup
       *without* echoing them back to the server. Called by App.vue
       after the initial /parliament/api/user fetch resolves. */
    hydrateThemeFromServer (state, { themeId, customTheme }) {
      if (customTheme && typeof customTheme === 'object' && customTheme.colors) {
        state.customTheme = customTheme;
        try { localStorage.setItem('parliamentCustomTheme', JSON.stringify(customTheme)); } catch (e) { /* ignore */ }
      }
      if (themeId && (themeId === 'custom1' || THEMES.some(t => t.id === themeId))) {
        state.theme = themeId;
        try { localStorage.setItem('parliamentTheme', themeId); } catch (e) { /* ignore */ }
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
