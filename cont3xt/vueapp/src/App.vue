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
  }
};
</script>

<style>
.margin-for-nav-and-progress {
  margin-top: 80px !important;
}

/* description list styles */
dl.dl-horizontal {
  margin-bottom: 0.2rem;
}
dl.dl-horizontal dt {
  float: left;
  width: 80px;
  overflow: hidden;
  clear: left;
  text-align: right;
  text-overflow: ellipsis;
  white-space: nowrap;
  margin-bottom: 0;
  color: var(--color-secondary);
}
dl.dl-horizontal dd {
  margin-left: 90px;
  margin-bottom: 0;
}

/* loading styles - uses b-overlay slot */
.overlay-loading {
  top: 160px;
  left: 75%;
  z-index: 10002;
  position: fixed;
  text-align: center;
  margin-left: -40px;
}

/* card styles */
body:not(.dark) .card {
  border-color: #FFF;
  background-color: #E9ECEF;
}
.card-body {
  padding: 0.25rem 0.5rem;
}
.results-summary > .card > .card-body,
.results-integration > .b-overlay-wrap > .card > .card-body {
  padding: 0.75rem;
}
</style>
