import { createApp } from 'vue';
import { createBootstrap } from 'bootstrap-vue-next';

import 'bootstrap/dist/css/bootstrap.css';
import 'bootstrap-vue-next/dist/bootstrap-vue-next.css';

// internal deps
import App from './App.vue';
import ArkimeSessionField from './components/sessions/SessionField.vue';
import HasPermission from './components/utils/HasPermission.vue';
import HasRole from '@common/HasRole.vue';
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
app.use(createBootstrap());

app.directive('has-role', HasRole);
app.directive('has-permission', HasPermission);
app.component('arkime-session-field', ArkimeSessionField);

// these globals are injected into index.ejs.html, by viewer.js
const constants = {
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
  BUILD_VERSION,
  BUILD_DATE,
  LOGOUT_URL,
  LOGOUT_URL_METHOD,
  DEFAULT_TIME_RANGE,
  SPIVIEW_CATEGORY_ORDER
};
// allow vue options api to access constants with this.$constants
app.config.globalProperties.$constants = constants;
// provide constants to vue composition api
app.provide('constants', constants);

app.mount('#app');
