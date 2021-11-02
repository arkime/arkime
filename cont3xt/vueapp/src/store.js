import Vue from 'vue';
import Vuex from 'vuex';

Vue.use(Vuex);

const store = new Vuex.Store({
  state: {
    loading: {
      total: 0,
      failed: 0,
      received: 0,
      done: false,
      failures: []
    },
    integrations: {}
  },
  mutations: {
    SET_LOADING (state, data) {
      const { total, failed, received, done, failure } = data;
      state.loading.done = done || state.loading.done;
      state.loading.total = total || state.loading.total;
      state.loading.failed = failed || state.loading.failed;
      state.loading.received = received || state.loading.received;
      if (failure) {
        state.loading.failures.push(failure);
      }
    },
    RESET_LOADING (state) {
      state.loading = {
        total: 0,
        failed: 0,
        received: 0,
        done: false,
        failures: []
      };
    },
    SET_INTEGRATIONS (state, data) {
      state.integrations = data;
    }
  },
  getters: {
    getLoading (state) {
      return state.loading;
    },
    getIntegrations (state) {
      return state.integrations;
    }
  }
});

export default store;
