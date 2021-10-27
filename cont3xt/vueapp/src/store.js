import Vue from 'vue';
import Vuex from 'vuex';

Vue.use(Vuex);

const store = new Vuex.Store({
  state: {
    loading: false // TODO should be a number?
  },
  mutations: {
    SET_LOADING (state, data) {
      state.loading = data;
    }
  },
  getters: {
    getLoading (state) {
      return state.loading;
    }
  }
});

export default store;
