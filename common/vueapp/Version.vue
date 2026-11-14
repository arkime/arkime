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
      <span
        v-bind="activatorProps"
        class="d-inline-flex align-center">
        <a
          :href="versionLink"
          class="arkime-version-link me-2 align-middle">
          v{{ version }}
        </a>
        <span
          v-if="showDot"
          role="img"
          :class="['update-dot', { 'update-dot-security': update.security }]"
          :aria-label="$t('updateCheck.available', { version: update.latest })" />
      </span>
    </template>
    <div class="version-popup">
      <div>{{ buildInfo }}</div>

      <template v-if="updateEnabled">
        <hr class="version-divider">

        <div v-if="update.status === 'checking'">
          {{ $t('updateCheck.checking') }}
        </div>
        <div v-else-if="update.latest">
          <a
            :href="update.latestUrl"
            target="_blank"
            rel="noreferrer noopener">
            {{ $t('updateCheck.available', { version: update.latest }) }}
          </a>
          <div
            v-if="update.security"
            class="version-security">
            {{ $t('updateCheck.security') }}
          </div>
          <div
            v-if="update.eol"
            class="version-security">
            {{ $t('updateCheck.eol') }}
          </div>
        </div>
        <div v-else-if="update.status === 'error'">
          {{ $t('updateCheck.failed') }}
        </div>
        <div v-else-if="update.status === 'done'">
          {{ $t('updateCheck.upToDate') }}
        </div>

        <div
          v-if="update.status !== 'checking'"
          class="version-actions">
          <v-btn
            size="x-small"
            :variant="needsOptIn ? 'tonal' : 'text'"
            @click="check">
            {{ needsOptIn ? $t('updateCheck.optIn') : $t('updateCheck.check') }}
          </v-btn>
          <v-btn
            v-if="update.latest"
            size="x-small"
            variant="text"
            @click="dismiss">
            {{ $t('updateCheck.dismiss') }}
          </v-btn>
        </div>

        <!-- only before the first opt in: say what clicking will send -->
        <div
          v-if="needsOptIn"
          class="version-disclosure">
          {{ $t('updateCheck.disclosure', { major: update.major, host: updateHost }) }}
        </div>
      </template>
    </div>
  </v-menu>
</template>

<script>
import { timezoneDateString } from './vueFilters.js';
import {
  updateCheckState, checkForUpdates, optIn, dismissUpdate, hasUndismissedUpdate
} from './UpdateCheckService.js';

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
      buildVersion: this.$constants.BUILD_VERSION,
      update: updateCheckState()
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
    },
    updateEnabled () {
      return this.update.mode !== 'off';
    },
    needsOptIn () {
      return !this.update.optedIn;
    },
    updateHost () {
      try {
        return new URL(this.update.baseUrl).host;
      } catch {
        return this.update.baseUrl;
      }
    },
    showDot () {
      return this.updateEnabled && hasUndismissedUpdate();
    }
  },
  methods: {
    check () {
      if (this.needsOptIn) { optIn(); } else { checkForUpdates({ force: true }); }
    },
    dismiss () {
      dismissUpdate();
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
  max-width: 320px;
}
.version-divider {
  margin: 6px 0;
  border: 0;
  border-top: 1px solid rgb(var(--v-theme-neutral-light));
}
.version-actions {
  display: flex;
  gap: 4px;
  margin-top: 4px;
}
.version-disclosure {
  margin-top: 6px;
  color: rgb(var(--v-theme-neutral-dark));
}
.version-security {
  color: rgb(var(--v-theme-warning));
}
.update-dot {
  width: 7px;
  height: 7px;
  border-radius: 50%;
  background-color: rgb(var(--v-theme-info));
  margin-left: -6px;
  margin-right: 6px;
  align-self: flex-start;
  margin-top: 4px;
}
.update-dot-security {
  background-color: rgb(var(--v-theme-warning));
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
