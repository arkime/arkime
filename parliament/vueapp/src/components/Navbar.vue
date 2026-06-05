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
          :src="logo"
          alt="hoot"
          class="arkime-logo">
        <v-tooltip activator="parent">
          {{ $t('navigation.tooltipHelpTip') }}
        </v-tooltip>
      </router-link>

      <div class="arkime-nav-list d-flex align-center">
        <v-btn
          to="/"
          :variant="$route.path === '/' ? 'flat' : 'text'"
          :style="$route.path === '/' ? activePillStyle : null"
          size="small"
          class="arkime-nav-btn"
          exact>
          {{ $t('navigation.parliament') }}
        </v-btn>
        <v-btn
          to="/issues"
          :variant="$route.path === '/issues' ? 'flat' : 'text'"
          :style="$route.path === '/issues' ? activePillStyle : null"
          size="small"
          class="arkime-nav-btn"
          exact>
          {{ $t('navigation.issues') }}
        </v-btn>
        <v-btn
          v-if="isAdmin"
          to="/settings"
          :variant="$route.path === '/settings' ? 'flat' : 'text'"
          :style="$route.path === '/settings' ? activePillStyle : null"
          size="small"
          class="arkime-nav-btn">
          {{ $t('navigation.settings') }}
        </v-btn>
        <v-btn
          v-if="isAdmin"
          to="/users"
          :variant="$route.path === '/users' ? 'flat' : 'text'"
          :style="$route.path === '/users' ? activePillStyle : null"
          size="small"
          class="arkime-nav-btn">
          {{ $t('navigation.users') }}
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

        <!-- ES status indicator -->
        <v-btn
          v-if="$route.path === '/' && nonGreenClusters.length > 0"
          color="error"
          variant="flat"
          size="small"
          class="ms-2"
          @click="scrollToNextNonGreenCluster">
          {{ $t('parliament.navEsIssues') }}
          <v-chip
            size="x-small"
            class="ms-2">
            {{ nonGreenClusters.length }}
          </v-chip>
          <v-tooltip activator="parent">
            {{ $t('parliament.navEsIssuesTip', {cluster: $t('common.clusterCount', nonGreenClusters.length)}) }}
          </v-tooltip>
        </v-btn>

        <!-- cont3xt url -->
        <v-btn
          v-if="settings.general.cont3xtUrl"
          :href="settings.general.cont3xtUrl"
          target="_blank"
          variant="outlined"
          color="primary"
          size="small"
          class="ms-2">
          {{ $t('navigation.cont3xt') }}
        </v-btn>

        <!-- wise url -->
        <v-btn
          v-if="settings.general.wiseUrl"
          :href="settings.general.wiseUrl"
          target="_blank"
          variant="outlined"
          color="info"
          size="small"
          class="ms-2">
          {{ $t('navigation.wise') }}
        </v-btn>

        <!-- refresh interval select -->
        <v-select
          v-model="refreshInterval"
          :items="refreshOptions"
          item-title="label"
          item-value="value"
          density="compact"
          variant="outlined"
          hide-details
          class="refresh-interval-select ms-2"
          prepend-inner-icon="mdi-refresh" />

        <Logout
          :base-path="path"
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
  name: 'ParliamentNavbar',
  components: {
    Logout,
    Version,
    LanguageSwitcher
  },
  data () {
    return {
      path: this.$constants.PATH,
      logo: 'assets/Arkime_Icon_White.png',
      currentNonGreenIndex: 0,
      // active-pill colors -- use button-fg + foreground so the pill
      // flips between themes (white-on-dark in light theme, dark-on-light
      // in dark theme) without us picking specific colors per theme.
      activePillStyle: {
        backgroundColor: 'rgb(var(--v-theme-button-fg))',
        color: 'rgb(var(--v-theme-foreground))'
      }
    };
  },
  computed: {
    isAdmin () { return this.$store.state.isAdmin; },
    settings () {
      return this.$store.state.parliament?.settings || { general: {} };
    },
    theme () {
      return this.$store.state.theme || 'arkime-light';
    },
    refreshInterval: {
      get () { return this.$store.state.refreshInterval; },
      set (newValue) { this.$store.commit('setRefreshInterval', newValue); }
    },
    refreshOptions () {
      return [
        { value: 0, label: this.$t('common.never') },
        { value: 15000, label: this.$t('common.secondCount', { count: 15 }) },
        { value: 30000, label: this.$t('common.secondCount', { count: 30 }) },
        { value: 45000, label: this.$t('common.secondCount', { count: 45 }) },
        { value: 60000, label: this.$t('common.minuteCount', { count: 1 }) },
        { value: 300000, label: this.$t('common.minuteCount', { count: 5 }) }
      ];
    },
    parliament () { return this.$store.state.parliament; },
    stats () { return this.$store.state.stats; },
    nonGreenClusters () {
      const clusters = [];
      if (!this.parliament?.groups || !this.stats) return clusters;

      for (const group of this.parliament.groups) {
        if (group.clusters) {
          for (const cluster of group.clusters) {
            const clusterStats = this.stats[cluster.id];
            if (clusterStats && cluster.type !== 'noAlerts' && (clusterStats.status === 'yellow' || clusterStats.status === 'red' || clusterStats.healthError)) {
              clusters.push({ groupId: group.id, clusterId: cluster.id, cluster });
            }
          }
        }
      }

      return clusters;
    }
  },
  watch: {
    nonGreenClusters () {
      this.currentNonGreenIndex = 0;
    },
    theme: {
      immediate: true,
      handler (val) {
        if (this.$vuetify) {
          // Register the saved custom palette before switching to it so
          // theme.change('custom1') resolves -- the palette arrives async
          // from the server via hydrateThemeFromServer.
          if (val === 'custom1' && this.$store.state.customTheme?.colors) {
            registerVuetifyTheme(this.$vuetify, 'custom1', this.$store.state.customTheme);
          }
          this.$vuetify.theme.change(val);
        }
        // legacy body.dark hook -- works for ANY dark theme.
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
    this.loadRefreshInterval();
  },
  methods: {
    loadRefreshInterval () {
      this.refreshInterval = localStorage.getItem('refreshInterval') || 15000;
    },
    scrollToNextNonGreenCluster () {
      if (this.nonGreenClusters.length === 0) return;

      if (this.$route.path !== '/') {
        this.$router.push('/').then(() => {
          this.$nextTick(() => this.doScroll());
        });
      } else {
        this.doScroll();
      }
    },
    doScroll () {
      const cluster = this.nonGreenClusters[this.currentNonGreenIndex];
      if (!cluster) return;
      this.$store.commit('setScrollToClusterId', cluster.clusterId);
      this.currentNonGreenIndex = (this.currentNonGreenIndex + 1) % this.nonGreenClusters.length;
    }
  }
};
</script>

<style scoped>
/* navbar shell + nav-btn typography come from common/vueapp/arkime-navbar.css */
.refresh-interval-select {
  max-width: 180px;
}
</style>
