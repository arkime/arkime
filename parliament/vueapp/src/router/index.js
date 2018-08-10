import Vue from 'vue';
import Router from 'vue-router';
import Parliament from '@/components/Parliament';
import Issues from '@/components/Issues';
import Settings from '@/components/Settings';
import Parliament404 from '@/components/404';

Vue.use(Router);

export default new Router({
  mode: 'history',
  base: '/parliament/',
  routes: [
    {
      path: '',
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
      path: '*',
      name: 'Not Found',
      component: Parliament404
    }
  ]
});
