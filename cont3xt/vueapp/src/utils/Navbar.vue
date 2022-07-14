<template>
  <div>
    <!-- cont3xt navbar -->
    <nav class="navbar navbar-expand navbar-dark bg-dark justify-content-between fixed-top">
      <router-link
        exact
        to="help"
        tabindex="-1"
        active-class="active">
        <span
          v-b-tooltip.hover
          id="tooltipHelp"
          class="fa fa-rocket fa-2x text-light"
          title="Can I help you? Click me to see the help page"
        />
      </router-link>
      <b-tooltip
        triggers=""
        boundary="window"
        placement="bottom"
        target="tooltipHelp"
        :show="getShiftKeyHold">
        <strong class="help-shortcut">H</strong>
      </b-tooltip>
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
            History
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
      <Version :timezone="timezone" />
      <!-- help button -->
      <router-link
        tabindex="-1"
        :to="{ path: 'help' }">
        <span class="fa fa-2x fa-fw fa-question-circle mr-2"
          v-b-tooltip.hover
          title="HELP!">
        </span>
      </router-link>
      <!-- dark/light mode -->
      <div class="form-inline"
        @keyup.enter="login"
        @keyup.esc="clearLogin">
        <button
          tabindex="-1"
          @click="toggleTheme"
          v-b-tooltip.hover.left
          class="btn cursor-pointer"
          title="Toggle light/dark theme"
          :class="{'btn-outline-info':theme === 'dark', 'btn-outline-warning':theme === 'light'}">
          <span v-if="theme === 'light'"
            class="fa fa-sun-o">
          </span>
          <span v-if="theme === 'dark'"
            class="fa fa-moon-o">
          </span>
        </button>
      </div> <!-- /dark/light mode -->
    </nav> <!-- /cont3xt nav -->
    <b-progress
      height="8px"
      class="mt-2 cursor-help"
      :max="getLoading.total"
      :animated="getLoading.total != getLoading.received + getLoading.failed">
      <b-progress-bar
        variant="success"
        :value="getLoading.received"
        v-b-tooltip.hover="`${getLoading.received}/${getLoading.total} fetched successfully`"
      />
      <b-progress-bar
        variant="danger"
        :value="getLoading.failed"
        v-b-tooltip.hover="`${getLoading.failed}/${getLoading.total} failed: ${getLoading.failures.join(', ')}`"
      />
    </b-progress>
  </div>
</template>

<script>
import axios from 'axios';
import { mapGetters } from 'vuex';

import Version from '@/../../../common/vueapp/Version';

let interval;

export default {
  name: 'Cont3xtNavbar',
  components: { Version },
  data: function () {
    return {
      healthError: ''
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
        document.body.classList = value === 'dark' ? ['dark'] : [];
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
      this.theme = (this.theme === 'light') ? 'dark' : 'light';

      localStorage.setItem('cont3xtTheme', this.theme);
    },
    reload () {
      window.location.reload();
    }
  },
  beforeDestroy: function () {
    if (interval) { clearInterval(interval); }
  }
};
</script>

<style scoped>
div.progress {
  top: 44px;
  width: 100%;
  z-index: 1030;
  position: fixed;
  border-radius: 0;
}
body.dark div.progress {
  background-color: #404040;
}

.holding-shift {
  color: var(--warning) !important;
}
</style>
