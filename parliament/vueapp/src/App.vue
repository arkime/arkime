<template>
  <div id="app">
    <div v-if="compatibleBrowser">
      <parliament-navbar />
      <router-view class="margin-for-nav" />
    </div>
    <div v-else>
      <parliament-upgrade-browser>
      </parliament-upgrade-browser>
    </div>
  </div>
</template>

<script>
import ParliamentNavbar from './components/Navbar';
import ParliamentUpgradeBrowser from './components/UpgradeBrowser';

export default {
  name: 'App',
  components: {
    ParliamentNavbar,
    ParliamentUpgradeBrowser
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
  }
};
</script>

<style>
/* app styles -------------------------------- */
body { background-color: #F0F0F0; }

a.no-href { color: #007bff !important; }
a.no-href:hover { color: #0056b3 !important; }

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
