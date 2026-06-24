<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-alert
    v-if="show"
    :type="banner.type"
    variant="tonal"
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

export default {
  name: 'AppBanner',
  data () {
    return {
      dismissedUpdated: Number(localStorage.getItem(DISMISS_KEY)) || 0
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
  methods: {
    dismiss () {
      this.dismissedUpdated = this.banner.updated || 0;
      localStorage.setItem(DISMISS_KEY, this.dismissedUpdated);
    }
  }
};
</script>

<style scoped>
.app-banner {
  border-radius: 0;
}
/* honor newlines an admin types into the message */
.app-banner-text {
  white-space: pre-line;
}
</style>
