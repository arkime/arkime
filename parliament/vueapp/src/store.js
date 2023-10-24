import Vue from 'vue';
import Vuex from 'vuex';

Vue.use(Vuex);

const store = new Vuex.Store({
  state: {
    roles: [],
    notifiers: [],
    theme: 'light',
    isUser: false,
    isAdmin: false,
    parliament: {},
    refreshInterval: 15000
  },
  mutations: {
    setTheme (state, value) {
      state.theme = value;
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
      state.roles = Vue.filter('parseRoles')(value);
    },
    setNotifiers (state, value) {
      state.notifiers = value;
    },
    setParliament (state, value) {
      state.parliament = value;
    },
    setSettings (state, value) {
      state.parliament.settings = value;
    }
  }
});

export default store;
