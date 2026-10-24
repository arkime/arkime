/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import { fetchWrapper } from '@common/fetchWrapper.js';
import store from '@/store';

// Concurrent callers (the global beforeEach and a route's beforeEnter guard
// firing for the same navigation) share one in-flight request instead of
// each issuing their own GET api/auth.
let pending = null;

export default {
  getAuthInfo: async function () {
    if (pending) { return pending; }
    pending = (async () => {
      try {
        const response = await fetchWrapper({ url: 'api/auth' });
        store.commit('setIsUser', response.isUser);
        store.commit('setIsAdmin', response.isAdmin);
      } catch (error) {
        store.commit('setIsUser', false);
        store.commit('setIsAdmin', false);
      } finally {
        pending = null;
      }
    })();
    return pending;
  }
};
