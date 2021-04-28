import Vue from 'vue';
import Vuex from 'vuex';

Vue.use(Vuex);

export default new Vuex.Store({
  state: {
    theme: 'light'
  },
  mutations: {
    SET_THEME (state, newTheme) {
      state.theme = newTheme;
    }
  },
  getters: {
    getTheme: (state) => {
      return state.theme;
    }
  }
});
