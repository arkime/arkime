<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div id="app">
    <template v-if="compatibleBrowser">
      <wise-navbar />
      <router-view class="wise-page-content" />
      <wise-footer />
    </template>
    <wise-upgrade-browser v-else />
  </div>
</template>

<script>
import WiseNavbar from './components/Navbar.vue';
import WiseUpgradeBrowser from './components/UpgradeBrowser.vue';
import WiseFooter from '@common/Footer.vue';
import { hydrateOrMigrateTheme } from '@common/themes/persistTheme.js';

export default {
  name: 'App',
  components: {
    WiseNavbar,
    WiseUpgradeBrowser,
    WiseFooter
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
    // user-fetching hook on startup, so we hit /api/user/settings directly.
    fetch('api/user/settings').then((r) => r.ok ? r.json() : {}).then((data) => {
      hydrateOrMigrateTheme({
        url: 'api/user/settings',
        settings: data,
        serverThemeKey: 'wiseVuetifyTheme',
        serverCustomKey: 'wiseVuetifyCustomTheme',
        localThemeKey: 'wiseTheme',
        localCustomKey: 'wiseCustomTheme'
      }, (themeId, customTheme) => {
        this.$store.commit('HYDRATE_THEME_FROM_SERVER', { themeId, customTheme });
      });
    }).catch(() => { /* unauthenticated or backend offline -- localStorage covers us */ });
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
/* a bit of breathing room under the fixed navbar */
.wise-page-content {
  padding-top: 12px;
}
</style>
