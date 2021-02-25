<template>
  <div>
    <!-- wise navbar -->
    <nav class="navbar navbar-expand navbar-dark bg-dark justify-content-between fixed-top">
      <!-- <router-link to="help"
        active-class="active"
        class="navbar-brand"
        exact>
        <img src="assets/Arkime_Icon_ColorMint.png"
          alt="hoot"
          v-b-tooltip.hover
          title="HOOT! Can I help you? Click me to see the help page"
        />
      </router-link> -->
      <img src="assets/Arkime_Icon_ColorMint.png"
        alt="owl image"
      />

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
        <!-- <li class="nav-item mr-2"
          v-if="(hasAuth && loggedIn) || (!hasAuth && !dashboardOnly)">
          <router-link to="settings"
            active-class="active"
            class="nav-link">
            Settings
          </router-link>
        </li> -->
      </ul> <!-- /page links -->

      <!-- login error -->
      <div v-if="error"
        class="alert alert-danger alert-sm mt-2 mr-3">
        <span class="fa fa-exclamation-triangle"></span>&nbsp;
        {{ error }} &nbsp;&nbsp;
        <button type="button"
          class="close cursor-pointer"
          @click="error = ''">
          <span>&times;</span>
        </button>
      </div> <!-- /login error -->

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

      <!-- <div class="form-inline"
        @keyup.enter="login"
        @keyup.esc="clearLogin">

        -- password input --
        <form>
          <input type="text"
            name="username"
            value="..."
            autocomplete="username"
            class="d-none"
          />
          <input class="form-control ml-1"
            tabindex="2"
            type="password"
            v-model="password"
            v-focus-input="focusPassInput"
            placeholder="password please"
            autocomplete="password"
            :class="{'hide-login':!showLoginInput,'show-login':showLoginInput}"
          />
        </form> -- /password input --
        -- login button --
        <button type="button"
          class="btn btn-outline-success cursor-pointer ml-1"
          @click="login"
          tabindex="3"
          v-if="!loggedIn && hasAuth && !dashboardOnly">
          <span class="fa fa-unlock">
          </span>&nbsp;
          Login
        </button> -- /login button --
        -- logout btn --
        <button type="button"
          class="btn btn-outline-danger cursor-pointer ml-1"
          @click="logout"
          tabindex="4"
          v-if="loggedIn">
          <span class="fa fa-lock">
          </span>&nbsp;
          Logout
        </button> -- /logout btn --
      </div> -->
    </nav> <!-- /wise navbar -->
  </div>

</template>

<script>
// import AuthService from '@/auth';
// import { focusInput } from '@/components/utils';

export default {
  name: 'WiseNavbar',
  // directives: { focusInput },
  data: function () {
    return {
      // login error
      error: '',
      // password input vars
      // password: '',
      // showLoginInput: false,
      // focusPassInput: false,
      // default theme is light
      theme: 'light',
      queryParams: {}
    };
  },
  watch: {
    '$route.query': function (newVal, oldVal) {
      this.queryParams = newVal;
    }
  },
  computed: {
    // auth vars
    // hasAuth: function () {
    //   return this.$store.state.hasAuth;
    // },
    // loggedIn: function () {
    //   return this.$store.state.loggedIn;
    // }
  },
  mounted: function () {
    // AuthService.hasAuth();
    // AuthService.isLoggedIn();

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
    // login: function () {
    //   if (this.showLoginInput) {
    //     this.focusPassInput = false;
    //
    //     if (!this.password) {
    //       this.error = 'Must provide a password to login.';
    //       setTimeout(() => { this.focusPassInput = true; });
    //       return;
    //     }
    //
    //     AuthService.login(this.password)
    //       .then((response) => {
    //         this.error = '';
    //         this.password = '';
    //         this.showLoginInput = false;
    //       })
    //       .catch((error) => {
    //         this.password = '';
    //         this.focusPassInput = true;
    //         this.error = error.text || 'Unable to login';
    //       });
    //   } else {
    //     this.showLoginInput = true;
    //     this.focusPassInput = true;
    //   }
    // },
    // logout: function () {
    //   AuthService.logout();
    // },
    // clearLogin: function () {
    //   this.password = '';
    //   this.showLoginInput = false;
    // }
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
