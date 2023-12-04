/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import Vue from 'vue';
import Vuex from 'vuex';

Vue.use(Vuex);

const store = new Vuex.Store({
  state: {
    user: undefined,
    roles: [],
    notifiers: [],
    theme: 'light',
    isUser: false,
    isAdmin: false,
    parliament: {},
    refreshInterval: 15000
  },
  mutations: {
    setUser (state, value) {
      state.user = value;
    },
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
    }
  },
  getters: {
    getUser: state => state.user,
    getRoles: state => state.roles
  }
});

export default store;
