import Vue from 'vue';
import store from '@/store';

export default {
  getAuthInfo: function () {
    Vue.axios.get('api/auth').then((response) => {
      store.commit('setIsUser', response.data.isUser);
      store.commit('setIsAdmin', response.data.isAdmin);
    }).catch((error) => {
      store.commit('setIsUser', false);
      store.commit('setIsAdmin', false);
      store.commit('setHasAuth', false);
    });
  }
};
