/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import { createRouter, createWebHistory } from 'vue-router';

import Cont3xt from '@/components/pages/Cont3xt.vue';
import Cont3xtStats from '@/components/pages/Stats.vue';
import Cont3xtSettings from '@/components/pages/Settings.vue';
import Cont3xtHelp from '@/components/pages/Help.vue';
import Cont3xt404 from '@/components/pages/404.vue';
import Users from '@/components/pages/Users.vue';
import AuditHistory from '@/components/pages/AuditHistory.vue';
import Roles from '@/components/pages/Roles.vue';
import Banner from '@common/BannerPage.vue';
import store from '@/store';
import UserService from '@/components/services/UserService';

// Admin pages are hidden from the navbar when the user lacks the role;
// guard the routes too so they can't be reached by typing the url directly.
// On a hard load the user isn't fetched yet, so pull it first.
async function requireRole (role) {
  let user = store.getters.getUser;
  if (!user) {
    try { user = await UserService.getUser(); } catch { /* treated as no access */ }
  }
  if (!user?.roles?.includes(role)) { return { name: 'Cont3xt' }; }
}

export default createRouter({
  // WEB_PATH is a global injected into index.ejs.html, by cont3xt.js
  // eslint-disable-next-line no-undef
  history: createWebHistory(WEB_PATH),
  routes: [
    {
      path: '/',
      name: 'Cont3xt',
      component: Cont3xt
    },
    {
      path: '/stats',
      name: 'Stats',
      component: Cont3xtStats
    },
    {
      path: '/settings',
      name: 'Settings',
      component: Cont3xtSettings
    },
    {
      path: '/users',
      name: 'Users',
      component: Users
    },
    {
      path: '/help',
      name: 'Help',
      component: Cont3xtHelp
    },
    {
      path: '/history',
      name: 'History',
      component: AuditHistory
    },
    {
      path: '/roles',
      name: 'Roles',
      component: Roles
    },
    {
      path: '/banner',
      name: 'Banner',
      component: Banner,
      beforeEnter: async () => await requireRole('cont3xtAdmin')
    },
    {
      path: '/:pathMatch(.*)*', // see: https://router.vuejs.org/guide/migration/#removed-star-or-catch-all-routes
      name: 'Not Found',
      component: Cont3xt404
    }
  ]
});
