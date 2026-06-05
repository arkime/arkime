<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span>
    <nav class="arkime-navbar d-flex align-center pe-2">
      <router-link
        to="help"
        class="arkime-navbar-brand"
        exact>
        <img
          :src="logo"
          alt="hoot"
          class="arkime-logo">
        <v-tooltip activator="parent">
          Can I help you? Click me to see the help page
        </v-tooltip>
      </router-link>

      <div class="arkime-nav-list d-flex align-center">
        <v-btn
          to="/"
          :variant="$route.path === '/' ? 'flat' : 'text'"
          :style="$route.path === '/' ? activePillStyle : null"
          size="small"
          class="arkime-nav-btn"
          exact>
          <span :class="{'nav-shortcut-active': getShiftKeyHold}">C</span>ont3xt
        </v-btn>
        <v-btn
          to="/stats"
          :variant="$route.path === '/stats' ? 'flat' : 'text'"
          :style="$route.path === '/stats' ? activePillStyle : null"
          size="small"
          class="arkime-nav-btn"
          exact>
          St<span :class="{'nav-shortcut-active': getShiftKeyHold}">a</span>ts
        </v-btn>
        <v-btn
          to="/settings"
          :variant="$route.path === '/settings' ? 'flat' : 'text'"
          :style="$route.path === '/settings' ? activePillStyle : null"
          size="small"
          class="arkime-nav-btn">
          <span :class="{'nav-shortcut-active': getShiftKeyHold}">S</span>ettings
        </v-btn>
        <v-btn
          v-if="getUser"
          to="/history"
          :variant="$route.path === '/history' ? 'flat' : 'text'"
          :style="$route.path === '/history' ? activePillStyle : null"
          size="small"
          class="arkime-nav-btn">
          Histor<span :class="{'nav-shortcut-active': getShiftKeyHold}">y</span>
        </v-btn>
        <v-btn
          v-if="getUser && getUser.roles && getUser.roles.includes('usersAdmin')"
          to="/users"
          :variant="$route.path === '/users' ? 'flat' : 'text'"
          :style="$route.path === '/users' ? activePillStyle : null"
          size="small"
          class="arkime-nav-btn">
          Users
        </v-btn>
        <v-btn
          v-if="getUser && getUser.assignableRoles && getUser.assignableRoles.length > 0"
          to="/roles"
          :variant="$route.path === '/roles' ? 'flat' : 'text'"
          :style="$route.path === '/roles' ? activePillStyle : null"
          size="small"
          class="arkime-nav-btn">
          Roles
        </v-btn>
      </div>

      <v-spacer />

      <div class="arkime-navbar-actions d-flex align-center">
        <!-- health check inline message -->
        <span
          v-if="healthError"
          class="me-2 text-medium-emphasis">
          {{ healthError || 'Network Error' }} - try
          <a
            tabindex="-1"
            @click="reload"
            class="cursor-pointer">
            reloading the page
          </a>
        </span>

        <!-- version (rainbow gradient via shared Version.vue) -->
        <Version :timezone="timezone" />

        <!-- help button -->
        <v-btn
          to="/help"
          variant="text"
          icon
          size="small"
          density="comfortable"
          class="arkime-help-btn ms-2">
          <v-icon icon="mdi-help-circle" />
          <v-tooltip activator="parent">
            HELP!
          </v-tooltip>
        </v-btn>

        <Logout
          :base-path="path"
          class="ms-2"
          size="small" />
      </div>
    </nav>

    <div class="navbarOffset" />

    <!-- progress bar -->
    <div class="cont3xt-progress-container">
      <v-progress-linear
        height="8px"
        min="0"
        :max="getLoading.total || 1"
        :striped="getLoading.total != getLoading.received + getLoading.failed"
        :class="{'cursor-help': getLoading.total}"
        :buffer-value="getLoading.failed"
        buffer-color="error"
        :model-value="getLoading.received"
        color="success" />
      <v-tooltip
        activator="parent"
        v-if="getLoading.total">
        {{ `${this.getLoading.received}/${this.getLoading.total} fetched successfully${(this.getLoading.failed > 0) ? `, ${this.getLoading.failed}/${this.getLoading.total} failed` : ''}` }}
      </v-tooltip>
    </div>
  </span>
</template>

<script>
import axios from 'axios';
import { mapGetters, useStore } from 'vuex';

import Logout from '@common/Logout.vue';
import Version from '@common/Version.vue';
import { useTheme } from 'vuetify';
import { watchEffect } from 'vue';
import { useGetters } from '@/vue3-helpers';
import { registerVuetifyTheme } from '@common/themes/registerVuetifyTheme.js';

let interval;
const minTimeToWait = 10000;
let timeToWait = minTimeToWait;

export default {
  name: 'Cont3xtNavbar',
  components: {
    Logout,
    Version
  },
  setup () {
    const theme = useTheme();
    const store = useStore();
    const { getTheme, getCustomTheme, getDarkThemeEnabled } = useGetters(store);

    // Re-runs when the theme id or custom palette arrives from the server
    // (HYDRATE_THEME_FROM_SERVER). Register 'custom1' before switching to
    // it so theme.change('custom1') resolves even when the palette only
    // shows up after the async /api/user fetch.
    watchEffect(() => {
      if (getCustomTheme.value && getCustomTheme.value.colors) {
        registerVuetifyTheme({ theme }, 'custom1', getCustomTheme.value);
      }
      theme.change(getTheme.value || 'arkime-light');
      // legacy body.dark hook for any non-Vuetify selectors that still read it
      document.body.classList = getDarkThemeEnabled.value ? ['dark'] : [];
    });
  },
  data: function () {
    return {
      healthError: '',
      path: this.$constants.WEB_PATH,
      logo: 'assets/Arkime_Icon_White.png',
      // active-pill colors -- use button-fg + foreground so the pill
      // flips between themes (white-on-dark in light theme, dark-on-light
      // in dark theme) without us picking specific colors per theme.
      activePillStyle: {
        backgroundColor: 'rgb(var(--v-theme-button-fg))',
        color: 'rgb(var(--v-theme-foreground))'
      }
    };
  },
  computed: {
    ...mapGetters(['getLoading', 'getUser', 'getShiftKeyHold', 'getTheme']),
    timezone () {
      return this.getUser?.settings?.timezone || 'local';
    }
  },
  mounted: function () {
    this.getHealth();
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    reload () {
      window.location.reload();
    },
    /* helper functions ---------------------------------------------------- */
    getHealth () {
      if (interval) { clearInterval(interval); }

      interval = setInterval(() => {
        axios.get('api/health').then((response) => {
          this.healthError = '';
          if (timeToWait !== minTimeToWait) {
            timeToWait = minTimeToWait;
            this.getHealth();
          }
        }).catch((error) => {
          this.healthError = error.text || error;
          timeToWait = Math.min(timeToWait * 2, 300000); // max 5 minutes between retries
          this.getHealth();
        });
      }, timeToWait);
    }
  },
  beforeUnmount: function () {
    if (interval) { clearInterval(interval); }
  }
};
</script>

<style scoped>
/* navbar shell comes from common/vueapp/arkime-navbar.css; below are
   cont3xt-specific helpers only. */

/* shortcut-letter underline shown when shift is held (so users see
   which key jumps to which page) */
.nav-shortcut-active {
  text-decoration: underline;
  text-decoration-color: rgb(var(--v-theme-warning));
}

/* progress bar sits flush under the navbar */
.cont3xt-progress-container .v-progress-linear {
  border-radius: 0;
}
</style>
