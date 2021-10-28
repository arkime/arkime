import Vue from 'vue';
import Router from 'vue-router';
import Cont3xt from '@/components/Cont3xt';
import Cont3xtSettings from '@/components/Settings';
import Cont3xtHelp from '@/components/Help';
import Cont3xt404 from '@/components/404';

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
