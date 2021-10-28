<template>
  <div id="app">
    <div v-if="compatibleBrowser">
      <cont3xt-navbar />
      <router-view class="margin-for-nav" />
      <!-- TODO implement progress bar -->
      <b-overlay
        fixed
        no-wrap
        :opacity="0.4"
        :show="getLoading"
      />
    </div>
    <div v-else>
      <cont3xt-upgrade-browser />
    </div>
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import Cont3xtNavbar from './components/Navbar';
import Cont3xtUpgradeBrowser from './components/UpgradeBrowser';

export default {
  name: 'App',
  components: {
    Cont3xtNavbar,
    Cont3xtUpgradeBrowser
  },
  data: function () {
    return {
      compatibleBrowser: true
    };
  },
  computed: {
    ...mapGetters(['getLoading'])
  },
  mounted: function () {
    this.compatibleBrowser = (typeof Object.__defineSetter__ === 'function') &&
      !!String.prototype.includes;

    if (!this.compatibleBrowser) {
      console.log('Incompatible browser, please upgrade!');
    }
  }
};
</script>
