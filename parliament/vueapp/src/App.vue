<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div id="app">
    <template v-if="compatibleBrowser">
      <parliament-navbar />
      <router-view />
      <parliament-footer />
    </template>
    <parliament-upgrade-browser v-else />
  </div>
</template>

<script>
import ParliamentNavbar from './components/Navbar.vue';
import ParliamentService from './components/parliament.service.js';
import ParliamentUpgradeBrowser from './components/UpgradeBrowser.vue';
import ParliamentFooter from '@common/Footer.vue';

export default {
  name: 'App',
  components: {
    ParliamentNavbar,
    ParliamentUpgradeBrowser,
    ParliamentFooter
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

    ParliamentService.getParliament().then((data) => {
      this.$store.commit('setParliament', data);
    });
  }
};
</script>

<style>
/* app styles -------------------------------- */
body {
  background-color: rgb(var(--v-theme-background));
  color: rgb(var(--v-theme-foreground));
}

a.no-href { color: #007bff !important; }
a.no-href:hover { color: #0056b3 !important; }

.text-muted-more { color: #DDDDDD; }

/* see top level common.css info area for usage */
.info-area { color: #777777; }
.info-area div { background-color: #FFFFFF; }

/* styles for bottom footer */
html {
  position: relative;
  min-height: 100%;
}
#app {
  padding-bottom: 25px;
}
</style>
