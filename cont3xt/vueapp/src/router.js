import Vue from 'vue';
import Router from 'vue-router';
import Cont3xt from '@/components/Cont3xt';
import Cont3xt404 from '@/components/404';

Vue.use(Router);

export default new Router({
  mode: 'history',
  base: '/cont3xt/',
  routes: [
    {
      path: '',
      name: 'cont3xt',
      component: Cont3xt
    },
    {
      path: '*',
      name: 'Not Found',
      component: Cont3xt404
    }
  ]
});
