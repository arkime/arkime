import Vue from 'vue';
import axios from 'axios';
import VueAxios from 'vue-axios';
import App from './App.vue';
import router from './router';
import store from './store';
import { BootstrapVue, IconsPlugin } from 'bootstrap-vue';
import 'bootstrap/dist/css/bootstrap.css';
import 'bootstrap-vue/dist/bootstrap-vue.css';
import '../../../common.css';

Vue.config.productionTip = false;

Vue.use(VueAxios, axios);
Vue.use(BootstrapVue);
Vue.use(IconsPlugin);

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
      BUILD_DATE, // from webpack.DefinePlugin
      BUILD_VERSION // from webpack.DefinePlugin
    };
  }
});
