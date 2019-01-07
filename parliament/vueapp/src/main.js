// The Vue build version to load with the `import` command
// (runtime-only or standalone) has been set in webpack.base.conf with an alias.
import Vue from 'vue';
import axios from 'axios';
import VueAxios from 'vue-axios';
import VueMoment from 'vue-moment';
import 'bootstrap/dist/css/bootstrap.css';
import 'bootstrap-vue/dist/bootstrap-vue.css';
import vBTooltip from 'bootstrap-vue/es/directives/tooltip/tooltip';
import bDropdown from 'bootstrap-vue/es/components/dropdown/dropdown';
import bDropdownItem from 'bootstrap-vue/es/components/dropdown/dropdown-item';
import bDropdownDropdown from 'bootstrap-vue/es/components/dropdown/dropdown-divider';
import bPagination from 'bootstrap-vue/es/components/pagination/pagination';

// internal deps
import App from './App';
import router from './router';
import store from './store';
import interceptorSetup from './interceptors';
import './filters.js';

Vue.config.productionTip = false;

Vue.use(VueAxios, axios);
Vue.use(VueMoment);

Vue.directive('b-tooltip', vBTooltip);
Vue.component('b-dropdown', bDropdown);
Vue.component('b-dropdown-item', bDropdownItem);
Vue.component('b-dropdown-divider', bDropdownDropdown);
Vue.component('b-pagination', bPagination);

// setup axios http interceptor to add token to reqs
interceptorSetup();

/* eslint-disable no-new */
new Vue({
  el: '#app',
  store,
  router,
  components: { App },
  template: '<App/>'
});
