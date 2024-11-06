import { createApp } from 'vue';
import { createVuetify } from 'vuetify/lib/framework.mjs';
import { createCont3xtTheme } from './theme.js';

import vueDebounce from 'vue-debounce';

// internal deps
import HasRole from '@common/HasRole.vue';
import App from './App.vue';
import store from './store';
import router from './router';
import C3Badge from '@/utils/C3Badge.vue';

// styling/css
import '@/index.scss';
import '@real_common/../common.css';
import 'vuetify/styles'; // vuetify css styles
import '@/cont3xt.css'; // cont3xt css is applied after common.css and vuetify because it modifies some of their styles
import '@/size.css'; // applied last to override all other styles

const app = createApp(App);

app.directive('debounce', vueDebounce({ defaultTime: '400ms' }));
app.component('C3Badge', C3Badge);

const vuetify = createVuetify({
  defaults: {
    VTextField: {
      density: 'compact',
      variant: 'outlined',
      color: 'primary',
      class: 'small-input',
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
    VCard: {
      elevation: 4,
      density: 'compact'
    }
  },
  theme: {
    options: { customProperties: true }, // creates css `var(--xxxxx)` for colors [eg: var(--v-primary-base)]
    defaultTheme: 'cont3xtLightTheme',
    themes: {
      cont3xtLightTheme: createCont3xtTheme('light'),
      cont3xtDarkTheme: createCont3xtTheme('dark')
    }
  }
});

app.use(vuetify);
app.use(store);
app.use(router);

app.directive('has-role', HasRole);

// these globals are injected into index.ejs.html, by cont3xt.js
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
