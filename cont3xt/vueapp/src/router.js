import { createRouter, createWebHistory } from 'vue-router';

import Cont3xt from '@/components/pages/Cont3xt.vue';
import Cont3xtStats from '@/components/pages/Stats.vue';
import Cont3xtSettings from '@/components/pages/Settings.vue';
import Cont3xtHelp from '@/components/pages/Help.vue';
import Cont3xt404 from '@/components/pages/404.vue';
import Users from '@/components/pages/Users.vue';
import AuditHistory from '@/components/pages/AuditHistory.vue';
import Roles from '@/components/pages/Roles.vue';

export default createRouter({
  // WEB_PATH is a global injected into index.ejs.html, by cont3xt.js
  /* eslint-disable no-undef */
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
      path: '/:pathMatch(.*)*', // see: https://router.vuejs.org/guide/migration/#removed-star-or-catch-all-routes
      name: 'Not Found',
      component: Cont3xt404
    }
  ]
});
