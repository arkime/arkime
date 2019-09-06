import Vue from 'vue';
import Vuex from 'vuex';

import Utils from './components/utils/utils';

Vue.use(Vuex);

const store = new Vuex.Store({
  state: {
    timeRange: 1,
    expression: undefined,
    time: {
      startTime: undefined,
      stopTime: undefined
    },
    showMaps: true,
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
    user: undefined,
    responseTime: undefined,
    sessionsTableState: undefined,
    views: undefined,
    loadingData: false,
    sorts: [['firstPacket', 'desc']],
    sortsParam: 'firstPacket:desc'
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
    toggleMaps (state, value) {
      state.showMaps = value;
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
        let item = value[i];
        state.sortsParam += item[0] + ':' + item[1];
        if (i < len - 1) { state.sortsParam += ','; }
      }
    }
  }
});

export default store;
