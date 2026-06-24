<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-alert
    v-if="show"
    ref="bannerEl"
    :type="banner.type"
    variant="flat"
    density="compact"
    closable
    class="app-banner rounded-0 mb-0"
    :close-label="$t('common.close')"
    @click:close="dismiss">
    <span class="app-banner-text">{{ banner.message }}</span>
  </v-alert>
</template>

<script>
// Per-browser dismissal keyed on the banner's updated time so editing or
// re-issuing the banner makes it reappear for everyone who dismissed it.
const DISMISS_KEY = 'arkimeBannerDismissed';
// CSS var (see arkime-navbar.css) that offsets the fixed navbar + page
// content below this top-pinned banner. 0px when no banner is showing.
const HEIGHT_VAR = '--app-banner-height';

export default {
  name: 'AppBanner',
  data () {
    return {
      dismissedUpdated: Number(localStorage.getItem(DISMISS_KEY)) || 0,
      resizeObserver: undefined
    };
  },
  computed: {
    banner () {
      return this.$store.state.banner || {};
    },
    show () {
      return !!(this.banner.enabled && this.banner.message) &&
        this.banner.updated !== this.dismissedUpdated;
    }
  },
  watch: {
    show () { this.syncHeight(); },
    'banner.message' () { this.syncHeight(); },
    'banner.type' () { this.syncHeight(); }
  },
  mounted () {
    this.syncHeight();
  },
  beforeUnmount () {
    this.teardownObserver();
    this.setHeight(0);
  },
  methods: {
    setHeight (px) {
      document.documentElement.style.setProperty(HEIGHT_VAR, `${px}px`);
    },
    teardownObserver () {
      if (this.resizeObserver) {
        this.resizeObserver.disconnect();
        this.resizeObserver = undefined;
      }
    },
    // keep --app-banner-height in sync with the rendered banner height
    // (handles message wrapping / viewport resize); 0 when hidden
    syncHeight () {
      this.$nextTick(() => {
        const el = this.$refs.bannerEl?.$el;
        if (!this.show || !el) {
          this.teardownObserver();
          this.setHeight(0);
          return;
        }
        this.setHeight(el.offsetHeight);
        if (!this.resizeObserver && window.ResizeObserver) {
          this.resizeObserver = new ResizeObserver(() => {
            const cur = this.$refs.bannerEl?.$el;
            if (cur) { this.setHeight(cur.offsetHeight); }
          });
          this.resizeObserver.observe(el);
        }
      });
    },
    dismiss () {
      this.dismissedUpdated = this.banner.updated || 0;
      localStorage.setItem(DISMISS_KEY, this.dismissedUpdated);
      // show flips to false -> the watcher resets the height var
    }
  }
};
</script>

<style scoped>
.app-banner {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  z-index: 8; /* above the fixed navbar (z-index 7) */
}
/* honor newlines an admin types into the message */
.app-banner-text {
  white-space: pre-line;
}
</style>
