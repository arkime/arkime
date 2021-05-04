import Vue from 'vue';
import Vuex from 'vuex';
import createPersistedState from 'vuex-persistedstate';

Vue.use(Vuex);

export default new Vuex.Store({
  state: {
    wiseTheme: 'light',
    statsDataInterval: 0
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
    getTheme (state) {
      return state.wiseTheme;
    },
    getStatsDataInterval (state) {
      return state.statsDataInterval;
    }
  },
  plugins: [createPersistedState({
    paths: [ // only these state variables are persisted to localstorage
      'wiseTheme', 'statsDataInterval'
    ]
  })]
});
