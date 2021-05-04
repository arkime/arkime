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
            Stats
          </router-link>
        </li>
        <li class="nav-item mr-2">
          <router-link to="/query"
            active-class="active"
            class="nav-link"
            exact>
            Query
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

      <!-- data refresh interval select -->
      <div v-if="$route.name === 'Stats'"
        style="width:auto;"
        class="input-group input-group-sm ml-1">
        <div class="input-group-prepend help-cursor"
          v-b-tooltip.hover="'Refresh interval for stats data'">
          <span class="input-group-text">
            Refresh Data Every
          </span>
        </div>
        <b-select class="form-control input-sm"
          v-model="statsDataInterval"
          :options="[
            { value: 0, text: 'None' },
            { value:5000, text: '5 seconds' },
            { value:15000, text: '15 seconds' },
            { value:30000, text: '30 seconds' },
            { value:60000, text: '1 minute' }
          ]">
        </b-select>

      </div> <!-- /data interval select -->

      <!-- help -->
      <router-link to="help">
        <span class="fa fa-lg fa-fw fa-question-circle mr-2 ml-2 help-link text-theme-button text-theme-gray-hover"
          v-b-tooltip.hover="'HELP!'">
        </span>
      </router-link> <!-- /help -->

      <!-- dark/light mode -->
      <button type="button"
        class="btn btn-outline-secondary cursor-pointer mr-2"
        @click="toggleTheme"
        v-b-tooltip.hover.left
        title="Toggle light/dark theme">
        <span v-if="wiseTheme === 'light'"
          class="fa fa-sun-o">
        </span>
        <span v-if="wiseTheme === 'dark'"
          class="fa fa-moon-o">
        </span>
      </button> <!-- /dark/light mode -->
    </nav> <!-- /wise navbar -->
  </div>
</template>

<script>

export default {
  name: 'WiseNavbar',
  data: function () {
    return {
      queryParams: {},
      dataInterval: 0
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
