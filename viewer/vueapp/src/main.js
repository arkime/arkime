import { createApp } from 'vue';
import { createBootstrap } from 'bootstrap-vue-next';

// internationalization
import { createI18n } from 'vue-i18n';

/**
 * Dynamically load all locale files from common/vueapp/locales/
 * Uses Vite's import.meta.glob for efficient bundling and loading
 */
async function loadLocales() {
  // Dynamically import all JSON files from the locales directory
  const localeModules = import.meta.glob('@common/locales/*.json');

  const messages = {};

  for (const path in localeModules) {
    try {
      const mod = await localeModules[path]();
      // Extract locale code from filename (e.g., 'en.json' -> 'en')
      const localeCode = path.match(/\/([^/]+)\.json$/)?.[1];

      if (localeCode && mod.default && typeof mod.default === 'object') {
        // Validate that the imported file contains translation keys
        if (Object.keys(mod.default).length > 0) {
          // Include the metadata for language switcher functionality
          messages[localeCode] = mod.default;
        }
      }
    } catch (error) {
      console.warn(`Failed to load locale from ${path}:`, error);
    }
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
  messages: {
    en: { loading: 'Loading...' } // temporary placeholder
  }
});

// css frameworks
import 'bootstrap/dist/css/bootstrap.css';
import 'bootstrap-vue-next/dist/bootstrap-vue-next.css';

// internal deps
import App from './App.vue';
import ArkimeSessionField from './components/sessions/SessionField.vue';
import HasPermission from './components/utils/HasPermission.vue';
import HasRole from '@common/HasRole.vue';
import router from './router.js';
import store from './store.js';

// common css
import '../../../common/common.css';
// bootstrap overrides
import './overrides.css';
// themed css deps
import './themes/purp.css';
import './themes/blue.css';
import './themes/green.css';
import './themes/cotton-candy.css';
import './themes/dark-2.css';
import './themes/dark-3.css';
import './themes/arkime-light.css';
import './themes/arkime-dark.css';

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

    console.log(`Loaded ${availableLocales.length} locales: ${availableLocales.join(', ')}`);

  } catch (error) {
    console.error('Failed to load locales:', error);
    // Continue with default configuration if locale loading fails
  }

  const app = createApp(App);

  app.use(store);
  app.use(router);
  app.use(i18n);
  app.use(createBootstrap());

  app.directive('has-role', HasRole);
  app.directive('has-permission', HasPermission);
  app.component('arkime-session-field', ArkimeSessionField);

  // these globals are injected into index.ejs.html, by viewer.js
  const constants = {
    TITLE_CONFIG,
    FOOTER_CONFIG,
    DEMO_MODE,
    VERSION,
    PATH,
    MULTIVIEWER,
    HASUSERSES,
    HUNTWARN,
    HUNTLIMIT,
    ANONYMOUS_MODE,
    BUSINESS_DAY_START,
    BUSINESS_DAY_END,
    BUSINESS_DAYS,
    TURN_OFF_GRAPH_DAYS,
    DISABLE_USER_PASSWORD_UI,
    BUILD_VERSION,
    BUILD_DATE,
    LOGOUT_URL,
    LOGOUT_URL_METHOD,
    DEFAULT_TIME_RANGE,
    SPIVIEW_CATEGORY_ORDER
  };
  // allow vue options api to access constants with this.$constants
  app.config.globalProperties.$constants = constants;
  // provide constants to vue composition api
  app.provide('constants', constants);

  app.mount('#app');
}

// Initialize the application
initializeApp();
