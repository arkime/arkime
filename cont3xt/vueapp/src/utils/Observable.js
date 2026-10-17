/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
class C3Observable {
  constructor (func) {
    this.obsFunc = func;
  }

  subscribe (observer) {
    return this.obsFunc(observer);
  }
}

export default C3Observable;
