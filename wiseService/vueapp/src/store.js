/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import { createStore } from 'vuex';

const store = createStore({
  state: {
    wiseTheme: localStorage.getItem('wiseTheme') || 'light',
    statsDataInterval: 30000
  },
  mutations: {
    SET_THEME (state, newTheme) {
      state.wiseTheme = newTheme;
      localStorage.setItem('wiseTheme', newTheme);
    },
    SET_STATS_DATA_INTERVAL (state, newInterval) {
      state.statsDataInterval = newInterval;
    }
  },
  getters: {
    getTheme: state => state.wiseTheme,
    getStatsDataInterval: state => state.statsDataInterval
  }
});

export default store;
