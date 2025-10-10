/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/

import { createApp } from 'vue';
import { createBootstrap } from 'bootstrap-vue-next';

// internationalization
import { createI18n } from 'vue-i18n';
import { fetchWrapper } from '@common/fetchWrapper.js';

import 'bootstrap/dist/css/bootstrap.css';
import 'bootstrap-vue-next/dist/bootstrap-vue-next.css';

// internal deps
import App from './App.vue';
import router from './router.js';
import store from './store.js';

import '../../../common/common.css';

/**
 * Dynamically load all locale files from the backend API.
 * This enables dynamic loading of locale files without rebuilding the app,
 * but does not provide true hot-reloading (runtime updates without reload).
 */
async function loadLocales() {
  const messages = {};

  try {
    // Get all locale files at once from the backend
    const localesData = await fetchWrapper({ url: 'api/locales' });
    if (!localesData.success || !localesData.locales) {
      throw new Error('Invalid locales response format');
    }

    // Add all loaded locales to messages
    Object.keys(localesData.locales).forEach(localeCode => {
      const localeData = localesData.locales[localeCode];

      // Validate that the locale has proper structure
      if (localeData.__meta && localeData.__meta.code && localeData.__meta.name) {
        messages[localeCode] = localeData;
      } else {
        console.warn(`Invalid locale data structure for ${localeCode}`);
      }
    });

  } catch (error) {
    console.error('Failed to load locales from backend:', error);

    // Fallback: provide a minimal English locale if everything fails
    messages.en = {
      __meta: { code: 'en', name: 'English', countryCode: 'US' }
    };
  }

  return messages;
}

// Create a placeholder i18n instance that will be configured after locale loading
const i18n = createI18n({
  locale: 'en', // default locale
  fallbackLocale: 'en', // fallback locale when translation is missing
  legacy: false, // use Composition API (Vue 3) - required to avoid deprecation warning
  globalInjection: true, // enable global $t function
  silentTranslationWarn: true, // suppress translation warnings in production
  silentFallbackWarn: true, // suppress fallback warnings in production
  escapeParameterHtml: true, // escape HTML in translations to prevent XSS
  messages: {
    en: { loading: 'Loading...' } // temporary placeholder
  }
});

/**
 * Initialize the application with dynamically loaded locales
 */
async function initializeApp() {
  try {
    // Load all available locales
    const messages = await loadLocales();

    // Configure i18n with loaded messages
    Object.keys(messages).forEach(locale => {
      i18n.global.setLocaleMessage(locale, messages[locale]);
    });

    // Set default locale if English is available, otherwise use the first available locale
    const availableLocales = Object.keys(messages);
    if (availableLocales.includes('en')) {
      i18n.global.locale.value = 'en';
    } else if (availableLocales.length > 0) {
      i18n.global.locale.value = availableLocales[0];
    }
  } catch (error) {
    console.error('Failed to load locales:', error);
    // Continue with default configuration if locale loading fails
  }

  const app = createApp(App);

  app.use(store);
  app.use(router);
  app.use(i18n);
  app.use(createBootstrap());

  // these globals are injected into index.ejs.html, by parliament.js
  const constants = {
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
}

// Initialize the application
initializeApp();
