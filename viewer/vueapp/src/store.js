import Vue from 'vue';
import Vuex from 'vuex';

Vue.use(Vuex);

// TODO add: view, bounding, interval

const store = new Vuex.Store({
  state: {
    timeRange: 1,
    startTime: undefined,
    stopTime: undefined,
    expression: undefined
  },
  mutations: {
    setTimeRange (state, value) {
      state.timeRange = value.toString();
    },
    setStartTime (state, value) {
      state.startTime = value.toString();
    },
    setStopTime (state, value) {
      state.stopTime = value.toString();
    },
    setExpression (state, value) {
      this.state.expression = value;
    },
    clearExpression (state) {
      this.state.expression = undefined;
    }
  }
});

export default store;
