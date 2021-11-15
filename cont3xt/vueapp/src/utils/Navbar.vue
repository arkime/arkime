<template>
  <div>
    <!-- cont3xt navbar -->
    <nav class="navbar navbar-expand navbar-dark bg-dark justify-content-between fixed-top">
      <router-link
        exact
        to="help"
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
            active-class="active"
            class="nav-link"
            exact>
            Cont3xt
          </router-link>
        </li>
        <li class="nav-item mr-2">
          <router-link
            exact
            to="stats"
            class="nav-link"
            active-class="active">
            Stats
          </router-link>
        </li>
        <li class="nav-item mr-2">
          <router-link
            to="settings"
            active-class="active"
            class="nav-link"
            exact>
            Settings
          </router-link>
        </li>
      </ul> <!-- /page links -->
      <div class="form-inline"
        @keyup.enter="login"
        @keyup.esc="clearLogin">
        <!-- dark/light mode -->
        <button type="button"
          @click="toggleTheme"
          v-b-tooltip.hover.left
          class="btn cursor-pointer mr-2"
          title="Toggle light/dark theme"
          :class="{'btn-outline-info':theme === 'dark', 'btn-outline-warning':theme === 'light'}">
          <span v-if="theme === 'light'"
            class="fa fa-sun-o">
          </span>
          <span v-if="theme === 'dark'"
            class="fa fa-moon-o">
          </span>
        </button> <!-- /dark/light mode -->
      </div>
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
import { mapGetters } from 'vuex';

export default {
  name: 'Cont3xtNavbar',
  data: function () {
    return {
      theme: 'dark' // default theme is dark
    };
  },
  computed: {
    ...mapGetters(['getLoading'])
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
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    toggleTheme: function () {
      if (this.theme === 'light') {
        this.theme = 'dark';
        document.body.classList = [this.theme];
      } else {
        this.theme = 'light';
        document.body.classList = [];
      }

      localStorage.setItem('cont3xtTheme', this.theme);
    }
  }
};
</script>

<style scoped>
div.progress {
  top: 46px;
  width: 100%;
  z-index: 1030;
  position: fixed;
  border-radius: 0;
}
body.dark div.progress {
  background-color: #404040;
}
</style>
