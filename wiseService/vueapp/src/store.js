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
    const raw = localStorage.getItem('wiseTheme');
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
    const raw = localStorage.getItem('wiseCustomTheme');
    return raw ? JSON.parse(raw) : null;
  } catch (e) {
    return null;
  }
}

const store = createStore({
  state: {
    wiseTheme: loadSavedTheme(),
    customTheme: loadSavedCustomTheme(),
    statsDataInterval: 30000
  },
  mutations: {
    SET_THEME (state, newTheme) {
      state.wiseTheme = newTheme;
      localStorage.setItem('wiseTheme', newTheme);
    },
    SET_CUSTOM_THEME (state, value) {
      state.customTheme = value;
      if (value) {
        localStorage.setItem('wiseCustomTheme', JSON.stringify(value));
      } else {
        localStorage.removeItem('wiseCustomTheme');
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
