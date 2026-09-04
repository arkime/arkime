<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span>
    <nav class="arkime-navbar d-flex align-center pe-2">
      <router-link
        to="help"
        class="arkime-navbar-brand"
        exact>
        <img
          alt="hoot"
          class="arkime-logo"
          src="/assets/Arkime_Icon_Black.png">
        <v-tooltip activator="parent">
          HOOT! Can I help you? Click me to see the help page
        </v-tooltip>
      </router-link>

      <div class="arkime-nav-list d-flex align-center">
        <v-btn
          :to="{ path: '/', query: queryParams }"
          :variant="$route.path === '/' ? 'flat' : 'text'"
          :style="$route.path === '/' ? activePillStyle : null"
          size="small"
          class="arkime-nav-btn"
          exact>
          {{ $t('navigation.stats') }}
        </v-btn>
        <v-btn
          to="/query"
          :variant="$route.path === '/query' ? 'flat' : 'text'"
          :style="$route.path === '/query' ? activePillStyle : null"
          size="small"
          class="arkime-nav-btn">
          {{ $t('navigation.query') }}
        </v-btn>
        <v-btn
          to="/config"
          :variant="$route.path === '/config' ? 'flat' : 'text'"
          :style="$route.path === '/config' ? activePillStyle : null"
          size="small"
          class="arkime-nav-btn">
          {{ $t('navigation.config') }}
        </v-btn>
        <v-btn
          to="/settings"
          :variant="$route.path === '/settings' ? 'flat' : 'text'"
          :style="$route.path === '/settings' ? activePillStyle : null"
          size="small"
          class="arkime-nav-btn">
          Settings
        </v-btn>
      </div>

      <v-spacer />

      <div class="arkime-navbar-actions d-flex align-center">
        <!-- version (rainbow gradient via shared Version.vue) -->
        <Version timezone="local" />

        <!-- language switcher -->
        <LanguageSwitcher additional-classes="ms-2" />

        <!-- help button -->
        <v-btn
          to="/help"
          variant="text"
          icon
          size="small"
          density="comfortable"
          class="arkime-help-btn ms-2">
          <v-icon icon="mdi-help-circle" />
          <v-tooltip activator="parent">
            {{ $t('navigation.helpTip') }}
          </v-tooltip>
        </v-btn>

        <!-- stats refresh interval (only on Stats route) -->
        <v-select
          v-if="$route.name === 'Stats'"
          v-model="statsDataInterval"
          :items="statsIntervalOptions"
          item-title="label"
          item-value="value"
          density="compact"
          variant="outlined"
          hide-details
          class="stats-interval-select ms-2"
          prepend-inner-icon="mdi-refresh" />

        <Logout
          class="ms-2"
          size="sm" />
      </div>
    </nav>

    <div class="navbarOffset" />
  </span>
</template>

<script>
import Logout from '@common/Logout.vue';
import Version from '@common/Version.vue';
import LanguageSwitcher from '@common/LanguageSwitcher.vue';
import { registerVuetifyTheme } from '@common/themes/registerVuetifyTheme.js';
import { THEMES } from '@common/themes/manifest.js';

export default {
  name: 'WiseNavbar',
  components: {
    Logout,
    Version,
    LanguageSwitcher
  },
  data () {
    return {
      queryParams: {},
      // active-pill colors -- use button-fg + foreground so the pill
      // flips between themes (white-on-dark in light theme, dark-on-light
      // in dark theme) without picking specific colors per theme.
      activePillStyle: {
        backgroundColor: 'rgb(var(--v-theme-button-fg))',
        color: 'rgb(var(--v-theme-foreground))'
      }
    };
  },
  computed: {
    wiseTheme () {
      return this.$store.state.wiseTheme || 'arkime-light';
    },
    statsDataInterval: {
      get () { return this.$store.state.statsDataInterval; },
      set (val) { this.$store.commit('SET_STATS_DATA_INTERVAL', val); }
    },
    statsIntervalOptions () {
      return [
        { value: 0, label: this.$t('common.none') },
        { value: 5000, label: this.$t('common.secondCount', 5) },
        { value: 15000, label: this.$t('common.secondCount', 15) },
        { value: 30000, label: this.$t('common.secondCount', 30) },
        { value: 60000, label: this.$t('common.minuteCount', 1) }
      ];
    }
  },
  watch: {
    '$route.query': function (newVal) {
      this.queryParams = newVal;
    },
    wiseTheme: {
      immediate: true,
      handler (val) {
        if (this.$vuetify) {
          // Register the saved custom palette before switching to it so
          // theme.change('custom1') resolves -- the palette arrives async
          // from the server via HYDRATE_THEME_FROM_SERVER.
          if (val === 'custom1' && this.$store.state.customTheme?.colors) {
            registerVuetifyTheme(this.$vuetify, 'custom1', this.$store.state.customTheme);
          }
          this.$vuetify.theme.change(val);
        }
        // legacy body.dark hook -- works for ANY dark theme (arkime-dark,
        // dark-2, dark-3, v7-modern-dark, or custom1 with dark flag).
        let dark = false;
        if (val === 'custom1') {
          dark = !!(this.$store.state.customTheme && this.$store.state.customTheme.dark);
        } else {
          const entry = THEMES.find(t => t.id === val);
          dark = !!(entry && entry.dark);
        }
        document.body.classList = dark ? ['dark'] : [];
      }
    }
  },
  mounted () {
    this.queryParams = this.$route.query;
  }
};
</script>

<style scoped>
/* navbar shell + nav-btn typography come from common/vueapp/arkime-navbar.css */
.stats-interval-select {
  max-width: 200px;
}
</style>
