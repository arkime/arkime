<template>

  <div>

    <!-- parliament navbar -->
    <nav class="navbar navbar-expand navbar-dark bg-dark justify-content-between fixed-top">
      <router-link to="help"
        active-class="active"
        class="navbar-brand"
        exact>
        <img src="assets/Arkime_Icon_White.png"
          alt="hoot"
          v-b-tooltip.hover
          title="HOOT! Can I help you? Click me to see the help page"
        />
      </router-link>
      <!-- page links -->
      <ul class="navbar-nav mr-auto ml-5">
        <li class="nav-item mr-2">
          <router-link to="/"
            active-class="active"
            class="nav-link"
            exact>
            Parliament
          </router-link>
        </li>
        <li class="nav-item mr-2">
          <router-link to="issues"
            active-class="active"
            class="nav-link"
            exact>
            Issues
          </router-link>
        </li>
        <li class="nav-item mr-2"
          v-if="(hasAuth && isAdmin) || !hasAuth">
          <router-link to="settings"
            active-class="active"
            class="nav-link">
            Settings
          </router-link>
        </li>
      </ul> <!-- /page links -->
      <!-- version -->
      <span class="pr-2">
        <Version timezone="local" />
      </span>
      <div class="form-inline">
        <!-- dark/light mode -->
        <button type="button"
          class="btn btn-outline-secondary cursor-pointer mr-2"
          @click="toggleTheme"
          v-b-tooltip.hover.left
          title="Toggle light/dark theme">
          <span v-if="theme === 'light'"
            class="fa fa-sun-o">
          </span>
          <span v-if="theme === 'dark'"
            class="fa fa-moon-o">
          </span>
        </button> <!-- /dark/light mode -->
        <!-- refresh interval select -->
        <span class="form-group">
          <div class="input-group">
            <span class="input-group-prepend cursor-help">
              <span class="input-group-text"
                v-b-tooltip.hover.left
                title="Page data refresh interval">
                <span class="fa fa-refresh">
                </span>
              </span>
            </span>
            <select class="form-control refresh-interval-control"
              tabindex="1"
              v-model="refreshInterval">
              <option value="0">Never</option>
              <option value="15000">15 seconds</option>
              <option value="30000">30 seconds</option>
              <option value="45000">45 seconds</option>
              <option value="60000">1 minute</option>
              <option value="300000">5 minutes</option>
            </select>
          </div>
        </span> <!-- /refresh interval select -->
      </div>
    </nav> <!-- /parliament nav -->

  </div>

</template>

<script>
import Focus from '@/../../../common/vueapp/Focus';
import Version from '@/../../../common/vueapp/Version';

export default {
  name: 'ParliamentNavbar',
  components: { Version },
  directives: { Focus },
  data: function () {
    return {
      // default theme is light
      theme: 'light'
    };
  },
  computed: {
    // auth vars
    isUser: function () {
      return this.$store.state.isUser;
    },
    isAdmin: function () {
      return this.$store.state.isAdmin;
    },
    hasAuth: function () {
      return this.$store.state.hasAuth;
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

<style scoped>
nav.navbar > .navbar-brand > img {
  position: absolute;
  height: 52px;
  top: 2px;
}

/* remove browser select box styling */
.refresh-interval-control {
  -webkit-appearance: none;
}
</style>
