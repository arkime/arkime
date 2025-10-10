<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <!-- parliament navbar -->
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
          :src="logo"
          alt="hoot"
          id="hoot-hoot"
          class="arkime-logo">
        <BTooltip
          target="hoot-hoot"
          placement="bottom">
          HOOT! Can I help you? Click me to see the help page
        </BTooltip>
      </router-link>
    </b-navbar-brand>

    <!-- page links -->
    <b-navbar-nav class="ms-4">
      <b-nav-item
        to="/"
        class="nav-link"
        :class="{'router-link-active': $route.path === '/'}"
        exact>
        Parliament
      </b-nav-item>
      <b-nav-item
        to="issues"
        class="nav-link"
        :class="{'router-link-active': $route.path === '/issues'}"
        exact>
        Issues
      </b-nav-item>
      <b-nav-item
        v-if="isAdmin"
        to="settings"
        :class="{'router-link-active': $route.path === '/settings'}"
        class="nav-link">
        Settings
      </b-nav-item>
      <b-nav-item
        v-if="isAdmin"
        to="users"
        :class="{'router-link-active': $route.path === '/users'}"
        class="nav-link">
        Users
      </b-nav-item>
    </b-navbar-nav> <!-- /page links -->

    <!-- version -->
    <b-navbar-nav
      class="ms-auto d-flex align-items-center">
      <span class="pe-4 align-self-center navbar-text no-wrap">
        <Version timezone="local" />
      </span>
      <LanguageSwitcher />
      <!-- cont3xt url -->
      <a
        v-if="settings.general.cont3xtUrl"
        target="_blank"
        class="btn btn-sm btn-outline-primary cursor-pointer me-2"
        :href="settings.general.cont3xtUrl">
        Cont3xt
      </a> <!-- /cont3xt url -->
      <!-- wise url -->
      <a
        v-if="settings.general.wiseUrl"
        target="_blank"
        class="btn btn-sm btn-outline-info cursor-pointer me-2"
        :href="settings.general.wiseUrl">
        WISE
      </a>
      <!-- /wise url -->
      <!-- dark/light mode -->
      <button
        type="button"
        class="btn btn-xs btn-outline-secondary cursor-pointer ms-2 me-2"
        @click="toggleTheme">
        <span
          v-if="theme === 'light'"
          class="fa fa-sun-o" />
        <span
          v-if="theme === 'dark'"
          class="fa fa-moon-o" />
      </button> <!-- /dark/light mode -->
      <!-- refresh interval select -->
      <BInputGroup size="xs">
        <BInputGroupText>
          <span class="fa fa-refresh" />
        </BInputGroupText>
        <select
          class="form-control refresh-interval-control"
          tabindex="1"
          v-model="refreshInterval">
          <option value="0">
            Never
          </option>
          <option value="15000">
            15 seconds
          </option>
          <option value="30000">
            30 seconds
          </option>
          <option value="45000">
            45 seconds
          </option>
          <option value="60000">
            1 minute
          </option>
          <option value="300000">
            5 minutes
          </option>
        </select>
      </BInputGroup>
      <!-- /refresh interval select -->
      <Logout
        :base-path="path"
        class="ms-2"
        size="sm" />
    </b-navbar-nav> <!-- /version -->
  </b-navbar> <!-- /parliament nav -->
</template>

<script>
import Logout from '@common/Logout.vue';
import Version from '@common/Version.vue';
import LanguageSwitcher from '@common/LanguageSwitcher.vue';

export default {
  name: 'ParliamentNavbar',
  components: {
    Logout,
    Version,
    LanguageSwitcher
  },
  data: function () {
    return {
      // default theme is light
      theme: 'light',
      path: this.$constants.PATH,
      logo: 'assets/Arkime_Icon_White.png'
    };
  },
  computed: {
    // auth vars
    isUser () {
      return this.$store.state.isUser;
    },
    isAdmin () {
      return this.$store.state.isAdmin;
    },
    settings () {
      return this.$store.state.parliament?.settings || { general: {} };
    },
    // data load interval
    refreshInterval: {
      get: function () {
        return this.$store.state.refreshInterval;
      },
      set: function (newValue) {
        this.$store.commit('setRefreshInterval', newValue);
      }
    }
  },
  mounted: function () {
    this.loadRefreshInterval();

    if (localStorage.getItem('parliamentTheme')) {
      this.theme = localStorage.getItem('parliamentTheme');
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

    this.$store.commit('setTheme', this.theme);
  },
  methods: {
    /* page functions -------------------------------------------------------- */
    loadRefreshInterval: function () {
      this.refreshInterval = localStorage.getItem('refreshInterval') || 15000;
    },
    toggleTheme: function () {
      if (this.theme === 'light') {
        this.theme = 'dark';
        document.body.classList = [this.theme];
      } else {
        this.theme = 'light';
        document.body.classList = [];
      }

      localStorage.setItem('parliamentTheme', this.theme);
      this.$store.commit('setTheme', this.theme);
    }
  }
};
</script>

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
