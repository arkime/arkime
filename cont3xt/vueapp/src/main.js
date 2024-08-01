// The Vue build version to load with the `import` command
// (runtime-only or standalone) has been set in webpack.base.conf with an alias.
// import Vue, { createApp } from 'vue';
import { createApp } from 'vue';
// import BootstrapVue from 'bootstrap-vue/dist/bootstrap-vue.esm';

// internal deps
// import App from '@/App';
// import router from '@/router';
// import store from '@/store';
// import '@/utils/filters.js';
import HasRole from '@common/HasRole.vue';

import '@/index.scss'; // includes boostrap(vue) scss
// common css needs to be after ^ because it overrides some bootstrap styles
import '@common/../common.css'; // TODO: toby-rm (only backwards import off @common [should @common = vueapp, like it does rn????])
// cont3xt css is applied after common.css because it modifies some of its styles
import '@/cont3xt.css';

// import Vue from 'vue';
// import Vue from 'vue';
import App from './App.vue';
import store from './store';
import router from './router';

import vueDebounce from 'vue-debounce';
import C3Badge from '@/utils/C3Badge.vue';

// vuetify css styles

// import 'font-awesome/css/font-awesome.min.css';
// import '@mdi/font/css/materialdesignicons.css';
import 'vuetify/styles';
// TODO: toby? import '@/vuetify-customized.scss'; // replacement for: import 'vuetify/styles'; (to add app specific modifications)
import { createVuetify } from 'vuetify/lib/framework.mjs';
// import { aliases, mdi } from 'vuetify/iconsets/mdi';

// import { aliases, fa } from 'vuetify/iconsets/fa'

// const { createApp } = Vue;

/** @type{Object<string, { dark: string, light: string }>} */
const theming = {
  background: {
    dark: '#212121'
  },
  dark: {
    light: '#343a40',
    dark: '#131313'
  },
  light: { // used for variant="light" badges
    light: '#f3f3f3',
    dark: '#131313'
  },
  well: {
    light: '#d6d8d9',
    dark: '#333'
  },
  'well-border': {
    light: '#c6c8ca',
    dark: '#444'
  },
  'progress-bar': {
    light: '#e9ecef',
    dark: '#404040'
  },
  'side-stub': {
    light: '#e9ecef',
    dark: '#555'
  },
  'integration-panel': {
    dark: '#333' // dark well color
  },
  'cont3xt-card': {
    light: '#E9ECEF',
    dark: '#303030'
  },
  'cont3xt-card-border': {
    light: '#FFF',
    dark: '#232323'
    // dark: 'rgba(0, 0, 0, 0.125)' // TODO: toby - can rgba work
  },
  'cont3xt-table-border': {
    light: '#dee2e6',
    dark: '#CCC'
  },
  'integration-btn': { // TODO: toby same !!
    light: '#343a40',
    dark: '#343a40'
  },
  'textarea-border': {
    light: '#ced4da',
    dark: '#EEEEEE'
  }
};

/**
  * @param {'light' | 'dark'} variant
  */
function createTheme (variant) {
  return {
    dark: variant === 'dark',
    colors:
      Object.fromEntries(
        Object.entries(theming)
          .map(([colorName, colors]) => [colorName, colors[variant]])
          .filter(([_, color]) => color != null)
      )
  };
}

const app = createApp(App);

app.directive('debounce', vueDebounce({ defaultTime: '400ms' }));
app.component('C3Badge', C3Badge);

// TODO: toby - csp nonce https://vuetifyjs.com/en/features/theme/#csp-nonce
const vuetify = createVuetify({
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
      delay: 50 // delay of 50ms (same as BootstrapVue)
    },
    VCard: {
      elevation: 4
    }
  },
  theme: {
    options: { customProperties: true }, // creates css `var(--xxxxx)` for colors [eg: var(--v-primary-base)]
    defaultTheme: 'cont3xtLightTheme',
    icons: {
      // defaultSet: 'mdi',
      // aliases,
      // sets: {
      //   mdi
      // }
      // defaultSet: 'fa',
      // aliases,
      // sets: {
      //   fa
      // }
    },
    themes: {
      cont3xtLightTheme: createTheme('light'),
      cont3xtDarkTheme: createTheme('dark')
    }
  }
});

app.use(vuetify);
// Vue.use(BootstrapVue); // TODO: toby - this causes a TON of warnings!
// app.use(BootstrapVue); // TODO: toby-rm use this when ready
app.use(store);
app.use(router);

app.directive('has-role', HasRole);

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
