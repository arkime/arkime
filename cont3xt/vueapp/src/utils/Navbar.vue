<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-flex flex-column">
    <!-- cont3xt navbar -->
    <nav class="navbar navbar-expand navbar-dark bg-dark justify-content-between">
      <router-link
        exact
        to="help"
        tabindex="-1"
        active-class="active">
        <span
          v-tooltip:bottom.close-on-content-click="'Can I help you? Click me to see the help page'"
          title="Can I help you? Click me to see the help page"
          class="fa fa-rocket fa-2x text-light"
          id="tooltipHelp"
          >
        </span>
        <v-tooltip
          v-if="getShiftKeyHold"
          :model-value="getShiftKeyHold"
          location="bottom"
          target="tooltipHelp"
          activator="#tooltipHelp"
          >
          <strong class="help-shortcut text-warning">H</strong>
        </v-tooltip>
      </router-link>
      <!-- page links -->
      <ul class="navbar-nav mr-auto ml-3">
        <li class="nav-item mr-2">
          <router-link
            to="/"
            exact
            tabindex="-1"
            class="nav-link"
            active-class="active">
            <span :class="{'holding-shift':getShiftKeyHold}">C</span>ont3xt
          </router-link>
        </li>
        <li class="nav-item mr-2">
          <router-link
            exact
            to="stats"
            tabindex="-1"
            class="nav-link"
            active-class="active">
            St<span :class="{'holding-shift':getShiftKeyHold}">a</span>ts
          </router-link>
        </li>
        <li class="nav-item mr-2">
          <router-link
            to="settings"
            tabindex="-1"
            class="nav-link"
            active-class="active">
            <span :class="{'holding-shift':getShiftKeyHold}">S</span>ettings
          </router-link>
        </li>
        <li class="nav-item mr-2">
          <router-link
              to="history"
              tabindex="-1"
              v-if="getUser"
              class="nav-link"
              active-class="active">
            Histor<span :class="{'holding-shift':getShiftKeyHold}">y</span>
          </router-link>
        </li>
        <li class="nav-item mr-2">
          <router-link
            to="users"
            tabindex="-1"
            v-if="getUser"
            class="nav-link"
            active-class="active"
            v-has-role="{user:getUser,roles:'usersAdmin'}">
            Users
          </router-link>
        </li>
        <li class="nav-item mr-2">
          <router-link
            to="roles"
            tabindex="-1"
            v-if="getUser && getUser.assignableRoles && getUser.assignableRoles.length > 0"
            class="nav-link"
            active-class="active">
            Roles
          </router-link>
        </li>
      </ul> <!-- /page links -->
      <!-- health check -->
      <div class="mr-2 text-light">
        <span v-if="healthError">
          {{ healthError || 'Network Error' }} - try
          <a tabindex="-1"
            @click="reload"
            class="cursor-pointer">
            reloading the page
          </a>
        </span>
      </div> <!-- /health check -->
      <!-- version -->
      <Version :timezone="timezone" class="no-wrap" />
      <!-- help button -->
      <router-link
        tabindex="-1"
        :to="{ path: 'help' }">
        <span class="fa fa-2x fa-fw fa-question-circle mr-2"
          v-tooltip="'HELP!'"
          title="HELP!">
        </span>
      </router-link>
      <!-- dark/light mode -->
      <div class="form-inline"
        @keyup.enter="login"
        @keyup.esc="clearLogin">
        <v-btn
          tabindex="-1"
          @click="toggleTheme"
          v-tooltip:start="'Toggle light/dark theme'"
          class="square-btn cursor-pointer"
          title="Toggle light/dark theme"
          variant="outlined"
          :color="(theme === 'light') ? 'warning' : 'info'"
          >
          <span v-if="theme === 'light'"
            class="fa fa-sun-o fa-fw">
          </span>
          <span v-if="theme === 'dark'"
            class="fa fa-moon-o fa-fw">
          </span>
        </v-btn>
      </div> <!-- /dark/light mode -->
      <Logout :base-path="path" />
    </nav> <!-- /cont3xt nav -->
    <div class="progress-container">
      <v-progress-linear
          height="8px"
          min="0"
          :max="getLoading.total || 1"
          :striped="getLoading.total != getLoading.received + getLoading.failed"
          :class="{'cursor-help': getLoading.total}"
          :buffer-value="getLoading.failed"
          buffer-color="#FF0000"
          :model-value="getLoading.received"
          color="#00FF00"
        />
      <v-tooltip activator="parent" v-if="getLoading.total">
         TODO: Toby
         <!-- {{ `${this.getLoading.received}/${this.getLoading.total} fetched successfully${(this.getLoading.failed > 0) ? `, ${this.getLoading.failed}/${this.getLoading.total} failed` : ''}` }} -->
      </v-tooltip>

    </div>
  </div>
</template>

<script>
import axios from 'axios';
import { mapGetters, useStore } from 'vuex';

import Logout from '@common/Logout.vue';
import Version from '@common/Version.vue';
import { useTheme } from 'vuetify';
import { watchEffect } from 'vue';
import { useGetters } from '@/vue3-helpers';

let interval;

export default {
  name: 'Cont3xtNavbar',
  components: {
    Logout,
    Version
  },
  setup () {
    const theme = useTheme();
    const store = useStore();
    const { getTheme } = useGetters(store);

    watchEffect(() => {
      document.body.classList = getTheme.value === 'dark' ? ['dark'] : []; // TODO: toby - do we still need?
      theme.global.name.value = (getTheme.value === 'dark') ? 'cont3xtDarkTheme' : 'cont3xtLightTheme';
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
    interval = setInterval(() => {
      axios.get('api/health').then((response) => {
        this.healthError = '';
      }).catch((error) => {
        this.healthError = error.text || error;
      });
    }, 10000);
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    toggleTheme () {
      this.theme = (this.theme === 'dark') ? 'light' : 'dark';

      localStorage.setItem('cont3xtTheme', this.theme);
    },
    reload () {
      window.location.reload();
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
body.dark .progress-container .progress {
  background-color: #404040;
}

.holding-shift {
  color: var(--warning) !important;
}
</style>
