<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <a
    :href="versionLink"
    :title="buildInfo"
    v-tooltip="buildInfo"
    class="navbar-text mr-2 text-right">
    v{{ version }}
  </a>
</template>

<script>
import { timezoneDateString } from './vueFilters';

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
