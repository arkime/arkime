// The Vue build version to load with the `import` command
// (runtime-only or standalone) has been set in webpack.base.conf with an alias.
import Vue from 'vue';
import axios from 'axios';
import VueAxios from 'vue-axios';
import VueMoment from 'vue-moment';
import 'bootstrap/dist/css/bootstrap.css';
import 'bootstrap-vue/dist/bootstrap-vue.css';
import BootstrapVue from 'bootstrap-vue/dist/bootstrap-vue.esm';

// internal deps
import App from './App';
import router from './router';
import store from './store';
import interceptorSetup from './interceptors';
import './filters.js';

import '../../../common/common.css';

Vue.config.productionTip = false;

Vue.use(VueAxios, axios);
Vue.use(VueMoment);
Vue.use(BootstrapVue);

// setup axios http interceptor to add cookie to reqs
interceptorSetup();

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
      PATH,
      VERSION,
      AUTHMODE,
      BUILD_DATE, // from webpack.DefinePlugin
      BUILD_VERSION // from webpack.DefinePlugin
    };
  }
});
