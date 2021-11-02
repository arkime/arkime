class Observable {
  constructor (func) {
    this.obsFunc = func;
  }

  subscribe (observer) {
    return this.obsFunc(observer);
  }
}

export default Observable;
