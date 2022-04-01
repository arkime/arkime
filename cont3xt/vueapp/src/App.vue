<template>
  <div id="app">
    <div v-if="compatibleBrowser">
      <cont3xt-navbar />
      <router-view class="margin-for-nav-and-progress" />
    </div>
    <div v-else>
      <cont3xt-upgrade-browser />
    </div>
  </div>
</template>

<script>
import Cont3xtNavbar from '@/utils/Navbar';
import UserService from '@/components/services/UserService';
import LinkService from '@/components/services/LinkService';
import Cont3xtService from '@/components/services/Cont3xtService';
import Cont3xtUpgradeBrowser from '@/components/pages/UpgradeBrowser';

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
  mounted: function () {
    this.compatibleBrowser = (typeof Object.__defineSetter__ === 'function') &&
      !!String.prototype.includes;

    if (!this.compatibleBrowser) {
      console.log('Incompatible browser, please upgrade!');
    }

    // NOTE: don't need to do anything with the data (the store does it)
    Cont3xtService.getIntegrations();
    LinkService.getLinkGroups();
    UserService.getUser();
    UserService.getRoles();
  }
};
</script>
