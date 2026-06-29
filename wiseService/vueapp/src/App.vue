<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div id="app">
    <template v-if="compatibleBrowser">
      <app-banner />
      <wise-navbar />
      <router-view />
      <wise-footer />
    </template>
    <wise-upgrade-browser v-else />
  </div>
</template>

<script>
import WiseNavbar from './components/Navbar.vue';
import WiseUpgradeBrowser from './components/UpgradeBrowser.vue';
import WiseFooter from '@common/Footer.vue';
import AppBanner from '@common/AppBanner.vue';
import { applyServerTheme } from '@common/themes/persistTheme.js';

export default {
  name: 'App',
  components: {
    WiseNavbar,
    WiseUpgradeBrowser,
    WiseFooter,
    AppBanner
  },
  data: function () {
    return {
      compatibleBrowser: true
    };
  },
  mounted: function () {
    this.compatibleBrowser = (typeof Object.__defineSetter__ === 'function') &&
      !!String.prototype.includes;

    if (!this.compatibleBrowser) {
      console.log('Incompatible browser, please upgrade!');
      return;
    }

    // Hydrate the Vuetify theme from user.settings; if the server has
    // no value yet, push localStorage up so the choice follows the user
    // to other browsers/devices on next load. wise has no other
    // user-fetching hook on startup, so we hit /api/settings directly.
    fetch('api/settings').then((r) => r.ok ? r.json() : {}).then((data) => {
      applyServerTheme(data, (themeId, customTheme) => {
        this.$store.commit('HYDRATE_THEME_FROM_SERVER', { themeId, customTheme });
      });
    }).catch(() => { /* unauthenticated or backend offline -- default theme applies */ });
  }
};
</script>

<style>
/* app styles -------------------------------- */
body {
  background-color: rgb(var(--v-theme-background));
  color: rgb(var(--v-theme-foreground));
}

.text-muted-more { color: #DDDDDD; }

/* small, condensed styles ------------------- */
.alert.alert-sm  {
  font-size: .85rem;
  padding: .25rem .4rem;
  margin-bottom: .5rem;
}
.alert.alert-sm > .close {
  line-height: .75;
}

/* see top level common.css info area for usage */
.info-area { color: rgb(var(--v-theme-foreground)); opacity: 0.7; }
.info-area div { background-color: rgb(var(--v-theme-surface-card)); }

/* styles for bottom footer */
html {
  position: relative;
  min-height: 100%;
}
#app {
  padding-bottom: 25px;
}
</style>
