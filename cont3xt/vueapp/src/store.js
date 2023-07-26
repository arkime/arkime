import Vue from 'vue';
import Vuex from 'vuex';
import createPersistedState from 'vuex-persistedstate';
import { iTypes, iTypeIndexMap } from '@/utils/iTypes';
import { indicatorId } from '@/utils/cont3xtUtil';

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
    linkGroups: undefined,
    linkGroupsError: '',
    collapsedLinkGroups: {},
    checkedLinks: {},
    selectedIntegrations: undefined,
    sidebarKeepOpen: false,
    views: [],
    integrationsPanelHoverDelay: 400,
    selectedView: undefined,
    shiftKeyHold: false,
    focusSearch: true,
    issueSearch: false,
    focusStartDate: false,
    focusLinkSearch: false,
    focusViewSearch: false,
    focusTagInput: false,
    toggleCache: false,
    downloadReport: false,
    copyShareLink: false,
    immediateSubmissionReady: false,
    theme: undefined,
    tags: [],
    tagDisplayCollapsed: true,
    seeAllViews: false,
    seeAllLinkGroups: false,
    seeAllOverviews: false,
    overviews: undefined,
    overviewsError: '',
    selectedOverviewIdMap: {},
    queuedIntegration: undefined,
    activeIndicator: undefined,
    activeSource: undefined,
    /** @type {{ [itype: string]: { [query: string]: { [integrationName: string]: object } } }} */
    results: {}, // results[<itype>][<query>][<integration_name>] yields the data for an integration
    /** @type {{ [indicatorId: string]: Cont3xtIndicatorNode }} */
    indicatorGraph: {}, // maps every `${query}-${itype}` to its corresponding indicator node
    /** @type {{ [indicatorId: string]: object }} */
    enhanceInfoTable: {}, // maps every `${query}-${itype}` to any enhancement info it may have
    linkGroupsPanelOpen: true
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
    SET_LINK_GROUPS (state, data) {
      state.linkGroups = data;
    },
    SET_LINK_GROUPS_ERROR (state, data) {
      state.linkGroupsError = data;
    },
    REMOVE_LINK_GROUP (state, id) {
      const index = state.linkGroups.findIndex(linkGroup => linkGroup._id === id);
      if (index !== -1) {
        state.linkGroups.splice(index, 1);
      }
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
    },
    SET_COLLAPSED_LINK_GROUPS (state, data) {
      state.collapsedLinkGroups = data;
    },
    SET_VIEWS (state, data) {
      state.views = data;
    },
    SET_INTEGRATIONS_PANEL_DELAY (state, data) {
      state.integrationsPanelHoverDelay = data;
    },
    SET_SELECTED_VIEW (state, data) {
      state.selectedView = data;
    },
    SET_SHIFT_HOLD (state, data) {
      state.shiftKeyHold = data;
    },
    SET_FOCUS_SEARCH (state, value) {
      state.focusSearch = value;
      setTimeout(() => { state.focusSearch = false; });
    },
    SET_ISSUE_SEARCH (state, value) {
      state.issueSearch = value;
      setTimeout(() => { state.issueSearch = false; });
    },
    SET_FOCUS_START_DATE (state, value) {
      state.focusStartDate = value;
      setTimeout(() => { state.focusStartDate = false; });
    },
    SET_FOCUS_LINK_SEARCH (state, value) {
      state.focusLinkSearch = value;
      setTimeout(() => { state.focusLinkSearch = false; });
    },
    SET_FOCUS_VIEW_SEARCH (state, value) {
      state.focusViewSearch = value;
      setTimeout(() => { state.focusViewSearch = false; });
    },
    SET_FOCUS_TAG_INPUT (state, value) {
      state.focusTagInput = value;
      setTimeout(() => { state.focusTagInput = false; });
    },
    SET_TOGGLE_CACHE (state, value) {
      state.toggleCache = value;
      setTimeout(() => { state.toggleCache = false; });
    },
    SET_DOWNLOAD_REPORT (state, value) {
      state.downloadReport = value;
      setTimeout(() => { state.downloadReport = false; });
    },
    SET_COPY_SHARE_LINK (state, value) {
      state.copyShareLink = value;
      setTimeout(() => { state.copyShareLink = false; });
    },
    SET_IMMEDIATE_SUBMISSION_READY (state, value) {
      state.immediateSubmissionReady = value;
    },
    SET_THEME (state, data) {
      state.theme = data;
    },
    SET_TAGS (state, data) {
      state.tags = data;
    },
    SET_TAG_DISPLAY_COLLAPSED (state, data) {
      state.tagDisplayCollapsed = data;
    },
    SET_SEE_ALL_VIEWS (state, data) {
      state.seeAllViews = data;
    },
    SET_SEE_ALL_LINK_GROUPS (state, data) {
      state.seeAllLinkGroups = data;
    },
    SET_SEE_ALL_OVERVIEWS (state, data) {
      state.seeAllOverviews = data;
    },
    SET_OVERVIEWS (state, data) {
      state.overviews = data;
    },
    SET_OVERVIEWS_ERROR (state, data) {
      state.overviewsError = data;
    },
    SET_SELECTED_OVERVIEW_ID_MAP (state, data) {
      state.selectedOverviewIdMap = data;
    },
    SET_SELECTED_OVERVIEW_ID_FOR_ITYPE (state, { iType, id }) {
      Vue.set(state.selectedOverviewIdMap, iType, id);
    },
    REMOVE_OVERVIEW (state, id) {
      const index = state.overviews.findIndex(overview => overview._id === id);
      if (index !== -1) {
        state.overviews.splice(index, 1);
      }
    },
    UPDATE_OVERVIEW (state, data) {
      const index = state.overviews.findIndex(overview => overview._id === data._id);
      if (index !== -1) {
        Vue.set(state.overviews, index, data);
      }
    },
    SET_INTEGRATION_RESULT (state, { indicator, source, result }) {
      const { itype, query } = indicator;

      if (!state.results[itype]) {
        Vue.set(state.results, itype, {});
      }
      if (!state.results[itype][query]) {
        Vue.set(state.results[itype], query, {});
      }

      Vue.set(state.results[itype][query], source, Object.freeze(result));
    },
    SET_ACTIVE_INDICATOR (state, data) {
      state.activeIndicator = data;
    },
    SET_QUEUED_INTEGRATION (state, data) {
      state.queuedIntegration = data;
    },
    SET_ACTIVE_SOURCE (state, data) {
      state.activeSource = data;
    },
    ADD_ENHANCE_INFO (state, { indicator, enhanceInfo }) {
      const id = indicatorId(indicator);

      if (!state.enhanceInfoTable[id]) {
        Vue.set(state.enhanceInfoTable, id, {});
      }
      for (const key in enhanceInfo) {
        Vue.set(state.enhanceInfoTable[id], key, enhanceInfo[key]);
      }
    },
    UPDATE_INDICATOR_GRAPH (state, { indicator, parentIndicator }) {
      const id = indicatorId(indicator);

      // a parentId of undefined means that the indicator is root-level
      const parentId = parentIndicator ? indicatorId(parentIndicator) : undefined;
      if (!state.indicatorGraph[id]) {
        if (!state.enhanceInfoTable[id]) {
          Vue.set(state.enhanceInfoTable, id, {});
        }

        const indicatorNode = {
          indicator,
          parentIds: new Set([parentId]),
          // handle case where child(ren) exist before parent
          children: Object.values(state.indicatorGraph).filter(node => node.parentIds.has(id)),
          enhanceInfo: state.enhanceInfoTable[id]
        };
        Vue.set(state.indicatorGraph, id, indicatorNode);
      } else {
        state.indicatorGraph[id].parentIds.add(parentId);
      }

      // handle case where parent already exists in the tree
      if (parentId && state.indicatorGraph[parentId]) {
        const alreadyAChild = state.indicatorGraph[parentId].children.some(child => indicatorId(child.indicator) === id);
        if (!alreadyAChild) {
          state.indicatorGraph[parentId].children.push(state.indicatorGraph[id]);
        }
      }
    },
    CLEAR_CONT3XT_RESULTS (state) {
      state.results = {};
      state.indicatorGraph = {};
      state.enhanceInfoTable = {};
    },
    TOGGLE_LINK_GROUPS_PANEL (state) {
      state.linkGroupsPanelOpen = !state.linkGroupsPanelOpen;
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
      return state.waitRendering;
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
    getSelectedIntegrations (state) {
      return state.selectedIntegrations;
    },
    getIntegrationsError (state) {
      return state.integrationsError;
    },
    getIntegrationsArray (state) {
      return state.integrationsArray;
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
    },
    getViews (state) {
      return state.views;
    },
    getSelectedView (state) {
      return state.selectedView;
    },
    getShiftKeyHold (state) {
      return state.shiftKeyHold;
    },
    getFocusSearch (state) {
      return state.focusSearch;
    },
    getIssueSearch (state) {
      return state.issueSearch;
    },
    getFocusStartDate (state) {
      return state.focusStartDate;
    },
    getFocusLinkSearch (state) {
      return state.focusLinkSearch;
    },
    getFocusViewSearch (state) {
      return state.focusViewSearch;
    },
    getFocusTagInput (state) {
      return state.focusTagInput;
    },
    getToggleCache (state) {
      return state.toggleCache;
    },
    getDownloadReport (state) {
      return state.downloadReport;
    },
    getCopyShareLink (state) {
      return state.copyShareLink;
    },
    getImmediateSubmissionReady (state) {
      return state.immediateSubmissionReady;
    },
    getAllViews (state, getters) {
      const makeSystemDefault = (viewName, integrationList, _id) => {
        return {
          creator: 'THE_SYSTEM',
          name: viewName,
          integrations: integrationList,
          viewRoles: [],
          editRoles: [],
          _id,
          _editable: false,
          _viewable: false,
          _systemDefault: true
        };
      };
      const systemDefaultViews = [
        makeSystemDefault('All', Object.keys(getters.getDoableIntegrations), 'all'),
        makeSystemDefault('None', [], 'none')
      ];
      return [...systemDefaultViews, ...state.views];
    },
    getTheme (state) {
      return state.theme;
    },
    getDarkThemeEnabled (state) {
      return state.theme === 'dark';
    },
    getTags (state) {
      return state.tags;
    },
    getTagDisplayCollapsed (state) {
      return state.tagDisplayCollapsed;
    },
    getSeeAllViews (state) {
      return state.seeAllViews;
    },
    getSeeAllLinkGroups (state) {
      return state.seeAllLinkGroups;
    },
    getSeeAllOverviews (state) {
      return state.seeAllOverviews;
    },
    getOverviews (state) {
      return state.overviews;
    },
    getOverviewMap (state) { // { [overviewId]: overview }
      return Object.fromEntries(
        (state.overviews ?? []).map(overview => [overview._id, overview])
      );
    },
    getSortedOverviews (state) {
      const overviews = [...(state.overviews ?? [])];
      overviews.sort((a, b) => {
        const iTypeDiff = iTypeIndexMap[a.iType] - iTypeIndexMap[b.iType];
        return (iTypeDiff === 0) ? a.name.localeCompare(b.name) : iTypeDiff;
      });
      return overviews;
    },
    getOverviewsError (state) {
      return state.overviewsError;
    },
    getSelectedOverviewIdMap (state) {
      return state.selectedOverviewIdMap;
    },
    getCorrectedSelectedOverviewIdMap (state, getters) {
      return Object.fromEntries(
        iTypes.map(iType => {
          let selectedId = state.selectedOverviewIdMap[iType];
          // fallback to default overview (id == iType) for undefined or incorrectly-iTyped
          if (selectedId == null || getters.getOverviewMap[selectedId]?.iType !== iType) {
            selectedId = iType;
          }
          return [iType, selectedId];
        })
      );
    },
    getSelectedOverviewMap (state, getters) {
      return Object.fromEntries(
        Object.entries(getters.getCorrectedSelectedOverviewIdMap).map(([iType, id]) => [
          iType, getters.getOverviewMap[id]
        ])
      );
    },
    getResults (state) {
      return state.results;
    },
    getActiveIndicator (state) {
      return state.activeIndicator;
    },
    getQueuedIntegration (state) {
      return state.queuedIntegration;
    },
    getActiveSource (state) {
      return state.activeSource;
    },
    getIndicatorGraph (state) {
      return state.indicatorGraph;
    },
    getLinkGroupsPanelOpen (state) {
      return state.linkGroupsPanelOpen;
    }
  },
  plugins: [createPersistedState({
    paths: [ // only these state variables are persisted to localstorage
      'checkedLinks', 'selectedIntegrations', 'sidebarKeepOpen',
      'collapsedLinkGroups', 'integrationsPanelHoverDelay', 'theme'
    ]
  })]
});

export default store;
