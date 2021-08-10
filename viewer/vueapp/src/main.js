import './createNonce';

// The Vue build version to load with the `import` command
// (runtime-only or standalone) has been set in webpack.base.conf with an alias.
import Vue from 'vue';
import Vuex from 'vuex';
import axios from 'axios';
import VueAxios from 'vue-axios';
import VueMoment from 'vue-moment';
import moment from 'moment-timezone';
import VueClipboard from 'vue-clipboard2';
import BootstrapVue from 'bootstrap-vue';
import 'bootstrap-vue/dist/bootstrap-vue.css';
import 'bootstrap/dist/css/bootstrap.css';
import 'pc-bootstrap4-datetimepicker/build/css/bootstrap-datetimepicker.css';

// internal deps
import App from './App';
import MolochSessionField from './components/sessions/SessionField';
import HasPermission from './components/utils/HasPermission';
import interceptorSetup from './interceptors';
import router from './router';
import store from './store';
import './filters.js';

import '../../../common.css';
// bootstrap overrides
import './overrides.css';
// themed css deps
import './themes/purp.css';
import './themes/blue.css';
import './themes/green.css';
import './themes/cotton-candy.css';
import './themes/dark-2.css';
import './themes/dark-3.css';
import './themes/arkime-light.css';
import './themes/arkime-dark.css';

Vue.config.productionTip = false;

Vue.use(Vuex);
Vue.use(VueClipboard);
Vue.use(BootstrapVue);
Vue.use(VueAxios, axios);
Vue.use(VueMoment, { moment });

Vue.directive('has-permission', HasPermission);
Vue.component('moloch-session-field', MolochSessionField);

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
      MOLOCH_TITLE_CONFIG: MOLOCH_TITLE_CONFIG,
      MOLOCH_DEMO_MODE: MOLOCH_DEMO_MODE,
      MOLOCH_DEV_MODE: MOLOCH_DEV_MODE,
      MOLOCH_VERSION: MOLOCH_VERSION,
      MOLOCH_PATH: MOLOCH_PATH,
      MOLOCH_MULTIVIEWER: MOLOCH_MULTIVIEWER,
      MOLOCH_HASUSERSES: MOLOCH_HASUSERSES,
      MOLOCH_HUNTWARN: MOLOCH_HUNTWARN,
      MOLOCH_HUNTLIMIT: MOLOCH_HUNTLIMIT,
      MOLOCH_ANONYMOUS_MODE: MOLOCH_ANONYMOUS_MODE,
      MOLOCH_BUSINESS_DAY_START: MOLOCH_BUSINESS_DAY_START,
      MOLOCH_BUSINESS_DAY_END: MOLOCH_BUSINESS_DAY_END,
      MOLOCH_BUSINESS_DAYS: MOLOCH_BUSINESS_DAYS,
      BUILD_VERSION: BUILD_VERSION
    };
  }
});
