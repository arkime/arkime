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
/* TODO apply theme colors to bootstrap elements */
body {
  --color-accent: #00B785;
  --color-orange: #E39300;
  --color-white: #EEEEEE;
  --color-black: #131313;
  --color-gray: #777777;
}
body.dark {
  --color-white: #131313;
  --color-black: #EEEEEE;
}

.text-orange {
  color: var(--color-orange) !important;
}
.text-accent {
  color: var(--color-accent) !important;
}

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
  color: var(--color-accent);
}
dl.dl-horizontal dd {
  margin-left: 90px;
  margin-bottom: 0;
}
</style>
