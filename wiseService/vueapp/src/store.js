/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import { createStore } from 'vuex';

const store = createStore({
  state: {
    wiseTheme: 'light',
    statsDataInterval: 30000
  },
  mutations: {
    SET_THEME (state, newTheme) {
      state.wiseTheme = newTheme;
    },
    SET_STATS_DATA_INTERVAL (state, newTheme) {
      state.statsDataInterval = newTheme;
    }
  },
  getters: {
    getTheme: state => state.wiseTheme,
    getStatsDataInterval: state => state.statsDataInterval
  }
});

export default store;
