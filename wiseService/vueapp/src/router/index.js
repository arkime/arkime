import Vue from 'vue';
import VueRouter from 'vue-router';
import Query from '../components/Query.vue';
import Config from '../components/Config.vue';
import Help from '../components/Help.vue';
import Stats from '../components/Stats.vue';

Vue.use(VueRouter);

const routes = [
  {
    path: '/',
    alias: '/query',
    name: 'Query',
    component: Query
  },
  {
    path: '/statistics',
    name: 'Stats',
    component: Stats
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
  }
];

const router = new VueRouter({
  mode: 'history',
  base: window.location.pathname.substring(0, window.location.pathname.lastIndexOf('/') + 1),
  routes
});

export default router;
