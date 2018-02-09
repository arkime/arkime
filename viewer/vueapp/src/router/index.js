import Vue from 'vue';
import Router from 'vue-router';
import Stats from '@/components/stats/Stats';

Vue.use(Router);

export default new Router({
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
