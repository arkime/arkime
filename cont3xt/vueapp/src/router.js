import Vue from 'vue';
import Router from 'vue-router';
import Cont3xt from '@/components/pages/Cont3xt';
import Cont3xtStats from '@/components/pages/Stats';
import Cont3xtSettings from '@/components/pages/Settings';
import Cont3xtHelp from '@/components/pages/Help';
import Cont3xt404 from '@/components/pages/404';

Vue.use(Router);

/* eslint-disable no-undef */
export default new Router({
  mode: 'history',
  base: WEB_PATH,
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
      path: '/help',
      name: 'Help',
      component: Cont3xtHelp
    },
    {
      path: '*',
      name: 'Not Found',
      component: Cont3xt404
    }
  ]
});
