/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/

import { createApp } from 'vue';
import { createBootstrap } from 'bootstrap-vue-next';

import 'bootstrap/dist/css/bootstrap.css';
import 'bootstrap-vue-next/dist/bootstrap-vue-next.css';

// internal deps
import App from './App.vue';
import router from './router.js';
import store from './store.js';

import '../../../common/common.css';

const app = createApp(App);

app.use(store);
app.use(router);
app.use(createBootstrap());

// these globals are injected into index.ejs.html, by cont3xt.js
const constants = {
  /* eslint-disable no-undef */
  PATH,
  VERSION,
  LOGOUT_URL,
  LOGOUT_URL_METHOD,
  BUILD_DATE,
  BUILD_VERSION
};
// allow vue options api to access constants with this.$constants
app.config.globalProperties.$constants = constants;
// provide constants to vue composition api
app.provide('constants', constants);

app.mount('#app');
