
import { createRouter, createWebHistory } from 'vue-router';

import store from '@/store';
import Stats from '@/components/stats/Stats.vue';
import Help from '@/components/help/Help.vue';
import Files from '@/components/files/Files.vue';
import Users from '@/components/users/Users.vue';
import Roles from '@/components/roles/Roles.vue';
import ArkimeHistory from '@/components/history/History.vue';
import Sessions from '@/components/sessions/Sessions.vue';
import Spiview from '@/components/spiview/Spiview.vue';
import Spigraph from '@/components/spigraph/Spigraph.vue';
import Connections from '@/components/connections/Connections.vue';
import Settings from '@/components/settings/Settings.vue';
import Upload from '@/components/upload/Upload.vue';
import Hunt from '@/components/hunt/Hunt.vue';
import Arkime404 from '@/components/utils/404.vue';

const router = createRouter({
  // PATH is a global injected into index.ejs.html, by viewer.js
  /* eslint-disable no-undef */
  history: createWebHistory(PATH),
  scrollBehavior: function (to, from, savedPosition) {
    if (to.hash) {
      let yoffset = 150;

      if (to.path === '/help') {
        yoffset = 50;
      }

      return {
        el: to.hash,
        top: yoffset,
        behavior: 'smooth'
      };
    }
  },
  routes: [
    {
      path: '/stats',
      name: 'Stats',
      component: Stats
    },
    {
      path: '/sessions',
      name: 'Sessions',
      alias: '/',
      component: Sessions
    },
    {
      path: '/help',
      name: 'Help',
      component: Help
    },
    {
      path: '/files',
      name: 'Files',
      component: Files
    },
    {
      path: '/users',
      name: 'Users',
      component: Users
    },
    {
      path: '/roles',
      name: 'Roles',
      component: Roles
    },
    {
      path: '/history',
      name: 'ArkimeHistory',
      component: ArkimeHistory
    },
    {
      path: '/spiview',
      name: 'Spiview',
      component: Spiview
    },
    {
      path: '/spigraph',
      name: 'Spigraph',
      component: Spigraph
    },
    {
      path: '/connections',
      name: 'Connections',
      component: Connections
    },
    {
      path: '/settings',
      name: 'Settings',
      component: Settings
    },
    {
      path: '/upload',
      name: 'Upload',
      component: Upload
    },
    {
      path: '/hunt',
      name: 'Hunt',
      component: Hunt
    },
    {
      path: '/:pathMatch(.*)*', // see: https://router.vuejs.org/guide/migration/#removed-star-or-catch-all-routes
      name: 'Not Found',
      component: Arkime404
    }
  ]
});

router.beforeEach((to, from, next) => {
  // always use the expression in the url query parameter if the navigation
  // was initiated from anything not in the arkime UI (browser forward/back btns)
  if (!to.params.nav && store.state.expression !== to.query.expression) {
    store.commit('setExpression', to.query.expression);
  }

  const page = to.name || 'Arkime - ';
  const view = to.query.view ? ` - ${to.query.view}` : '';
  const expression = to.query.expression ? ` - ${to.query.expression}` : '';

  /* eslint-disable no-undef */
  const title = TITLE_CONFIG.replace(/_page_/g, page)
    .replace(/( *_-expression|_expression)_/g, expression)
    .replace(/( *_-view|_view)_/g, view);

  document.title = title;

  next();
});

export default router;
