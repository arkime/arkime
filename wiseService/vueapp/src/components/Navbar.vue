<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <!-- wise navbar -->
  <b-navbar
    fixed="top"
    variant="dark"
    class="px-3"
    :container="false">
    <b-navbar-brand>
      <router-link
        to="help"
        class="me-2"
        exact>
        <img
          alt="hoot"
          id="help-img"
          class="arkime-logo"
          src="/assets/Arkime_Icon_ColorMint.png">
        <BTooltip
          target="help-img"
          title="HOOT! Can I help you? Click me to see the help page" />
      </router-link>
    </b-navbar-brand>

    <!-- page links -->
    <b-navbar-nav class="ms-4">
      <b-nav-item
        :to="{ path: '/', query: queryParams }"
        :class="{'router-link-active': $route.path === '/'}"
        class="nav-link"
        exact>
        Stats
      </b-nav-item>
      <b-nav-item
        to="/query"
        :class="{'router-link-active': $route.path === '/query'}"
        class="nav-link">
        Query
      </b-nav-item>
      <b-nav-item
        to="config"
        :class="{'router-link-active': $route.path === '/config'}"
        class="nav-link">
        Config
      </b-nav-item>
    </b-navbar-nav> <!-- /page links -->

    <b-navbar-nav
      class="ms-auto d-flex align-items-center">
      <!-- data refresh interval select -->
      <div
        v-if="$route.name === 'Stats'"
        style="width:auto;"
        class="input-group input-group-sm ms-1">
        <span class="input-group-text">
          Refresh Data Every
        </span>
        <BFormSelect
          size="sm"
          v-model="statsDataInterval"
          :options="[
            { value: 0, text: 'None' },
            { value:5000, text: '5 seconds' },
            { value:15000, text: '15 seconds' },
            { value:30000, text: '30 seconds' },
            { value:60000, text: '1 minute' }
          ]" />
      </div> <!-- /data interval select -->

      <!-- version -->
      <span class="ps-4">
        <Version timezone="local" />
      </span>

      <!-- help -->
      <router-link to="help">
        <span
          id="help-icon"
          class="fa fa-2x fa-fw fa-question-circle me-2 ms-2 help-link text-theme-button text-theme-gray-hover" />
        <BTooltip
          target="help-icon"
          title="HELP!" />
      </router-link> <!-- /help -->

      <!-- dark/light mode -->
      <button
        type="button"
        id="theme-toggle"
        class="btn btn-sm btn-outline-secondary cursor-pointer me-2"
        @click="toggleTheme">
        <span
          v-if="wiseTheme === 'light'"
          class="fa fa-sun-o fa-fw" />
        <span
          v-if="wiseTheme === 'dark'"
          class="fa fa-moon-o fa-fw" />
      </button>
      <BTooltip
        target="theme-toggle"
        title="Toggle light/dark theme" />
      <!-- /dark/light mode -->
      <Logout
        class="ms-2"
        size="sm" />
    </b-navbar-nav>
  </b-navbar> <!-- /wise navbar -->
</template>

<script>
import Logout from '@common/Logout.vue';
import Version from '@common/Version.vue';

export default {
  name: 'WiseNavbar',
  components: {
    Logout,
    Version
  },
  data: function () {
    return {
      queryParams: {}
    };
  },
  computed: {
    wiseTheme: {
      get () {
        return this.$store.state.wiseTheme;
      },
      set (wiseTheme) {
        this.$store.commit('SET_THEME', wiseTheme);
      }
    },
    statsDataInterval: {
      get () {
        return this.$store.state.statsDataInterval;
      },
      set (dataInterval) {
        this.$store.commit('SET_STATS_DATA_INTERVAL', dataInterval);
      }
    }
  },
  watch: {
    '$route.query': function (newVal, oldVal) {
      this.queryParams = newVal;
    }
  },
  mounted: function () {
    if (this.wiseTheme === 'dark') {
      document.body.classList = [this.wiseTheme];
    }

    this.queryParams = this.$route.query;
  },
  methods: {
    /* page functions -------------------------------------------------------- */
    toggleTheme: function () {
      if (this.wiseTheme === 'light') {
        this.wiseTheme = 'dark';
        document.body.classList = [this.wiseTheme];
      } else {
        this.wiseTheme = 'light';
        document.body.classList = [];
      }
    }
  }
};
</script>

<style scoped>
/* animations -------------------------------- */
.hide-login, .show-login {
  transition: width 0.5s cubic-bezier(0.250, 0.460, 0.450, 0.940),
              opacity 0.2s cubic-bezier(0.250, 0.460, 0.450, 0.940);
}
.show-login {
  width: 200px;
}
.hide-login {
  width: 0px;
  opacity: 0;
  padding: 0;
}
</style>

<style>
nav.navbar li:hover {
  background-color: black;
}
nav.navbar li a {
  transition: all .4s;
}
nav.navbar ul.navbar-nav li.nav-link a.nav-link {
  display: inline-block !important;
  padding-top: 1px;
}
</style>
