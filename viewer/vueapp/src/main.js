import { createApp } from 'vue';
import { createBootstrap } from 'bootstrap-vue-next';
import { createVuetify } from 'vuetify/lib/framework.mjs';
import { aliases as faAliases, fa as faSet } from 'vuetify/iconsets/fa4';

// internationalization
import { createI18nInstance } from '@common/i18nSetup.js';

// css frameworks
import 'bootstrap/dist/css/bootstrap.css';
import 'bootstrap-vue-next/dist/bootstrap-vue-next.css';
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
import { createViewerTheme } from './theme.js';

// common css
import '../../../common/common.css';
// bootstrap overrides
import './overrides.css';
// themed css deps -- these overlay on top of Vuetify; see theme.js notes
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
  // Create and configure i18n instance
  const i18n = await createI18nInstance('api/locales');

  const app = createApp(App);

  app.use(store);
  app.use(router);
  app.use(i18n);
  app.use(createBootstrap({
    components: {
      BTooltip: {
        boundary: 'viewport',
        teleportTo: 'body'
      },
      BDropdown: {
        boundary: 'viewport',
        teleportTo: 'body'
      }
    }
  }));

  // Vuetify mounts alongside Bootstrap Vue Next during the migration. Both
  // libraries coexist until viewer is fully migrated and BVN is removed at
  // Phase 4 cutover. Defaults match cont3xt's pattern -- analyst UI is dense.
  //
  // Icons: Font Awesome 4.7 is the icon library throughout viewer (per the
  // migration plan -- MDI swap is deferred). The fa4 iconset maps Vuetify's
  // symbolic aliases ($file, $close, $next, $prev, etc.) to FA 4 classes,
  // so v-file-input, v-select, v-data-table, etc. render with the FA icons
  // already loaded by viewer rather than missing MDI glyphs.
  const vuetify = createVuetify({
    icons: {
      defaultSet: 'fa',
      aliases: faAliases,
      sets: { fa: faSet }
    },
    defaults: {
      VTextField: { density: 'compact', variant: 'outlined', hideDetails: 'auto' },
      VSelect: { density: 'compact', variant: 'outlined', hideDetails: 'auto' },
      VCheckbox: { density: 'compact', hideDetails: 'auto' },
      VTooltip: { location: 'top', delay: 50, maxWidth: 400 },
      VBtn: { density: 'compact', variant: 'flat' }
    },
    theme: {
      options: { customProperties: true },
      defaultTheme: 'arkimeLight',
      themes: {
        arkimeLight: createViewerTheme('light'),
        arkimeDark: createViewerTheme('dark')
      }
    }
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
