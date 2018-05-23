import Vue from 'vue';
import Router from 'vue-router';
import store from '@/store';
import Stats from '@/components/stats/Stats';
import Help from '@/components/help/Help';
import Files from '@/components/files/Files';
import Users from '@/components/users/Users';
import History from '@/components/history/History';
import Sessions from '@/components/sessions/Sessions';
import Spiview from '@/components/spiview/Spiview';
import Spigraph from '@/components/spigraph/Spigraph';
import Connections from '@/components/connections/Connections';

Vue.use(Router);

/* eslint-disable no-undef */
const router = new Router({
  mode: 'history',
  base: MOLOCH_PATH,
  scrollBehavior: function (to, from, savedPosition) {
    if (to.hash) {
      let yoffset = 150;

      if (to.path === '/help') {
        yoffset = 50;
      }

      return {
        selector: to.hash,
        offset: { x: 0, y: yoffset }
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
      path: '/history',
      name: 'History',
      component: History
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
    }
  ]
});

router.beforeEach((to, from, next) => {
  // TODO update/remove as angular pages go away
  // loads the angular app pages
  if (to.path.includes('/settings') || to.path.includes('/upload')) {
    location.reload();
  }

  // always use the expression in the url query parameter if the navigation
  // was initiated from anything not in the moloch UI (browser forward/back btns)
  if (!to.params.nav && store.state.expression !== to.query.expression) {
    store.commit('setExpression', to.query.expression);
  }

  let page = to.name || 'Moloch - ';
  let view = to.query.view ? ` - ${to.query.view}` : '';
  let expression = to.query.expression ? ` - ${to.query.expression}` : '';

  /* eslint-disable no-undef */
  let title = MOLOCH_TITLE_CONFIG.replace(/_page_/g, page)
    .replace(/( *_-expression|_expression)_/g, expression)
    .replace(/( *_-view|_view)_/g, view);

  document.title = title;

  next();
});

export default router;
