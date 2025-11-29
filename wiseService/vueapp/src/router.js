/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import { createRouter, createWebHistory } from 'vue-router';

import Query from '@/components/Query.vue';
import Config from '@/components/Config.vue';
import Help from '@/components/Help.vue';
import Stats from '@/components/Stats.vue';
import Wise404 from '@/components/404.vue';

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
