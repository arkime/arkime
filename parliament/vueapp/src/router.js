/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import { createRouter, createWebHistory } from 'vue-router';

import Parliament from '@/components/Parliament.vue';
import Issues from '@/components/Issues.vue';
import Settings from '@/components/Settings.vue';
import Parliament404 from '@/components/404.vue';
import Help from '@/components/Help.vue';
import Users from '@/components/Users.vue';
import AuthService from '@/auth.js';

const router = createRouter({
  history: createWebHistory('/parliament/'),
  base: '/parliament/',
  scrollBehavior: function (to, from, savedPosition) {
    if (to.hash) {
      return {
        selector: to.hash,
        offset: { x: 0, y: 60 }
      };
    }
  },
  routes: [
    {
      path: '',
      alias: '/',
      name: 'Parliament',
      component: Parliament
    },
    {
      path: '/issues',
      name: 'Issues',
      component: Issues
    },
    {
      path: '/settings',
      name: 'Settings',
      component: Settings
    },
    {
      path: '/help',
      name: 'Help',
      component: Help
    },
    {
      path: '/users',
      name: 'Users',
      component: Users
    },
    {
      path: '/:pathMatch(.*)*', // see: https://router.vuejs.org/guide/migration/#removed-star-or-catch-all-routes
      name: 'Not Found',
      component: Parliament404
    }
  ]
});

router.beforeEach((to, from, next) => {
  AuthService.getAuthInfo();
  next();
});

export default router;
