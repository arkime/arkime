<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div id="app">
    <div v-if="compatibleBrowser">
      <wise-navbar />
      <router-view class="margin-for-nav" />
    </div>
    <div v-else>
      <wise-upgrade-browser />
    </div>
  </div>
</template>

<script>
import WiseNavbar from './components/Navbar';
import WiseUpgradeBrowser from './components/UpgradeBrowser';

export default {
  name: 'App',
  components: {
    WiseNavbar,
    WiseUpgradeBrowser
  },
  data: function () {
    return {
      compatibleBrowser: true
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
    }
  },
  mounted: function () {
    this.compatibleBrowser = (typeof Object.__defineSetter__ === 'function') &&
      !!String.prototype.includes;

    if (!this.compatibleBrowser) {
      console.log('Incompatible browser, please upgrade!');
    }

    if (window.matchMedia) {
      const darkMode = window.matchMedia('(prefers-color-scheme: dark)').matches;

      let hasTheme = false; // determine if there is a theme set
      if (localStorage && localStorage.vuex) {
        hasTheme = JSON.parse(localStorage.vuex).wiseTheme;
      }

      if (hasTheme) { return; } // don't do anything if theme is already set

      // if there's no theme, default to the same theme as the OS
      this.wiseTheme = darkMode ? 'dark' : 'light';
      document.body.classList = darkMode ? ['dark'] : [];
    }
  }
};
</script>

<style>
/* app styles -------------------------------- */
body { background-color: #F0F0F0; }

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
.info-area { color: #777777; }
.info-area div { background-color: #FFFFFF; }
</style>
