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
        class="arkime-version-link me-2 align-middle">
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
  background-color: rgb(var(--v-theme-background));
  color: rgb(var(--v-theme-foreground));
  border: 1px solid rgb(var(--v-theme-neutral-light));
  border-radius: 4px;
  padding: 6px 10px;
  font-size: 0.85rem;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.25);
}
</style>

<!-- Rainbow gradient on the version link itself. NOT scoped: the
     v-menu activator slot renders the <a> via portal in some setups,
     and we want every app that mounts this component to get the
     effect without re-declaring it. -->
<style>
.arkime-version-link {
  text-decoration: none;
  /* inherit the app's body font + weight; keep rainbow */
  font-size: 0.95rem;
  white-space: nowrap;
  background: linear-gradient(
    90deg,
    #FF8A95 0%,
    #FFB36B 17%,
    #FFE066 33%,
    #8AE890 50%,
    #7BCEFF 67%,
    #B69DFF 83%,
    #FF9DD8 100%
  );
  background-clip: text;
  -webkit-background-clip: text;
  -webkit-text-fill-color: rgb(var(--v-theme-button-fg));
  transition: -webkit-text-fill-color 0.4s ease;
}
.arkime-version-link:hover {
  -webkit-text-fill-color: transparent;
}
</style>
