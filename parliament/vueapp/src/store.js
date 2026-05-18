/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import { createStore } from 'vuex';
import { THEMES, DEFAULT_THEME_ID } from '@common/themes/manifest.js';

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
      localStorage.setItem('parliamentTheme', value);
    },
    setCustomTheme (state, value) {
      state.customTheme = value;
      if (value) {
        localStorage.setItem('parliamentCustomTheme', JSON.stringify(value));
      } else {
        localStorage.removeItem('parliamentCustomTheme');
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
