import { createApp } from 'vue';
import axios from 'axios';
import VueAxios from 'vue-axios';
import VueMoment from 'vue-moment';
import moment from 'moment-timezone';
import VueClipboard from 'vue-clipboard2';
// TODO VUE3 is there a vue3 bootstrap component library?
// import BootstrapVue from 'bootstrap-vue';
// import 'bootstrap-vue/dist/bootstrap-vue.css';
import 'bootstrap/dist/css/bootstrap.css';
// import 'pc-bootstrap4-datetimepicker/build/css/bootstrap-datetimepicker.css';

// internal deps
import App from './App.vue';
import ArkimeSessionField from './components/sessions/SessionField.vue';
import HasPermission from './components/utils/HasPermission.vue';
import HasRole from '@real_common/HasRole.vue';
import interceptorSetup from './interceptors.js';
import router from './router.js';
import store from './store.js';

import '../../../common/common.css';
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

const app = createApp(App);

app.use(store);
app.use(router);

// TODO VUE3 are these even needed?
// Vue.use(VueClipboard);
// Vue.use(BootstrapVue);
// Vue.use(VueAxios, axios);
// Vue.use(VueMoment, { moment });

app.directive('has-role', HasRole);
app.directive('has-permission', HasPermission);
app.component('arkime-session-field', ArkimeSessionField);

interceptorSetup();

// these globals are injected into index.ejs.html, by viewer.js
app.config.globalProperties.$constants = {
  /* eslint-disable no-undef */
  TITLE_CONFIG,
  FOOTER_CONFIG,
  DEMO_MODE,
  VERSION,
  PATH,
  MULTIVIEWER,
  HASUSERSES,
  HUNTWARN,
  HUNTLIMIT,
  ANONYMOUS_MODE,
  BUSINESS_DAY_START,
  BUSINESS_DAY_END,
  BUSINESS_DAYS,
  TURN_OFF_GRAPH_DAYS,
  DISABLE_USER_PASSWORD_UI,
  BUILD_VERSION, // TODO does this work?
  BUILD_DATE, // TODO does this work?
  LOGOUT_URL,
  DEFAULT_TIME_RANGE,
  SPIVIEW_CATEGORY_ORDER
};

app.mount('#app');
