import Vue from 'vue';
import Vuex from 'vuex';

Vue.use(Vuex);

// TODO add: view, bounding, interval

const store = new Vuex.Store({
  state: {
    timeRange: 1,
    expression: undefined,
    time: {
      startTime: undefined,
      stopTime: undefined
    }
  },
  mutations: {
    setTimeRange (state, value) {
      state.timeRange = value.toString();
    },
    setTime (state, value) {
      if (value.startTime) {
        state.time.startTime = value.startTime.toString();
      }
      if (value.stopTime) {
        state.time.stopTime = value.stopTime.toString();
      }
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
