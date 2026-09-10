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
import Roles from '@/components/Roles.vue';
import Banner from '@common/BannerPage.vue';
import ViewConfigPage from '@common/ViewConfigPage.vue';
import AuthService from '@/auth.js';
import UserService from '@/components/user.service.js';
import store from '@/store';
import { createRequireRole } from '@common/routeGuards.js';

// Admin pages are hidden from the navbar when the user isn't an admin;
// guard the routes too so they can't be reached by typing the url directly.
// The global beforeEach doesn't await getAuthInfo, so ask for it here.
async function requireAdmin () {
  await AuthService.getAuthInfo();
  if (!store.state.isAdmin) { return { name: 'Parliament' }; }
}

// The user pages answer to arkime-wide roles rather than parliamentAdmin.
// Always re-fetch rather than trusting a cached store.state.user -- these
// are admin-sensitive routes, so a role revoked mid-session shouldn't keep
// granting access off a stale value until a hard reload.
const requireUser = createRequireRole(UserService.getUser, () => store.state.user, 'Parliament', { alwaysRefetch: true });

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
      component: Users,
      beforeEnter: async () => await requireUser(u => u?.roles?.includes('usersAdmin'))
    },
    {
      path: '/roles',
      name: 'Roles',
      component: Roles,
      beforeEnter: async () => await requireUser(u => !!u?.canAssignRoles)
    },
    {
      path: '/banner',
      name: 'Banner',
      component: Banner,
      beforeEnter: async () => await requireAdmin()
    },
    {
      path: '/viewconfig',
      name: 'ViewConfig',
      component: ViewConfigPage,
      // access is viewConfigMode as well as a role, so the server decides
      beforeEnter: async () => await requireUser(u => !!u?.canViewConfig)
    },
    {
      path: '/:pathMatch(.*)*', // see: https://router.vuejs.org/guide/migration/#removed-star-or-catch-all-routes
      name: 'Not Found',
      component: Parliament404
    }
  ]
});

router.beforeEach((to, from) => {
  AuthService.getAuthInfo();
});

export default router;
