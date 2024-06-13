// The Vue build version to load with the `import` command
// (runtime-only or standalone) has been set in webpack.base.conf with an alias.
// import Vue from 'vue';
import Vue, { createApp } from 'vue';
// import VueClipboard from 'vue-clipboard2';
// import VueMoment from 'vue-moment';
// import moment from 'moment-timezone';
import BootstrapVue from 'bootstrap-vue/dist/bootstrap-vue.esm';

// internal deps
// import App from '@/App';
// import router from '@/router';
// import store from '@/store';
// import '@/utils/filters.js';
import HasRole from '@common/HasRole.vue';

import '@/index.scss'; // includes boostrap(vue) scss
// common css needs to be after ^ because it overrides some bootstrap styles
import '@common/../common.css'; // TODO: toby-rm (only backwards import off @common [should @common = vueapp, like it does rn????])
// cont3xt css is applied after common.css because it modifies some of its styles
import '@/cont3xt.css';

// import Vue from 'vue';
// import Vue from 'vue';
import App from './App.vue';
import store from './store';
import router from './router';

// const { createApp } = Vue;
const app = createApp(App);

Vue.use(BootstrapVue);
// app.use(BootstrapVue); // TODO: toby-rm use this when ready
app.use(store);
app.use(router);

app.directive('has-role', HasRole);

app.config.globalProperties.$constants = {
  // TODO: toby-rm: no-undefs b/c these are fed in through interpolated index.html/template (UPDATE to make correct? TOBY)
  // eslint-disable-next-line no-undef
  VERSION,
  // eslint-disable-next-line no-undef
  WEB_PATH,
  // eslint-disable-next-line no-undef
  LOGOUT_URL,
  // eslint-disable-next-line no-undef
  DISABLE_USER_PASSWORD_UI,
  // eslint-disable-next-line no-undef
  DEMO_MODE,
  BUILD_DATE, // from webpack.DefinePlugin
  BUILD_VERSION // from webpack.DefinePlugin
};

app.mount('#app');

// Vue.config.productionTip = false;
//
// Vue.use(BootstrapVue);
// Vue.use(VueClipboard);
// Vue.use(VueMoment, { moment });
//

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
