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
    renderingCard: false,
    waitRendering: false,
    renderingTable: false,
    integrations: {},
    displayIntegration: {},
    integrationData: {}
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
    SET_RENDERING_CARD (state, data) {
      state.renderingCard = data;
    },
    WAIT_RENDERING (state, data) {
      state.waitRendering = data;
    },
    SET_RENDERING_TABLE (state, data) {
      state.renderingTable = data;
    },
    SET_INTEGRATIONS (state, data) {
      state.integrations = data;
    },
    SET_DISPLAY_INTEGRATION (state, data) {
      state.displayIntegration = data;
    },
    SET_INTEGRATION_DATA (state, data) {
      state.integrationData = Object.freeze(data);
    }
  },
  getters: {
    getLoading (state) {
      return state.loading;
    },
    getRendering (state) {
      return state.renderingCard;
    },
    waitRendering (state) {
      return state.waitRendering;
    },
    getRenderingTable (state) {
      return state.renderingTable;
    },
    getIntegrations (state) {
      return state.integrations;
    },
    getIntegrationData (state) {
      return state.integrationData;
    }
  }
});

export default store;
