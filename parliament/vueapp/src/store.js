import Vue from 'vue';
import Vuex from 'vuex';

Vue.use(Vuex);

const store = new Vuex.Store({
  state: {
    isUser: false,
    isAdmin: false,
    hasAuth: false,
    loggedIn: false,
    commonAuth: false,
    dashboardOnly: false,
    refreshInterval: 15000
  },
  mutations: {
    setIsUser (state, value) {
      state.isUser = value;
    },
    setIsAdmin (state, value) {
      state.isAdmin = value;
    },
    setHasAuth (state, value) {
      state.hasAuth = value;
    },
    setLoggedIn (state, value) {
      state.loggedIn = value;
    },
    setCommonAuth (state, value) {
      state.commonAuth = value;
    },
    setDashboardOnly (state, value) {
      state.dashboardOnly = value;
    },
    setRefreshInterval (state, value) {
      value = parseInt(value) || 0;
      localStorage.setItem('refreshInterval', value);
      state.refreshInterval = value;
    }
  }
});

export default store;
