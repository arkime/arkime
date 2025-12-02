<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-flex flex-column">
    <!-- cont3xt navbar -->
    <nav class="d-flex flex-row navbar navbar-expand navbar-dark bg-grey-darken-4 justify-space-between align-center pr-2">
      <router-link
        exact
        to="help"
        tabindex="-1"
        active-class="active">
        <v-btn
          variant="text"
          color="white"
          class="square-btn"
          slim>
          <v-icon
            v-tooltip:bottom.close-on-content-click="'Can I help you? Click me to see the help page'"
            title="Can I help you? Click me to see the help page"
            icon="mdi-rocket-launch"
            class="text-white"
            id="tooltipHelp"
            size="x-large" />
        </v-btn>
        <short-cut-tooltip target-id="tooltipHelp">
          H
        </short-cut-tooltip>
      </router-link>
      <!-- page links -->
      <ul class="navbar-nav mr-auto ml-3 d-flex flex-row pa-0">
        <li class="nav-item mr-2">
          <router-link
            to="/"
            exact
            tabindex="-1"
            class="nav-link"
            active-class="active">
            <v-btn
              variant="text"
              color="grey">
              <span
                class="nav-shortcut"
                :class="{'text-warning':getShiftKeyHold}">C</span>ont3xt
            </v-btn>
          </router-link>
        </li>
        <li class="nav-item mr-2">
          <router-link
            exact
            to="stats"
            tabindex="-1"
            class="nav-link"
            active-class="active">
            <v-btn
              variant="text"
              color="grey">
              St<span
                class="nav-shortcut"
                :class="{'text-warning':getShiftKeyHold}">a</span>ts
            </v-btn>
          </router-link>
        </li>
        <li class="nav-item mr-2">
          <router-link
            to="settings"
            tabindex="-1"
            class="nav-link"
            active-class="active">
            <v-btn
              variant="text"
              color="grey">
              <span
                class="nav-shortcut"
                :class="{'text-warning':getShiftKeyHold}">S</span>ettings
            </v-btn>
          </router-link>
        </li>
        <li class="nav-item mr-2">
          <router-link
            to="history"
            tabindex="-1"
            v-if="getUser"
            class="nav-link"
            active-class="active">
            <v-btn
              variant="text"
              color="grey">
              Histor<span
                class="nav-shortcut"
                :class="{'text-warning':getShiftKeyHold}">y</span>
            </v-btn>
          </router-link>
        </li>
        <li
          class="nav-item mr-2"
          v-if="getUser && getUser.roles && getUser.roles.includes('usersAdmin')">
          <router-link
            to="users"
            tabindex="-1"
            v-if="getUser"
            class="nav-link"
            active-class="active">
            <v-btn
              variant="text"
              color="grey">
              Users
            </v-btn>
          </router-link>
        </li>
        <li class="nav-item mr-2">
          <router-link
            to="roles"
            tabindex="-1"
            v-if="getUser && getUser.assignableRoles && getUser.assignableRoles.length > 0"
            class="nav-link"
            active-class="active">
            <v-btn
              variant="text"
              color="grey">
              Roles
            </v-btn>
          </router-link>
        </li>
      </ul> <!-- /page links -->
      <!-- health check -->
      <div class="mr-2 text-muted">
        <span v-if="healthError">
          {{ healthError || 'Network Error' }} - try
          <a
            tabindex="-1"
            @click="reload"
            class="cursor-pointer">
            reloading the page
          </a>
        </span>
      </div> <!-- /health check -->
      <!-- version -->
      <Version
        :timezone="timezone"
        class="no-wrap text-grey" />
      <!-- help button -->
      <router-link
        tabindex="-1"
        :to="{ path: 'help' }">
        <v-btn
          variant="text"
          title="HELP!"
          color="primary"
          slim>
          <v-icon
            size="x-large"
            icon="mdi-help-circle mdi-fw"
            v-tooltip="'HELP!'" />
        </v-btn>
      </router-link>
      <!-- dark/light mode -->
      <v-btn
        size="small"
        tabindex="-1"
        @click="toggleTheme"
        v-tooltip:start="'Toggle light/dark theme'"
        class="square-btn cursor-pointer"
        title="Toggle light/dark theme"
        variant="outlined"
        :color="(theme === 'light') ? 'warning' : 'info'">
        <v-icon
          v-if="theme === 'light'"
          icon="mdi-white-balance-sunny mdi-fw" />
        <v-icon
          v-if="theme === 'dark'"
          icon="mdi-weather-night mdi-fw" />
      </v-btn>
      <!-- </div> -->
      <Logout
        :base-path="path"
        size="small" />
    </nav> <!-- /cont3xt nav -->
    <div class="progress-container bg-progress-bar">
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
  </div>
</template>

<script>
import axios from 'axios';
import { mapGetters, useStore } from 'vuex';

import Logout from '@common/Logout.vue';
import Version from '@common/Version.vue';
import ShortCutTooltip from '@/utils/ShortCutTooltip.vue';
import { useTheme } from 'vuetify';
import { watchEffect } from 'vue';
import { useGetters } from '@/vue3-helpers';

let interval;
const minTimeToWait = 10000;
let timeToWait = minTimeToWait;

export default {
  name: 'Cont3xtNavbar',
  components: {
    Logout,
    Version,
    ShortCutTooltip
  },
  setup () {
    const theme = useTheme();
    const store = useStore();
    const { getTheme } = useGetters(store);

    watchEffect(() => {
      theme.change((getTheme.value === 'dark') ? 'cont3xtDarkTheme' : 'cont3xtLightTheme');
      // once the few lingering reliances on body.dark are removed, this can be safely removed
      document.body.classList = getTheme.value === 'dark' ? ['dark'] : [];
    });
  },
  data: function () {
    return {
      healthError: '',
      path: this.$constants.WEB_PATH
    };
  },
  computed: {
    ...mapGetters(['getLoading', 'getUser', 'getShiftKeyHold', 'getTheme']),
    timezone () {
      return this.getUser?.settings?.timezone || 'local';
    },
    theme: {
      get () {
        return this.getTheme;
      },
      set (value) {
        this.$store.commit('SET_THEME', value);
      }
    }
  },
  mounted: function () {
    if (this.getTheme === undefined) {
      if (window.matchMedia) {
        const darkMode = window.matchMedia('(prefers-color-scheme: dark)').matches;
        this.theme = darkMode ? 'dark' : 'light';
      } else {
        this.theme = 'light';
      }
    } else {
      this.theme = this.getTheme; // initialize theme setting side-effects
    }

    this.getHealth();
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    toggleTheme () {
      this.theme = (this.theme === 'dark') ? 'light' : 'dark';

      localStorage.setItem('cont3xtTheme', this.theme);
    },
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
.progress-container .progress {
  border-radius: 0;
}

.active button {
  color: white !important;
}

.nav-shortcut {
  margin-left: -1px;
  margin-right: -1px;
}
</style>
