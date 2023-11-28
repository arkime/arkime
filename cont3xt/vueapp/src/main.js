// The Vue build version to load with the `import` command
// (runtime-only or standalone) has been set in webpack.base.conf with an alias.
import Vue from 'vue';
import VueClipboard from 'vue-clipboard2';
import VueMoment from 'vue-moment';
import moment from 'moment-timezone';
import BootstrapVue from 'bootstrap-vue/dist/bootstrap-vue.esm';

// internal deps
import App from '@/App';
import router from '@/router';
import store from '@/store';
import '@/utils/filters.js';
import '@/../../../common/vueapp/vueFilters.js';
import HasRole from '@/../../../common/vueapp/HasRole';

import '@/index.scss'; // includes boostrap(vue) scss
// common css needs to be after ^ because it overrides some bootstrap styles
import '@/../../../common/common.css';
// cont3xt css is applied after common.css because it modifies some of its styles
import '@/cont3xt.css';

Vue.config.productionTip = false;

Vue.use(BootstrapVue);
Vue.use(VueClipboard);
Vue.use(VueMoment, { moment });

Vue.directive('has-role', HasRole);

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
      VERSION,
      WEB_PATH,
      LOGOUT_URL,
      DISABLE_USER_PASSWORD_UI,
      DEMO_MODE,
      BUILD_DATE, // from webpack.DefinePlugin
      BUILD_VERSION // from webpack.DefinePlugin
    };
  }
});
