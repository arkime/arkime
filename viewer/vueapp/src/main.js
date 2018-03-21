// The Vue build version to load with the `import` command
// (runtime-only or standalone) has been set in webpack.base.conf with an alias.
import Vue from 'vue';
import axios from 'axios';
import VueAxios from 'vue-axios';
import VueMoment from 'vue-moment';
import moment from 'moment-timezone';
import BootstrapVue from 'bootstrap-vue/dist/bootstrap-vue.esm';
import 'bootstrap-vue/dist/bootstrap-vue.css';
import 'bootstrap/dist/css/bootstrap.css';

// internal deps
import App from './App';
import HasPermission from './components/utils/HasPermission';
import interceptorSetup from './interceptors';
import router from './router';
import './filters.js';
// bootstrap overrides
import './overrides.css';
// themed css deps
import './themes/default.css';
import './themes/blue.css';
import './themes/green.css';
import './themes/cotton-candy.css';
import './themes/dark-2.css';
import './themes/dark-3.css';

Vue.config.productionTip = false;

Vue.use(VueAxios, axios);
Vue.use(BootstrapVue);
Vue.use(VueMoment, { moment });

Vue.directive('has-permission', HasPermission);

interceptorSetup();

/* eslint-disable no-new */
new Vue({
  el: '#app',
  router,
  components: { App },
  template: '<App/>',
  created: function () {
    // define app constants
    /* eslint-disable no-undef */
    Vue.prototype.$constants = {
      MOLOCH_TITLE_CONFIG: MOLOCH_TITLE_CONFIG,
      MOLOCH_DEMO_MODE: MOLOCH_DEMO_MODE,
      MOLOCH_DEV_MODE: MOLOCH_DEV_MODE,
      MOLOCH_VERSION: MOLOCH_VERSION
    };
  }
});
