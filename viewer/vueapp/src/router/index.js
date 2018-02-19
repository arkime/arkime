import Vue from 'vue';
import Router from 'vue-router';
import Stats from '@/components/stats/Stats';

Vue.use(Router);

const router = new Router({
  mode: 'history',
  base: window.location.pathname,
  routes: [
    {
      path: '/stats',
      alias: '/',
      name: 'Stats',
      component: Stats
    }
  ]
});

router.beforeEach((to, from, next) => {
  let page = to.name || 'Moloch Vue App'; // TODO different fallback?
  let expression = ''; // TODO set expression
  let view = ''; // TODO set view

  /* eslint-disable no-undef */
  let title = MOLOCH_TITLE_CONFIG.replace(/_page_/g, page)
    .replace(/( *_-expression|_expression)_/g, expression)
    .replace(/( *_-view|_view)_/g, view);

  document.title = title;

  next(); // complete route change
});

export default router;
