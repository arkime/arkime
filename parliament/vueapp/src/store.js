import Vue from 'vue';
import Vuex from 'vuex';

Vue.use(Vuex);

const store = new Vuex.Store({
  state: {
    theme: 'light',
    isUser: false,
    isAdmin: false,
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
    }
  }
});

export default store;
