export default {
  state: {
    expression: undefined
  },
  setExpression (newValue) {
    this.state.expression = newValue;
  },
  clearExpression () {
    this.state.expression = undefined;
  }
};
