/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/

import { createApp } from 'vue';
import { createVuetify } from 'vuetify/lib/framework.mjs';
import { aliases as mdiAliases, mdi as mdiSet } from 'vuetify/iconsets/mdi';

// internationalization
import { createI18nInstance } from '@common/i18nSetup.js';

import { buildVuetifyThemes } from './theme.js';

// internal deps
import App from './App.vue';
import router from './router.js';
import store from './store.js';

// Vuetify FIRST so the shared common.css overrides win on source order.
import 'vuetify/styles';
import '../../../common/common.css';
import '../../../common/arkime-input-group.css';
import '../../../common/vueapp/arkime-navbar.css';

/**
 * Initialize the application with dynamically loaded locales
 */
async function initializeApp () {
  // Create and configure i18n instance
  const i18n = await createI18nInstance('api/locales');

  const app = createApp(App);

  const vuetify = createVuetify({
    icons: {
      defaultSet: 'mdi',
      aliases: mdiAliases,
      sets: { mdi: mdiSet }
    },
    defaults: {
      VTextField: {
        density: 'compact',
        variant: 'outlined',
        color: 'primary',
        hideDetails: true
      },
      VSelect: {
        density: 'compact',
        variant: 'outlined',
        color: 'primary',
        hideDetails: true
      },
      VCheckbox: {
        density: 'compact',
        color: 'primary',
        hideDetails: true
      },
      VTooltip: {
        location: 'top',
        delay: 50,
        maxWidth: 400
      },
      VBtn: { density: 'compact', variant: 'flat' },
      VDataTable: { sortAscIcon: 'mdi:mdi-chevron-up', sortDescIcon: 'mdi:mdi-chevron-down' },
      VDataTableServer: { sortAscIcon: 'mdi:mdi-chevron-up', sortDescIcon: 'mdi:mdi-chevron-down' },
      VCard: {
        elevation: 4
      }
    },
    theme: buildVuetifyThemes()
  });

  app.use(store);
  app.use(router);
  app.use(i18n);
  app.use(vuetify);

  // these globals are injected into index.ejs.html, by parliament.js
  /* eslint-disable no-undef */
  const constants = {
    PATH,
    VERSION,
    LOGOUT_URL,
    LOGOUT_URL_METHOD,
    BUILD_DATE,
    BUILD_VERSION,
    FOOTER_CONFIG
  };
  // allow vue options api to access constants with this.$constants
  app.config.globalProperties.$constants = constants;
  // provide constants to vue composition api
  app.provide('constants', constants);

  app.mount('#app');
}

// Initialize the application
initializeApp();
