<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <!-- wise navbar -->
    <nav class="navbar navbar-expand navbar-dark bg-dark justify-content-between fixed-top">
      <router-link to="help">
        <img
          alt="hoot"
          id="help-img"
          src="/assets/Arkime_Icon_ColorMint.png"
        />
        <BTooltip
          target="help-img"
          title="HOOT! Can I help you? Click me to see the help page"
        />
      </router-link>
      <!-- page links -->
      <ul class="navbar-nav me-auto">
        <li class="nav-item me-2">
          <router-link :to="{ path: '/', query: queryParams }"
            active-class="active"
            class="nav-link"
            exact>
            Stats
          </router-link>
        </li>
        <li class="nav-item me-2">
          <router-link to="/query"
            active-class="active"
            class="nav-link">
            Query
          </router-link>
        </li>
        <li class="nav-item me-2">
          <router-link to="config"
            active-class="active"
            class="nav-link">
            Config
          </router-link>
        </li>
      </ul> <!-- /page links -->

      <!-- data refresh interval select -->
      <div v-if="$route.name === 'Stats'"
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
          ]">
        </BFormSelect>

      </div> <!-- /data interval select -->

      <!-- version -->
      <span class="ps-4">
        <Version timezone="local" />
      </span>

      <!-- help -->
      <router-link to="help">
        <span id="help-icon" class="fa fa-2x fa-fw fa-question-circle me-2 ms-2 help-link text-theme-button text-theme-gray-hover" />
        <BTooltip target="help-icon" title="HELP!" />
      </router-link> <!-- /help -->

      <!-- dark/light mode -->
      <button
        type="button"
        id="theme-toggle"
        class="btn btn-outline-secondary cursor-pointer me-2"
        @click="toggleTheme">
        <span v-if="wiseTheme === 'light'"
          class="fa fa-sun-o fa-fw">
        </span>
        <span v-if="wiseTheme === 'dark'"
          class="fa fa-moon-o fa-fw">
        </span>
      </button>
      <BTooltip target="theme-toggle" title="Toggle light/dark theme" />
      <!-- /dark/light mode -->
      <Logout />
    </nav> <!-- /wise navbar -->
  </div>
</template>

<script>
import Logout from '@real_common/Logout.vue';
import Version from '@real_common/Version.vue';

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
nav.navbar img {
  position: absolute;
  height: 52px;
  top: 2px;
}

.navbar-nav {
  margin-left: 4rem;
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
