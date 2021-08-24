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
    remotecluster: undefined,
    esCluster: {
      availableCluster: {
        active: [],
        inactive: []
      },
      selectedCluster: []
    },
    fieldsArr: [],
    fieldsMap: {}, // NOTE: this has duplicate fields where dbField and dbField2 are different
    fieldhistory: [],
    timeRange: 1,
    expression: undefined,
    time: {
      startTime: undefined,
      stopTime: undefined
    },
    stickyViz: false,
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
    displayKeyboardShortcutsHelp: undefined,
    responseTime: undefined,
    sessionsTableState: undefined,
    loadingData: false,
    sorts: [['firstPacket', 'desc']],
    sortsParam: 'firstPacket:desc',
    stickySessionsBtn: false,
    showCapStartTimes: true,
    capStartTimes: [{ nodeName: 'none', startTime: 1 }]
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
    },
    setIssueSearch (state, value) {
      state.issueSearch = value;
      if (value) {
        setTimeout(() => {
          state.issueSearch = false;
        });
      }
    },
    setFocusTimeRange (state, value) {
      state.focusTimeRange = value;
    },
    setShiftKeyHold (state, value) {
      state.shiftKeyHold = value;
    },
    setDisplayKeyboardShortcutsHelp (state, value) {
      state.displayKeyboardShortcutsHelp = value;
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
      state.fieldsMap = value.fieldsMap;
      state.remoteclusters = value.remoteclusters;
      state.fieldhistory = value.fieldhistory.fields;
      state.esCluster.availableCluster = value.clusters;
    }
  }
});

export default store;
