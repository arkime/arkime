import { createApp } from 'vue';

// internal deps
import HasRole from '@common/HasRole.vue';
import App from './App.vue';
import store from './store';
import router from './router';
import C3Badge from '@/utils/C3Badge.vue';

import vueDebounce from 'vue-debounce';

import '@/index.scss';
import '@real_common/../common.css';
import '@/cont3xt.css'; // cont3xt css is applied after common.css because it modifies some of its styles
import 'vuetify/styles'; // vuetify css styles

import { createVuetify } from 'vuetify/lib/framework.mjs';

// NOTE: in theming, it is important to specify both light/dark variants
//       unless it is a pre-existing color. Otherwise, vuetify components
//       will have janky background colors over the unspecified variant
//       since they use the background color to calculate parts of their color.
/** @type{Object<string, { dark: string, light: string }>} */
const theming = {
  info: { all: '#AB4CFF' },
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
  'secondary-gray': { all: '#6C757D' },
  muted: { // used for icons on pre-search cont3xt page
    light: '#6c757d',
    dark: '#6c757d'
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
    dark: '#333', // dark well color
    light: '#FFFFFF'
  },
  'cont3xt-card': {
    light: '#E9ECEF',
    dark: '#303030'
  },
  'cont3xt-card-hover': {
    light: '#d9dbde',
    dark: '#3d3d3d'
  },
  'cont3xt-card-border': {
    light: '#FFF',
    dark: '#232323'
  },
  'cont3xt-table-border': {
    light: '#dee2e6',
    dark: '#CCC'
  },
  'integration-btn': { all: '#343a40' },
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
          .map(([colorName, colors]) => [colorName, colors[variant] ?? colors.all])
          .filter(([_, color]) => color != null)
      )
  };
}

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
      delay: 50 // delay of 50ms (same as BootstrapVue)
    },
    VCard: {
      elevation: 4
    }
  },
  theme: {
    options: { customProperties: true }, // creates css `var(--xxxxx)` for colors [eg: var(--v-primary-base)]
    defaultTheme: 'cont3xtLightTheme',
    themes: {
      cont3xtLightTheme: createTheme('light'),
      cont3xtDarkTheme: createTheme('dark')
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
