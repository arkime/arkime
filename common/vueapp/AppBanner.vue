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
    <banner-message
      :message="banner.message"
      :effects="banner.effects" />
  </v-alert>
</template>

<script>
import BannerMessage from './BannerMessage.vue';
import { bannerState, loadBanner } from './BannerService.js';

// dismissal is keyed on banner.updated so an edited banner reappears
const DISMISS_KEY = 'arkimeBannerDismissed';
const HEIGHT_VAR = '--app-banner-height'; // see arkime-navbar.css

export default {
  name: 'AppBanner',
  components: { BannerMessage },
  data () {
    return {
      bstate: bannerState(),
      dismissedUpdated: Number(localStorage.getItem(DISMISS_KEY)) || 0,
      resizeObserver: undefined,
      now: Date.now(), // bumped by a timer so expiry hides the banner live
      expiryTimer: undefined
    };
  },
  computed: {
    banner () {
      return this.bstate.banner || {};
    },
    show () {
      if (!(this.banner.enabled && this.banner.message)) { return false; }
      if (this.banner.updated === this.dismissedUpdated) { return false; }
      if (this.banner.expires && this.now >= this.banner.expires) { return false; }
      return true;
    }
  },
  watch: {
    show () { this.syncHeight(); },
    banner () { this.syncHeight(); this.scheduleExpiry(); }
  },
  mounted () {
    this.syncHeight();
    this.scheduleExpiry();
    loadBanner().catch(() => { /* not logged in / no banner -- ignore */ });
  },
  beforeUnmount () {
    this.teardownObserver();
    this.clearExpiry();
    this.setHeight(0);
  },
  methods: {
    setHeight (px) {
      document.documentElement.style.setProperty(HEIGHT_VAR, `${px}px`);
    },
    clearExpiry () {
      if (this.expiryTimer) { clearTimeout(this.expiryTimer); this.expiryTimer = undefined; }
    },
    // bump `now` at expiry so `show` recomputes; re-arm past setTimeout's ~24.8d cap
    scheduleExpiry () {
      this.clearExpiry();
      this.now = Date.now();
      const expires = this.banner.expires;
      if (expires && expires > this.now) {
        const delay = Math.min(expires - this.now, 2147483647);
        this.expiryTimer = setTimeout(() => {
          this.now = Date.now();
          this.scheduleExpiry();
        }, delay + 50);
      }
    },
    teardownObserver () {
      if (this.resizeObserver) {
        this.resizeObserver.disconnect();
        this.resizeObserver = undefined;
      }
    },
    // keep --app-banner-height synced with the rendered banner (0 when hidden)
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
  /* keep the band slim, near navbar height */
  min-height: 0 !important;
  padding-top: 3px !important;
  padding-bottom: 3px !important;
}
.app-banner :deep(.v-alert__prepend) {
  margin-inline-end: 10px;
}
</style>
