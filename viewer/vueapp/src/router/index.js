import Vue from 'vue';
import Router from 'vue-router';
import Stats from '@/components/stats/Stats';
import Sessions from '@/components/sessions/Sessions';

Vue.use(Router);

/* eslint-disable no-undef */
const router = new Router({
  mode: 'history',
  base: MOLOCH_PATH,
  routes: [
    {
      path: '/stats',
      name: 'Stats',
      component: Stats
    },
    {
      path: '/sessions2',
      name: 'Sessions',
      component: Sessions
    }
  ]
});

router.beforeEach((to, from, next) => {
  // TODO update/remove as angular pages go away
  // loads the angular app pages (for now, anything but the stats page)
  if (!to.path.includes('stats') && !to.path.includes('sessions2') && to.path !== '/') {
    location.reload();
  }

  let page = to.name || 'Moloch - ';
  let view = to.query.view ? ` - ${to.query.view}` : '';
  let expression = to.query.expression ? ` - ${to.query.expression}` : '';

  /* eslint-disable no-undef */
  let title = MOLOCH_TITLE_CONFIG.replace(/_page_/g, page)
    .replace(/( *_-expression|_expression)_/g, expression)
    .replace(/( *_-view|_view)_/g, view);

  document.title = title;

  next(); // complete route change
});

export default router;
