import Vue from 'vue';
import store from '@/store';

export default {
  getAuthInfo: function () {
    Vue.axios.get('api/auth').then((response) => {
      store.commit('setIsUser', response.data.isUser);
      store.commit('setIsAdmin', response.data.isAdmin);
      store.commit('setHasAuth', response.data.hasAuth);
    }).catch((error) => {
      store.commit('setIsUser', false);
      store.commit('setIsAdmin', false);
      store.commit('setHasAuth', false);
    });
  },

  updateCommonAuth: function (data) {
    return new Promise((resolve, reject) => {
      Vue.axios.put('api/auth/commonAuth', data).then((response) => {
        store.commit('setCommonAuth', true);
        return resolve(response.data);
      }).catch((error) => {
        return reject(error.response.data);
      });
    });
  }
};
