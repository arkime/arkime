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

// vuetify css styles

// import 'font-awesome/css/font-awesome.min.css';
// import '@mdi/font/css/materialdesignicons.css';
import 'vuetify/styles';
// TODO: toby? import '@/vuetify-customized.scss'; // replacement for: import 'vuetify/styles'; (to add app specific modifications)
import { createVuetify } from 'vuetify/lib/framework.mjs';
// import { aliases, mdi } from 'vuetify/iconsets/mdi';

// import { aliases, fa } from 'vuetify/iconsets/fa'

// const { createApp } = Vue;
const app = createApp(App);
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
      hideDetails: true
    },
    VCheckbox: {
      hideDetails: true
    }
  },
  theme: {
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
      cont3xtLightTheme: {
        dark: false,
        colors: { }
      },
      cont3xtDarkTheme: {
        dark: true,
        colors: {
          background: '#212121'
        }
      }
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
