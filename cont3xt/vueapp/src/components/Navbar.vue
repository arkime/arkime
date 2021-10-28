<template>

  <div>

    <!-- cont3xt navbar -->
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
      </div>
    </nav> <!-- /cont3xt nav -->

  </div>

</template>

<script>
export default {
  name: 'Cont3xtNavbar',
  data: function () {
    return {
      // default theme is dark
      theme: 'dark'
    };
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
nav.navbar > .navbar-brand > img {
  position: absolute;
  height: 52px;
  top: 2px;
}
</style>
