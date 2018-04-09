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
    addToExpression (state, value) {
      let newExpr = '';

      if (!this.state.expression) { this.state.expression = ''; }

      if (this.state.expression && this.state.expression !== '') {
        if (this.state.expression[this.state.expression.length - 1] !== ' ') {
          // if last char is not a space, add it
          newExpr += ' ';
        }
        newExpr += (value.op || '&&') + ' ';
      }

      newExpr += value.expression;

      this.state.expression += newExpr;
    },
    clearExpression (state) {
      this.state.expression = undefined;
    }
  }
});

export default store;
