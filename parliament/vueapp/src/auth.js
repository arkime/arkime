/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import { fetchWrapper } from '@common/fetchWrapper.js';
import store from '@/store';

export default {
  getAuthInfo: async function () {
    try {
      const response = await fetchWrapper({ url: 'api/auth' });
      store.commit('setIsUser', response.isUser);
      store.commit('setIsAdmin', response.isAdmin);
    } catch (error) {
      store.commit('setIsUser', false);
      store.commit('setIsAdmin', false);
    }
  }
};
