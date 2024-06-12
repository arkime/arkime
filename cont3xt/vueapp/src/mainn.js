// The Vue build version to load with the `import` command
// (runtime-only or standalone) has been set in webpack.base.conf with an alias.
import Vue from 'vue';
// import { createApp } from 'vue';
// import VueClipboard from 'vue-clipboard2';
// import VueMoment from 'vue-moment';
// import moment from 'moment-timezone';
import BootstrapVue from 'bootstrap-vue/dist/bootstrap-vue.esm';

// internal deps
// import App from '@/App';
// import router from '@/router';
// import store from '@/store';
// import '@/utils/filters.js';
// import '@/../../../common/vueapp/vueFilters.js';
// import HasRole from '@common/HasRole';

import '@/index.scss'; // includes boostrap(vue) scss
// common css needs to be after ^ because it overrides some bootstrap styles
import '@common/../common.css'; // TODO: toby-rm (only backwards import off @common [should @common = vueapp, like it does rn????])
// cont3xt css is applied after common.css because it modifies some of its styles
import '@/cont3xt.css';

// import Vue from 'vue';
// import Vue from 'vue';
import Appp from './Appp.vue';
import store from './store';
// import router from './router';

const { createApp } = Vue;
const app = createApp(Appp);

Vue.use(BootstrapVue);
// app.use(BootstrapVue);
app.use(store);
// app.use(router);
app.config.globalProperties.$constants = {
  TESTME: 123, // TODO: toby-rm
  VERSION,
  WEB_PATH,
  LOGOUT_URL,
  DISABLE_USER_PASSWORD_UI,
  DEMO_MODE,
  // BUILD_DATE, // from webpack.DefinePlugin
  // BUILD_VERSION // from webpack.DefinePlugin
};
Vue.prototype.$constants = {
  TESTME: 123
  // VERSION,
  // WEB_PATH,
  // LOGOUT_URL,
  // DISABLE_USER_PASSWORD_UI,
  // DEMO_MODE,
  // BUILD_DATE, // from webpack.DefinePlugin
  // BUILD_VERSION // from webpack.DefinePlugin
};

app.mount('#app');

// Vue.config.productionTip = false;
//
// Vue.use(BootstrapVue);
// Vue.use(VueClipboard);
// Vue.use(VueMoment, { moment });
//
// Vue.directive('has-role', HasRole);

/* eslint-disable no-new */
// new Vue({
//   el: '#app',
//   store,
//   router,
//   components: { App },
//   template: '<App/>',
//   created: function () {
//     // define app constants
//     /* eslint-disable no-undef */
//     Vue.prototype.$constants = {
//       VERSION,
//       WEB_PATH,
//       LOGOUT_URL,
//       DISABLE_USER_PASSWORD_UI,
//       DEMO_MODE,
//       BUILD_DATE, // from webpack.DefinePlugin
//       BUILD_VERSION // from webpack.DefinePlugin
//     };
//   }
// });
