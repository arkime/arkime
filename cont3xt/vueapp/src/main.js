// The Vue build version to load with the `import` command
// (runtime-only or standalone) has been set in webpack.base.conf with an alias.
import Vue from 'vue';
import VueClipboard from 'vue-clipboard2';
import 'bootstrap/dist/css/bootstrap.css';
import 'bootstrap-vue/dist/bootstrap-vue.css';
import BootstrapVue from 'bootstrap-vue/dist/bootstrap-vue.esm';

// internal deps
import App from '@/App';
import router from '@/router';
import store from '@/store';
import '@/utils/filters.js';

import '@/../../../common.css';
import '@/overrides.css';

Vue.config.productionTip = false;

Vue.use(BootstrapVue);
Vue.use(VueClipboard);

/* eslint-disable no-new */
new Vue({
  el: '#app',
  store,
  router,
  components: { App },
  template: '<App/>',
  created: function () {
    // define app constants
    /* eslint-disable no-undef */
    Vue.prototype.$constants = {
      WEB_PATH: WEB_PATH
    };
  }
});
