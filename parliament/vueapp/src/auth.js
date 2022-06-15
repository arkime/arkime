import Vue from 'vue';
import store from '@/store';

export default {
  login: function (password) {
    return new Promise((resolve, reject) => {
      Vue.axios.post('api/auth', { password }).then((response) => {
        this.saveToken(response.data.token);
        resolve(response.data);
      }).catch((error) => {
        this.saveToken('');
        reject(error.response.data);
      });
    });
  },

  logout: function () {
    Vue.axios.post('api/logout').then((response) => {
      this.saveToken('');
    }).catch((error) => {
      this.saveToken('');
    });
  },

  saveToken: function (token) {
    localStorage.setItem('token', token);
    store.commit('setLoggedIn', !!token);
  },

  getToken: function () {
    return localStorage.getItem('token') || '';
  },

  isLoggedIn: function () {
    Vue.axios.get('api/auth/loggedin').then((response) => {
      store.commit('setIsUser', response.data.isUser);
      store.commit('setIsAdmin', response.data.isAdmin);
      store.commit('setLoggedIn', response.data.loggedin);
      store.commit('setCommonAuth', response.data.commonAuth);
    }).catch((error) => {
      store.commit('setIsUser', false);
      store.commit('setIsAdmin', false);
      store.commit('setLoggedIn', false);
      store.commit('setCommonAuth', false);
    });
  },

  hasAuth: function () {
    Vue.axios.get('api/auth').then((response) => {
      store.commit('setHasAuth', response.data.hasAuth);
      store.commit('setDashboardOnly', response.data.dashboardOnly);
    }).catch((error) => {
      store.commit('setHasAuth', false);
      store.commit('setDashboardOnly', false);
    });
  },

  updatePassword: function (currentPassword, newPassword, authSetupCode) {
    return new Promise((resolve, reject) => {
      Vue.axios.put('api/auth/update', {
        newPassword,
        authSetupCode,
        currentPassword
      }).then((response) => {
        store.commit('setHasAuth', true);
        this.saveToken(response.data.token);
        resolve(response.data);
      }).catch((error) => {
        reject(error.response.data);
      });
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
