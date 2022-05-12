import Vue from 'vue';
import Vuex from 'vuex';

import Utils from './components/utils/utils';

Vue.use(Vuex);

const store = new Vuex.Store({
  state: {
    esHealth: undefined,
    esHealthError: undefined,
    user: undefined,
    views: undefined,
    remoteclusters: undefined,
    esCluster: {
      availableCluster: {
        active: [],
        inactive: []
      },
      selectedCluster: []
    },
    fieldsArr: [],
    fieldsMap: {}, // NOTE: this has duplicate fields where dbField and dbField2 are different
    fieldsAliasMap: {},
    fieldhistory: [],
    timeRange: 1,
    expression: undefined,
    time: {
      startTime: undefined,
      stopTime: undefined
    },
    hideViz: false,
    stickyViz: false,
    fetchGraphData: false,
    disabledAggregations: false,
    showMaps: true,
    showToolBars: true,
    mapSrc: true,
    mapDst: true,
    xffGeo: false,
    graphType: undefined,
    seriesType: undefined,
    focusSearch: undefined,
    issueSearch: undefined,
    focusTimeRange: undefined,
    shiftKeyHold: false,
    responseTime: undefined,
    sessionsTableState: undefined,
    loadingData: false,
    sorts: [['firstPacket', 'desc']],
    sortsParam: 'firstPacket:desc',
    stickySessionsBtn: false,
    showCapStartTimes: true,
    capStartTimes: [{ nodeName: 'none', startTime: 1 }],
    roles: []
  },
  getters: {
    sessionsTableState (state) {
      if (!state.sessionsTableState.order) {
        state.sessionsTableState = Utils.getDefaultTableState();
      }
      return state.sessionsTableState;
    }
  },
  mutations: {
    setTimeRange (state, value) {
      state.timeRange = value.toString();
    },
    setTime (state, value) {
      if (value.startTime !== undefined) {
        state.time.startTime = value.startTime.toString();
      }
      if (value.stopTime !== undefined) {
        state.time.stopTime = value.stopTime.toString();
      }
    },
    setExpression (state, value) {
      state.expression = value;
    },
    addToExpression (state, value) {
      let newExpr = '';

      if (!state.expression) { state.expression = ''; }

      if (state.expression && state.expression !== '') {
        if (state.expression[state.expression.length - 1] !== ' ') {
          // if last char is not a space, add it
          newExpr += ' ';
        }
        newExpr += (value.op || '&&') + ' ';
      }

      newExpr += value.expression;

      state.expression += newExpr;
    },
    clearExpression (state) {
      state.expression = undefined;
    },
    toggleStickyViz (state, value) {
      state.stickyViz = value;
    },
    toggleHideViz (state, value) {
      state.hideViz = value;
    },
    setFetchGraphData (state, value) {
      state.fetchGraphData = value;
    },
    toggleMaps (state, value) {
      state.showMaps = value;
    },
    toggleToolBars (state) {
      state.showToolBars = !state.showToolBars;
    },
    toggleMapSrc (state, value) {
      state.mapSrc = value;
    },
    toggleMapDst (state, value) {
      state.mapDst = value;
    },
    toggleMapXffGeo (state, value) {
      state.xffGeo = value;
    },
    updateGraphType (state, value) {
      state.graphType = value;
    },
    updateSeriesType (state, value) {
      state.seriesType = value;
    },
    setFocusSearch (state, value) {
      state.focusSearch = value;
      setTimeout(() => { state.focusSearch = false; });
    },
    setIssueSearch (state, value) {
      state.issueSearch = value;
      setTimeout(() => { state.issueSearch = false; });
    },
    setFocusTimeRange (state, value) {
      state.focusTimeRange = value;
      setTimeout(() => { state.focusTimeRange = false; });
    },
    setShiftKeyHold (state, value) {
      state.shiftKeyHold = value;
    },
    setUser (state, value) {
      state.user = value;
    },
    setUserSettings (state, value) {
      state.user.settings = value;
    },
    setResponseTime (state, value) {
      state.responseTime = value;
    },
    setSessionsTableState (state, value) {
      state.sessionsTableState = value;
    },
    setViews (state, value) {
      state.views = value;
    },
    addViews (state, value) {
      state.views[value.name] = value;
    },
    deleteViews (state, value) {
      state.views[value] = null;
      delete state.views[value];
    },
    updateViews (state, value) {
      // if name of view changes in update
      if (value.name !== value.key) {
        state.views[value.key] = null;
        delete state.views[value.key];
      }
      delete value.key;

      state.views[value.name] = value;
    },
    setLoadingData (state, value) {
      state.loadingData = value;
    },
    setSorts (state, value) {
      state.sorts = value;

      state.sortsParam = '';

      if (!Array.isArray(value)) {
        state.sortsParam = value;
        return;
      }

      // combine sort field and sort order into one string
      // because server takes one param
      for (let i = 0, len = value.length; i < len; ++i) {
        const item = value[i];
        state.sortsParam += item[0] + ':' + item[1];
        if (i < len - 1) { state.sortsParam += ','; }
      }
    },
    setAvailableCluster (state, value) {
      state.esCluster.availableCluster = value;
    },
    setSelectedCluster (state, value) {
      state.esCluster.selectedCluster = value;
    },
    setStickySessionsBtn (state, value) {
      state.stickySessionsBtn = value;
    },
    setShowCapStartTimes (state, value) {
      state.showCapStartTimes = value;
    },
    setCapStartTimes (state, value) {
      state.capStartTimes = value;
    },
    setESHealth (state, value) {
      state.esHealth = value;
    },
    setESHealthError (state, value) {
      state.esHealthError = value;
    },
    setAppInfo (state, value) {
      state.esHealth = value.esHealth;
      state.esHealthError = value.esHealthError;
      state.user = value.user;
      state.views = value.views;
      state.fieldsArr = value.fieldsArr;
      state.remoteclusters = value.remoteclusters;
      state.fieldhistory = value.fieldhistory.fields || [];
      state.esCluster.availableCluster = value.clusters;
      state.roles = Vue.filter('parseRoles')(value.roles);

      // fieldsMap has keys for these fields: dbField, dbField2, fieldECS, and exp (id/key)
      // fieldsAliasMap has keys for field aliases
      for (const key in value.fieldsMap) {
        const field = value.fieldsMap[key];
        state.fieldsMap[field.exp] = field;
        state.fieldsMap[field.dbField] = field;
        if (field.dbField2 !== undefined) {
          state.fieldsMap[field.dbField2] = field;
        }
        if (field.fieldECS !== undefined) {
          state.fieldsMap[field.fieldECS] = field;
        }
        (field.aliases || []).forEach((alias) => {
          state.fieldsAliasMap[alias] = field;
        });
      }
    },
    setRoles (state, value) {
      state.roles = value;
    },
    setDisabledAggregations (state, value) {
      state.disabledAggregations = value;
    }
  }
});

export default store;
