// The Vue build version to load with the `import` command
// (runtime-only or standalone) has been set in webpack.base.conf with an alias.
// import Vue, { createApp } from 'vue';
import { createApp } from 'vue';
// import BootstrapVue from 'bootstrap-vue/dist/bootstrap-vue.esm';

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

// vuetify css styles
import 'vuetify/styles';
import { createVuetify } from 'vuetify/lib/framework.mjs';

// const { createApp } = Vue;
const app = createApp(App);
const vuetify = createVuetify({ });

app.use(vuetify);
// Vue.use(BootstrapVue); // TODO: toby - this causes a TON of warnings!
// app.use(BootstrapVue); // TODO: toby-rm use this when ready
app.use(store);
app.use(router);

app.directive('has-role', HasRole);

app.config.globalProperties.$constants = {
  // eslint-disable-next-line no-undef
  VERSION, // from cont3xt.js
  // eslint-disable-next-line no-undef
  WEB_PATH, // from cont3xt.js
  // eslint-disable-next-line no-undef
  LOGOUT_URL, // from cont3xt.js
  // eslint-disable-next-line no-undef
  DISABLE_USER_PASSWORD_UI, // from cont3xt.js
  // eslint-disable-next-line no-undef
  DEMO_MODE, // from cont3xt.js
  // eslint-disable-next-line no-undef
  BUILD_DATE, // from vite.config.js
  // eslint-disable-next-line no-undef
  BUILD_VERSION // from vite.config.js
};

app.mount('#app');
