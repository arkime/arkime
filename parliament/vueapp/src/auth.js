import Vue from 'vue';
import store from '@/store';

export default {
  login: function (password) {
    return new Promise((resolve, reject) => {
      Vue.axios.post('api/auth', { password: password })
        .then((response) => {
          this.saveToken(response.data.token);
          resolve(response.data);
        })
        .catch((error) => {
          this.saveToken('');
          reject(error.response.data);
        });
    });
  },

  logout: function () {
    this.saveToken('');
  },

  saveToken: function (token) {
    localStorage.setItem('token', token);
    store.commit('setLoggedIn', !!token);
  },

  getToken: function () {
    return localStorage.getItem('token') || '';
  },

  isLoggedIn: function () {
    Vue.axios.get('api/auth/loggedin')
      .then((response) => {
        store.commit('setLoggedIn', response.data.loggedin);
      })
      .catch((error) => {
        store.commit('setLoggedIn', false);
      });
  },

  hasAuth: function () {
    Vue.axios.get('api/auth')
      .then((response) => {
        store.commit('setHasAuth', response.data.hasAuth);
        store.commit('setDashboardOnly', response.data.dashboardOnly);
      })
      .catch((error) => {
        store.commit('setHasAuth', false);
        store.commit('setDashboardOnly', false);
      });
  },

  updatePassword: function (currentPassword, newPassword) {
    return new Promise((resolve, reject) => {
      Vue.axios.put('api/auth/update', {
        newPassword: newPassword,
        currentPassword: currentPassword
      })
        .then((response) => {
          store.commit('setHasAuth', true);
          this.saveToken(response.data.token);
          resolve(response.data);
        })
        .catch((error) => {
          this.saveToken('');
          reject(error.response.data);
        });
    });
  }
};
