<template>
  <div>
    <!-- wise navbar -->
    <nav class="navbar navbar-expand navbar-dark bg-dark justify-content-between fixed-top">
      <router-link to="help">
        <img alt="hoot"
          src="assets/Arkime_Icon_ColorMint.png"
          v-b-tooltip.hover="'HOOT! Can I help you? Click me to see the help page'"
        />
      </router-link>
      <!-- page links -->
      <ul class="navbar-nav mr-auto">
        <li class="nav-item mr-2">
          <router-link :to="{ path: '/', query: queryParams }"
            active-class="active"
            class="nav-link"
            exact>
            Query
          </router-link>
        </li>
        <li class="nav-item mr-2">
          <router-link to="/statistics"
            active-class="active"
            class="nav-link"
            exact>
            Stats
          </router-link>
        </li>
        <li class="nav-item mr-2">
          <router-link to="config"
            active-class="active"
            class="nav-link"
            exact>
            Config
          </router-link>
        </li>
      </ul> <!-- /page links -->

      <router-link to="help">
        <span class="fa fa-lg fa-fw fa-question-circle mr-2 ml-2 help-link text-theme-button text-theme-gray-hover"
          v-b-tooltip.hover="'HELP!'">
        </span>
      </router-link>

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
    </nav> <!-- /wise navbar -->
  </div>
</template>

<script>

export default {
  name: 'WiseNavbar',
  // directives: { focusInput },
  data: function () {
    return {
      theme: 'light',
      queryParams: {}
    };
  },
  watch: {
    '$route.query': function (newVal, oldVal) {
      this.queryParams = newVal;
    }
  },
  mounted: function () {
    if (localStorage.getItem('wiseTheme')) {
      this.theme = localStorage.getItem('wiseTheme');
      if (this.theme === 'dark') {
        document.body.classList = [this.theme];
      }
    }

    this.queryParams = this.$route.query;
  },
  methods: {
    /* page functions -------------------------------------------------------- */
    toggleTheme: function () {
      if (this.theme === 'light') {
        this.theme = 'dark';
        document.body.classList = [this.theme];
      } else {
        this.theme = 'light';
        document.body.classList = [];
      }

      localStorage.setItem('wiseTheme', this.theme);
    }
  }
};
</script>

<style scoped>
nav.navbar img {
  position: absolute;
  height: 52px;
  top: 2px;
}

.navbar-nav {
  margin-left: 4rem;
}

/* remove browser select box styling */
.refresh-interval-control {
  -webkit-appearance: none;
}

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
