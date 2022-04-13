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
          class="fa fa-rocket fa-2x text-light"
          title="Can I help you? Click me to see the help page"
        />
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
            Cont3xt
          </router-link>
        </li>
        <li class="nav-item mr-2">
          <router-link
            exact
            to="stats"
            tabindex="-1"
            class="nav-link"
            active-class="active">
            Stats
          </router-link>
        </li>
        <li class="nav-item mr-2">
          <router-link
            to="settings"
            tabindex="-1"
            class="nav-link"
            active-class="active">
            Settings
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
      theme: 'dark', // default theme is dark
      healthError: ''
    };
  },
  computed: {
    ...mapGetters(['getLoading', 'getUser']),
    timezone () {
      return this.getUser?.settings?.timezone || 'local';
    }
  },
  mounted: function () {
    if (localStorage.getItem('cont3xtTheme')) {
      this.theme = localStorage.getItem('cont3xtTheme');
      if (this.theme === 'dark') {
        document.body.classList = [this.theme];
      }
    } else { // there's no theme set use the OS default
      if (window.matchMedia) {
        const darkMode = window.matchMedia('(prefers-color-scheme: dark)').matches;
        this.theme = darkMode ? 'dark' : 'light';
        document.body.classList = darkMode ? ['dark'] : [];
      } else {
        this.theme = 'light';
        document.body.classList = [];
      }
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
      if (this.theme === 'light') {
        this.theme = 'dark';
        document.body.classList = [this.theme];
      } else {
        this.theme = 'light';
        document.body.classList = [];
      }

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
</style>
