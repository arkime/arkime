import Vue from 'vue';
import Router from 'vue-router';
import Parliament from '@/components/Parliament';
import Issues from '@/components/Issues';
import Settings from '@/components/Settings';
import Parliament404 from '@/components/404';
import Help from '@/components/Help';
import AuthService from '@/auth';

Vue.use(Router);

const router = new Router({
  mode: 'history',
  base: '/parliament/',
  scrollBehavior: function (to, from, savedPosition) {
    if (to.hash) {
      return {
        selector: to.hash,
        offset: { x: 0, y: 60 }
      };
    }
  },
  routes: [
    {
      path: '',
      alias: '/',
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
      path: '/help',
      name: 'Help',
      component: Help
    },
    {
      path: '*',
      name: 'Not Found',
      component: Parliament404
    }
  ]
});

router.beforeEach((to, from, next) => {
  AuthService.getAuthInfo();
  next();
});

export default router;
