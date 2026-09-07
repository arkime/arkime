/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import { createRouter, createWebHistory } from 'vue-router';

import Query from '@/components/Query.vue';
import Config from '@/components/Config.vue';
import Help from '@/components/Help.vue';
import Stats from '@/components/Stats.vue';
import Settings from '@/components/Settings.vue';
import Banner from '@common/BannerPage.vue';
import store from '@/store';
import WiseService from '@/components/wise.service';
import Wise404 from '@/components/404.vue';

// Admin pages are hidden from the navbar when the user lacks the role;
// guard the routes too so they can't be reached by typing the url directly.
// On a hard load the user isn't fetched yet, so pull it first.
async function requireRole (role) {
  if (!store.state.user) {
    try {
      const user = await WiseService.getCurrentUser();
      // api/user sits behind isWiseUser, which answers non-users with a
      // 200 { success: false } body instead of an error status
      if (user?.userId) { store.commit('SET_USER', user); }
    } catch { /* treated as no access */ }
  }
  if (!store.state.user?.roles?.includes(role)) { return { name: 'Stats' }; }
}

const router = createRouter({
  // PATH is a global injected into index.ejs.html, by wiseService.js
  /* eslint-disable no-undef */
  history: createWebHistory(PATH),
  scrollBehavior (to, from, savedPosition) {
    if (to.hash) {
      return new Promise((resolve, reject) => {
        setTimeout(() => {
          resolve({
            selector: to.hash,
            offset: { x: 0, y: 60 }
          });
        });
      });
    }
  },
  routes: [
    {
      path: '/',
      alias: '/statistics',
      name: 'Stats',
      component: Stats
    },
    {
      path: '/query',
      name: 'Query',
      component: Query
    },
    {
      path: '/config',
      name: 'Config',
      component: Config
    },
    {
      path: '/settings',
      name: 'Settings',
      component: Settings
    },
    {
      path: '/banner',
      name: 'Banner',
      component: Banner,
      beforeEnter: async () => await requireRole('wiseAdmin')
    },
    {
      path: '/help',
      name: 'Help',
      component: Help
    },
    {
      path: '/:pathMatch(.*)*', // see: https://router.vuejs.org/guide/migration/#removed-star-or-catch-all-routes
      name: 'Not Found',
      component: Wise404
    }
  ]
});

export default router;
