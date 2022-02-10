import Vue from 'vue';
import Vuex from 'vuex';
import createPersistedState from 'vuex-persistedstate';

Vue.use(Vuex);

const store = new Vuex.Store({
  state: {
    user: undefined,
    roles: [],
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
    linkGroups: undefined,
    linkGroupsError: '',
    checkedLinks: {},
    selectedIntegrations: undefined,
    sidebarKeepOpen: false
  },
  mutations: {
    SET_USER (state, data) {
      state.user = data;
    },
    SET_ROLES (state, data) {
      state.roles = data || [];
    },
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
    },
    TOGGLE_CHECK_LINK (state, { lgId, lname }) {
      const clone = JSON.parse(JSON.stringify(state.checkedLinks));

      if (!clone[lgId]) {
        clone[lgId] = {};
      }

      if (clone[lgId][lname]) {
        clone[lgId][lname] = false;
      } else {
        clone[lgId][lname] = true;
      }

      Vue.set(state, 'checkedLinks', clone);
    },
    TOGGLE_CHECK_ALL_LINKS (state, { lgId, checked }) {
      const clone = JSON.parse(JSON.stringify(state.checkedLinks));

      for (const lg of state.linkGroups) {
        if (lg._id === lgId) {
          if (!state.checkedLinks[lgId]) {
            clone[lgId] = {};
          }

          for (const link of lg.links) {
            clone[lgId][link.name] = checked;
          }

          Vue.set(state, 'checkedLinks', clone);
          return;
        }
      }
    },
    SET_SELECTED_INTEGRATIONS (state, data) {
      Vue.set(state, 'selectedIntegrations', data);
    },
    SET_SIDEBAR_KEEP_OPEN (state, data) {
      state.sidebarKeepOpen = data;
    }
  },
  getters: {
    getUser (state) {
      return state.user;
    },
    getRoles (state) {
      return state.roles;
    },
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
    getDoableIntegrations (state) {
      const doableIntegrations = {};
      for (const key in state.integrations) {
        if (state.integrations[key].doable) {
          doableIntegrations[key] = state.integrations[key];
        }
      }
      return doableIntegrations;
    },
    getSortedIntegrations (state, getters) {
      const integrations = [];
      for (const integration in getters.getDoableIntegrations) {
        integrations.push({ ...state.integrations[integration], key: integration });
      }
      integrations.sort((a, b) => { return a.key.localeCompare(b.key); });
      return integrations;
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
    },
    getCheckedLinks (state) {
      return state.checkedLinks;
    },
    getSidebarKeepOpen (state) {
      return state.sidebarKeepOpen;
    }
  },
  plugins: [createPersistedState({
    paths: [ // only these state variables are persisted to localstorage
      'checkedLinks', 'selectedIntegrations', 'sidebarKeepOpen'
    ]
  })]
});

export default store;
