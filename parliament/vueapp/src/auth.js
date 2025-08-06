/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import { fetchWrapper } from '@/fetchWrapper.js';
import store from '@/store';

export default {
  getAuthInfo: async function () {
    try {
      const response = await fetchWrapper({ url: 'api/auth' });
      store.commit('setIsUser', response.data.isUser);
      store.commit('setIsAdmin', response.data.isAdmin);
    } catch (error) {
      store.commit('setIsUser', false);
      store.commit('setIsAdmin', false);
    }
  }
};
