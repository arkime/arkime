/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/

import { createApp } from 'vue';
import { createVuetify } from 'vuetify/lib/framework.mjs';
import { aliases as faAliases, fa as faSet } from 'vuetify/iconsets/fa4';
import { mdi as mdiSet } from 'vuetify/iconsets/mdi';

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

  // Override the fa4 iconset's `clear` alias -- by default it maps to
  // `fa-check-circle` which on clearable v-text-fields looks like a
  // green check (mistaken for a checkbox). `fa-times-circle` is the
  // intuitive X-in-circle for a clear action.
  const arkimeFaAliases = { ...faAliases, clear: 'fa-times-circle' };

  const vuetify = createVuetify({
    icons: {
      defaultSet: 'fa',
      aliases: arkimeFaAliases,
      sets: { fa: faSet, mdi: mdiSet }
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
