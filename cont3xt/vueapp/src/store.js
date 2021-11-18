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
    renderingArray: false,
    integrations: {},
    integrationsError: '',
    integrationsArray: [],
    displayIntegration: {},
    integrationData: {},
    linkGroups: [],
    linkGroupsError: ''
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
    SET_WAIT_RENDERING (state, data) {
      state.waitRendering = data;
    },
    SET_RENDERING_TABLE (state, data) {
      state.renderingTable = data;
    },
    SET_RENDERING_ARRAY (state, data) {
      state.renderingArray = data;
    },
    SET_INTEGRATIONS (state, data) {
      state.integrations = data;
      const integrationsArray = [];
      for (const key in data) {
        integrationsArray.push({
          name: key,
          ...data[key]
        });
      }
      state.integrationsArray = integrationsArray;
    },
    SET_INTEGRATIONS_ERROR (state, data) {
      state.integrationsError = data;
    },
    SET_DISPLAY_INTEGRATION (state, data) {
      state.displayIntegration = data;
    },
    SET_INTEGRATION_DATA (state, data) {
      state.integrationData = Object.freeze(data);
    },
    SET_LINK_GROUPS (state, data) {
      state.linkGroups = data;
    },
    SET_LINK_GROUPS_ERROR (state, data) {
      state.linkGroupsError = data;
    },
    REMOVE_LINK_GROUP (state, index) {
      state.linkGroups.splice(index, 1);
    },
    UPDATE_LINK_GROUP (state, data) {
      for (let i = 0; i < state.linkGroups.length; i++) {
        if (state.linkGroups[i]._id === data._id) {
          Vue.set(state.linkGroups, i, data);
          return;
        }
      }
    }
  },
  getters: {
    getLoading (state) {
      return state.loading;
    },
    getRendering (state) {
      return state.renderingCard;
    },
    getWaitRendering (state) {
      return state.getWaitRendering;
    },
    getRenderingTable (state) {
      return state.renderingTable;
    },
    getRenderingArray (state) {
      return state.renderingArray;
    },
    getIntegrations (state) {
      return state.integrations;
    },
    getIntegrationsError (state) {
      return state.integrationsError;
    },
    getIntegrationsArray (state) {
      return state.integrationsArray;
    },
    getIntegrationData (state) {
      return state.integrationData;
    },
    getLinkGroups (state) {
      return state.linkGroups;
    },
    getLinkGroupsError (state) {
      return state.linkGroupsError;
    }
  }
});

export default store;
