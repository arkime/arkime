<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-menu
    open-on-hover
    :close-on-content-click="false"
    location="bottom">
    <template #activator="{ props: activatorProps }">
      <a
        v-bind="activatorProps"
        :href="versionLink"
        class="navbar-text me-2 text-right align-middle text-white">
        v{{ version }}
      </a>
    </template>
    <div class="version-popup">
      {{ buildInfo }}
    </div>
  </v-menu>
</template>

<script>
import { timezoneDateString } from './vueFilters.js';

// NOTE: parent application must have the constants present in the application
export default {
  name: 'Version',
  props: {
    timezone: { // the timezone to format the time
      type: String,
      required: true
    }
  },
  data () {
    return {
      version: this.$constants.VERSION,
      buildDate: this.$constants.BUILD_DATE,
      buildVersion: this.$constants.BUILD_VERSION
    };
  },
  computed: {
    buildInfo () {
      const dateMs = new Date(this.buildDate).getTime();
      const date = timezoneDateString(dateMs, this.timezone);
      return `${this.buildVersion}${!isNaN(dateMs) ? ' @ ' + date : ''}`;
    },
    versionLink () {
      if (!this.version) return '';
      if (this.version.includes('-GIT')) {
        return `https://github.com/arkime/arkime/commit/${this.buildVersion}`;
      } else {
        return `https://github.com/arkime/arkime/releases/tag/v${this.version}`;
      }
    }
  }
};
</script>

<style scoped>
.version-popup {
  background-color: var(--color-background);
  color: var(--color-foreground);
  border: 1px solid var(--color-gray-light);
  border-radius: 4px;
  padding: 6px 10px;
  font-size: 0.85rem;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.25);
}
</style>
