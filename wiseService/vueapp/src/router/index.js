import Vue from 'vue';
import VueRouter from 'vue-router';
import Query from '@/components/Query';
import Config from '@/components/Config';
import Help from '@/components/Help';
import Stats from '@/components/Stats';
import Wise404 from '@/components/404';

Vue.use(VueRouter);

const routes = [
  {
    path: '/',
    alias: '/statistics',
    name: 'Stats',
    component: Stats
  },
  {
    path: '/query',
    name: 'Query',
    component: Query
  },
  {
    path: '/config',
    name: 'Config',
    component: Config
  },
  {
    path: '/help',
    name: 'Help',
    component: Help
  },
  {
    path: '*',
    name: 'Not Found',
    component: Wise404
  }
];

const router = new VueRouter({
  mode: 'history',
  base: window.location.pathname.substring(0, window.location.pathname.lastIndexOf('/') + 1),
  scrollBehavior (to, from, savedPosition) {
    if (to.hash) {
      return new Promise((resolve, reject) => {
        setTimeout(() => {
          resolve({
            selector: to.hash,
            offset: { x: 0, y: 60 }
          });
        });
      });
    }
  },
  routes
});

export default router;
