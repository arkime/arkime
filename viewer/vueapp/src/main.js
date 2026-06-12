import { createApp } from 'vue';
import { createVuetify } from 'vuetify/lib/framework.mjs';
import { aliases as mdiAliases, mdi as mdiSet } from 'vuetify/iconsets/mdi';

// internationalization
import { createI18nInstance } from '@common/i18nSetup.js';

// css frameworks
import 'vuetify/styles';

// vue color picker styles
import 'vue-color/style.css';

// internal deps
import App from './App.vue';
import ArkimeSessionField from './components/sessions/SessionField.vue';
import HasPermission from './components/utils/HasPermission.vue';
import HasRole from '@common/HasRole.vue';
import router from './router.js';
import store from './store.js';
import { i18nValue, i18nBDD } from '@common/i18nHelpers.js';
import { buildVuetifyThemes } from './theme.js';

// common css
import '../../../common/common.css';
// shared bridge classes (e.g. .arkime-input-group used across multiple
// components in place of Bootstrap's .input-group). Lives in common/ so it
// also reaches parliament + WISE which still consume common/vueapp/ files.
import '../../../common/arkime-input-group.css';
// shared navbar styles -- viewer is the source of truth for the
// arkime-navbar look; every app imports this to stay in lockstep.
import '@common/arkime-navbar.css';
// arkime element + Vuetify-component bridge styles
import './overrides.css';
// Themes now live in common/vueapp/themes/manifest.js -- 10 Vuetify-native
// themes (8 ports + 2 new v7) registered via buildVuetifyThemes() below.
// The legacy per-theme CSS files in src/themes/*.css are gone.

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

  // Icons: Material Design Icons (mdi) across all four apps.
  const vuetify = createVuetify({
    icons: {
      defaultSet: 'mdi',
      aliases: mdiAliases,
      sets: { mdi: mdiSet }
    },
    defaults: {
      VTextField: { density: 'compact', variant: 'outlined', hideDetails: 'auto' },
      VSelect: { density: 'compact', variant: 'outlined', hideDetails: 'auto' },
      VCheckbox: { density: 'compact', hideDetails: 'auto' },
      VTooltip: { location: 'top', delay: 50, maxWidth: 400 },
      VBtn: { density: 'compact', variant: 'flat' },
      VDataTable: { sortAscIcon: 'mdi:mdi-chevron-up', sortDescIcon: 'mdi:mdi-chevron-down' },
      VDataTableServer: { sortAscIcon: 'mdi:mdi-chevron-up', sortDescIcon: 'mdi:mdi-chevron-down' },
      // tighter gutters everywhere — Vuetify's stock v-row uses 24px
      // gutters which made the settings/PQ/hunt forms read airy compared
      // to non-v-row pages (e.g. Views.vue). `dense` knocks that down to
      // 8px which matches the rest of the app's density.
      VRow: { dense: true }
    },
    theme: buildVuetifyThemes()
  });
  app.use(vuetify);

  app.directive('has-role', HasRole);
  app.directive('has-permission', HasPermission);
  app.directive('i18n-value', i18nValue);
  app.directive('i18n-bdd', i18nBDD);
  app.component('ArkimeSessionField', ArkimeSessionField);

  // these globals are injected into index.ejs.html, by viewer.js
  /* eslint-disable no-undef */
  const constants = {
    TITLE_CONFIG,
    FOOTER_CONFIG,
    DEMO_MODE,
    VERSION,
    PATH,
    MULTIVIEWER,
    HASUSERSES,
    HASTSHARK,
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
    SPIVIEW_CATEGORY_ORDER,
    CLUSTER_DEFAULT
  };
  // allow vue options api to access constants with this.$constants
  app.config.globalProperties.$constants = constants;
  // provide constants to vue composition api
  app.provide('constants', constants);

  app.mount('#app');
}

// Initialize the application
initializeApp();
