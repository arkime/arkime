import { createApp } from 'vue';
import { createVuetify } from 'vuetify/lib/framework.mjs';
import { aliases as mdiAliases, mdi as mdiSet } from 'vuetify/iconsets/mdi';
import { buildVuetifyThemes } from './theme.js';

import vueDebounce from 'vue-debounce';

// internationalization -- the shared common/vueapp components use $t(),
// so cont3xt needs a vue-i18n instance backed by /api/locales.
import { createI18nInstance } from '@common/i18nSetup.js';

// internal deps
import App from './App.vue';
import store from './store';
import router from './router';
import C3Badge from '@/utils/C3Badge.vue';

// styling/css -- Vuetify FIRST so the shared common.css overrides
// (compact input padding, button height, etc.) win on source order.
import 'vuetify/styles';
import '@/index.scss';
import '@common/../common.css';
import '@common/../arkime-input-group.css';
import '@common/arkime-navbar.css';
import '@/cont3xt.css'; // cont3xt css is applied after common.css and vuetify because it modifies some of their styles
import '@/size.css'; // applied last to override all other styles

async function initializeApp () {
  const app = createApp(App);

  app.directive('debounce', vueDebounce({ defaultTime: '400ms' }));
  app.component('C3Badge', C3Badge);

  const i18n = await createI18nInstance('api/locales');
  app.use(i18n);

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
        color: 'secondary',
        hideDetails: true
      },
      VTooltip: {
        location: 'top', // by default, place tooltips above target--and not on it!!
        delay: 50, // delay of 50ms (same as BootstrapVue)
        maxWidth: 400 // increase the width of tooltips (because andy said so)
      },
      VBtn: { density: 'compact', variant: 'flat' },
      VDataTable: { sortAscIcon: 'mdi:mdi-chevron-up', sortDescIcon: 'mdi:mdi-chevron-down' },
      VDataTableServer: { sortAscIcon: 'mdi:mdi-chevron-up', sortDescIcon: 'mdi:mdi-chevron-down' },
      VCard: {
        elevation: 4,
        density: 'compact'
      }
    },
    theme: buildVuetifyThemes()
  });

  app.use(vuetify);
  app.use(store);
  app.use(router);

  // these globals are injected into index.ejs.html, by cont3xt.js
  /* eslint-disable no-undef */
  app.config.globalProperties.$constants = {

    VERSION, // from cont3xt.js

    WEB_PATH, // from cont3xt.js

    LOGOUT_URL, // from cont3xt.js

    LOGOUT_URL_METHOD, // from cont3xt.js

    DISABLE_USER_PASSWORD_UI, // from cont3xt.js

    DEMO_MODE, // from cont3xt.js

    BUILD_DATE, // from vite.config.js

    BUILD_VERSION // from vite.config.js
  };

  app.mount('#app');
}

initializeApp();
