export default {
  state: {
    expression: ''
  },
  setExpression (newValue) {
    this.state.message = newValue;
  },
  clearExpression () {
    this.state.message = '';
  }
};
