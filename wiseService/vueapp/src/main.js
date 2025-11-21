/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/

import { createApp } from 'vue';
import { createBootstrap } from 'bootstrap-vue-next';

// internationalization
import { createI18nInstance } from '@common/i18nSetup.js';

import 'bootstrap/dist/css/bootstrap.css';
import 'bootstrap-vue-next/dist/bootstrap-vue-next.css';

// internal deps
import App from './App.vue';
import router from './router.js';
import store from './store.js';

import '../../../common/common.css';

/**
 * Initialize the application with dynamically loaded locales
 */
async function initializeApp() {
  // Create and configure i18n instance
  const i18n = await createI18nInstance('api/locales');

  const app = createApp(App);

  app.use(store);
  app.use(router);
  app.use(i18n);
  app.use(createBootstrap());

  // these globals are injected into index.ejs.html, by wiseService.js
  /* eslint-disable no-undef */
  const constants = {
    PATH,
    VERSION,
    LOGOUT_URL,
    LOGOUT_URL_METHOD,
    FOOTER_CONFIG,
    BUILD_DATE,
    BUILD_VERSION
  };
  // allow vue options api to access constants with this.$constants
  app.config.globalProperties.$constants = constants;
  // provide constants to vue composition api
  app.provide('constants', constants);

  app.mount('#app');
}

// Initialize the application
initializeApp();
