<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="settings-page">
    <!-- sub navbar -->
    <div class="sub-navbar">
      <span class="sub-navbar-title">
        <v-icon
          icon="mdi-cog"
          class="me-1" />
        <span>{{ $t('parliament.settings.title') }}</span>
      </span>
    </div> <!-- /sub navbar -->

    <v-container
      fluid
      class="pa-0">
      <!-- page error -->
      <v-alert
        v-if="error"
        type="error"
        density="compact"
        class="mb-3"
        closable
        @click:close="error = ''">
        {{ error }}
        <span v-if="!settings && !networkError">
          {{ $t('parliament.settings.ifProblem') }}
          <a
            class="no-decoration"
            href="javascript:void(0)"
            @click="restoreDefaults('all')">
            {{ $t('parliament.settings.restoreDefaults') }}
          </a>
        </span>
      </v-alert> <!-- /page error -->

      <!-- page content -->
      <v-row
        v-if="isAdmin"
        no-gutters>
        <!-- navigation -->
        <v-col
          cols="12"
          xl="1"
          lg="2"
          md="2"
          sm="3"
          role="tablist"
          aria-orientation="vertical">
          <v-tabs
            :model-value="visibleTab"
            direction="vertical"
            density="compact"
            color="primary"
            selected-class="font-weight-bold"
            @update:model-value="openView($event)">
            <v-tab value="general">
              <v-icon
                icon="mdi-cog"
                class="me-1" />
              {{ $t('parliament.settings.general') }}
            </v-tab>
            <v-tab value="notifiers">
              <v-icon
                icon="mdi-bell"
                class="me-1" />
              {{ $t('parliament.settings.notifiers') }}
            </v-tab>
            <v-tab value="themes">
              <v-icon
                icon="mdi-brush"
                class="me-1" />
              Themes
            </v-tab>
            <v-tab value="banner">
              <v-icon
                icon="mdi-bullhorn"
                class="me-1" />
              {{ $t('settings.banner.title') }}
            </v-tab>
          </v-tabs>

          <!-- bottom fixed messages -->
          <v-snackbar
            :model-value="!!message"
            :color="msgType === 'danger' ? 'error' : (msgType || 'success')"
            location="bottom"
            :timeout="-1"
            @update:model-value="(v) => { if (!v) message = '' }">
            {{ message }}
            <template #actions>
              <v-btn
                variant="text"
                icon="mdi-close"
                @click="message = ''" />
            </template>
          </v-snackbar>
          <!-- /bottom fixed messages -->
        </v-col> <!-- /navigation -->

        <v-col
          cols="12"
          xl="11"
          lg="10"
          md="10"
          sm="9"
          class="settings-content-pane">
          <!-- general -->
          <div
            v-if="visibleTab === 'general' && settings"
            class="compact-form">
            <v-row>
              <v-col
                xl="9"
                lg="12"
                cols="12">
                <h2 class="d-flex align-center mb-2">
                  <span class="flex-grow-1">
                    {{ $t('parliament.settings.general') }}
                  </span>
                  <v-btn
                    size="small"
                    variant="outlined"
                    color="warning"
                    @click="restoreDefaults('general')">
                    {{ $t('parliament.settings.reset') }}
                  </v-btn>
                </h2>
                <v-divider class="mb-3" />
              </v-col>
            </v-row>
            <v-row v-if="settings.general">
              <!-- out of date -->
              <v-col
                xl="9"
                lg="12"
                cols="12">
                <div class="arkime-input-group arkime-input-group--fluid">
                  <span class="arkime-input-label">
                    {{ $t('parliament.settings.outOfDate') }}
                  </span>
                  <input
                    type="number"
                    class="arkime-input-control"
                    id="outOfDate"
                    @input="debounceInput"
                    v-model="settings.general.outOfDate"
                    max="3600">
                  <span class="arkime-input-label">
                    {{ $t('common.seconds') }}
                  </span>
                </div>
                <small
                  class="d-block text-medium-emphasis mt-1"
                  v-html="$t('parliament.settings.outOfDateHtml')" />
              </v-col> <!-- /out of date -->
              <!-- es query timeout -->
              <v-col
                xl="9"
                lg="12"
                cols="12">
                <div class="arkime-input-group arkime-input-group--fluid">
                  <span class="arkime-input-label">
                    {{ $t('parliament.settings.esQueryTimeout') }}
                  </span>
                  <input
                    type="number"
                    class="arkime-input-control"
                    id="esQueryTimeout"
                    @input="debounceInput"
                    v-model="settings.general.esQueryTimeout"
                    max="60">
                  <span class="arkime-input-label">
                    {{ $t('common.seconds') }}
                  </span>
                </div>
                <small
                  class="d-block text-medium-emphasis mt-1"
                  v-html="$t('parliament.settings.esQueryTimeoutHtml')" />
              </v-col> <!-- /es query timeout -->
              <!-- low packets -->
              <v-col
                xl="9"
                lg="12"
                cols="12">
                <div class="d-flex ga-2">
                  <div class="arkime-input-group arkime-input-group--fluid">
                    <span class="arkime-input-label">
                      {{ $t('parliament.settings.noPackets') }}
                    </span>
                    <input
                      type="number"
                      class="arkime-input-control"
                      id="noPackets"
                      @input="debounceInput"
                      v-model="settings.general.noPackets"
                      max="100000"
                      min="-1">
                    <span class="arkime-input-label">
                      packets
                    </span>
                  </div>
                  <div class="arkime-input-group arkime-input-group--fluid">
                    <span class="arkime-input-label">
                      {{ $t('parliament.settings.noPacketsLength') }}
                    </span>
                    <input
                      type="number"
                      class="arkime-input-control"
                      id="noPacketsLength"
                      @input="debounceInput"
                      v-model="settings.general.noPacketsLength"
                      max="100000"
                      min="1">
                    <span class="arkime-input-label">
                      {{ $t('common.seconds') }}
                    </span>
                  </div>
                </div>
                <small
                  class="d-block text-medium-emphasis mt-1"
                  v-html="$t('parliament.settings.noPacketsHtml')" />
              </v-col> <!-- /low packets -->
              <!-- low disk space -->
              <v-col
                xl="9"
                lg="12"
                cols="12">
                <div class="arkime-input-group arkime-input-group--fluid">
                  <span class="arkime-input-label">
                    {{ $t('parliament.settings.lowDiskSpace') }}
                  </span>
                  <input
                    type="number"
                    class="arkime-input-control"
                    id="lowDiskSpace"
                    @input="debounceInput"
                    v-model="settings.general.lowDiskSpace"
                    :max="settings.general.lowDiskSpaceType === 'percentage' ? 100 : 100000"
                    min="0"
                    :step="settings.general.lowDiskSpaceType === 'percentage' ? 0.1 : 1">
                  <select
                    class="arkime-input-control"
                    style="max-width: 150px;"
                    @change="debounceInput"
                    v-model="settings.general.lowDiskSpaceType">
                    <option value="percentage">
                      {{ $t('common.percent') }}
                    </option>
                    <option value="gb">
                      GB
                    </option>
                  </select>
                </div>
                <small
                  class="d-block text-medium-emphasis mt-1"
                  v-html="$t('parliament.settings.lowDiskSpaceHtml')" />
              </v-col> <!-- /low disk space -->
              <!-- low disk space ES -->
              <v-col
                xl="9"
                lg="12"
                cols="12">
                <div class="arkime-input-group arkime-input-group--fluid">
                  <span class="arkime-input-label">
                    {{ $t('parliament.settings.lowDiskSpaceES') }}
                  </span>
                  <input
                    type="number"
                    class="arkime-input-control"
                    id="lowDiskSpaceES"
                    @input="debounceInput"
                    v-model.number="settings.general.lowDiskSpaceES"
                    :max="settings.general.lowDiskSpaceESType === 'percentage' ? 100 : 100000"
                    min="0"
                    :step="settings.general.lowDiskSpaceESType === 'percentage' ? 0.1 : 1">
                  <select
                    class="arkime-input-control"
                    style="max-width: 150px;"
                    @change="debounceInput"
                    v-model="settings.general.lowDiskSpaceESType">
                    <option value="percentage">
                      {{ $t('common.percent') }}
                    </option>
                    <option value="gb">
                      GB
                    </option>
                  </select>
                </div>
                <small
                  class="d-block text-medium-emphasis mt-1"
                  v-html="$t('parliament.settings.lowDiskSpaceESHtml')" />
              </v-col> <!-- /low disk space ES -->
              <!-- remove issues after -->
              <v-col
                xl="9"
                lg="12"
                cols="12">
                <div class="arkime-input-group arkime-input-group--fluid">
                  <span class="arkime-input-label">
                    {{ $t('parliament.settings.removeIssuesAfter') }}
                  </span>
                  <input
                    type="number"
                    class="arkime-input-control"
                    id="removeIssuesAfter"
                    @input="debounceInput"
                    v-model="settings.general.removeIssuesAfter"
                    max="10080">
                  <span class="arkime-input-label">
                    {{ $t('common.minutes') }}
                  </span>
                </div>
                <small
                  class="d-block text-medium-emphasis mt-1"
                  v-html="$t('parliament.settings.removeIssuesAfterHtml')" />
              </v-col> <!-- /remove issues after -->
              <!-- remove acknowledged issues after -->
              <v-col
                xl="9"
                lg="12"
                cols="12">
                <div class="arkime-input-group arkime-input-group--fluid">
                  <span class="arkime-input-label">
                    {{ $t('parliament.settings.removeAcknowledgedAfter') }}
                  </span>
                  <input
                    type="number"
                    class="arkime-input-control"
                    id="removeAcknowledgedAfter"
                    @input="debounceInput"
                    v-model="settings.general.removeAcknowledgedAfter"
                    max="10080">
                  <span class="arkime-input-label">
                    {{ $t('common.minutes') }}
                  </span>
                </div>
                <small
                  class="d-block text-medium-emphasis mt-1"
                  v-html="$t('parliament.settings.removeAcknowledgedAfterHtml')" />
              </v-col> <!-- /remove acknowledged issues after -->
              <!-- wise url -->
              <v-col
                xl="9"
                lg="12"
                cols="12">
                <div class="arkime-input-group arkime-input-group--fluid">
                  <span class="arkime-input-label">
                    {{ $t('parliament.settings.wiseUrl') }}
                  </span>
                  <input
                    type="text"
                    class="arkime-input-control"
                    id="wiseUrl"
                    @input="debounceInput"
                    v-model="settings.general.wiseUrl">
                </div>
                <small
                  class="d-block text-medium-emphasis mt-1"
                  v-html="$t('parliament.settings.wiseUrlHtml')" />
              </v-col> <!-- /wise url -->
              <!-- cont3xt url -->
              <v-col
                xl="9"
                lg="12"
                cols="12">
                <div class="arkime-input-group arkime-input-group--fluid">
                  <span class="arkime-input-label">
                    {{ $t('parliament.settings.cont3xtUrl') }}
                  </span>
                  <input
                    type="text"
                    class="arkime-input-control"
                    id="cont3xtUrl"
                    @input="debounceInput"
                    v-model="settings.general.cont3xtUrl">
                </div>
                <small
                  class="d-block text-medium-emphasis mt-1"
                  v-html="$t('parliament.settings.cont3xtUrlHtml')" />
              </v-col> <!-- /cont3xt url -->
            </v-row>
          </div>
          <!-- /general -->

          <!-- notifiers tab -->
          <div v-if="visibleTab === 'notifiers' && settings">
            <!-- hostname -->
            <v-row>
              <v-col cols="12">
                <div class="arkime-input-group arkime-input-group--fluid">
                  <span class="arkime-input-label">
                    {{ $t('parliament.settings.parliamentHostname') }}
                  </span>
                  <input
                    type="text"
                    class="arkime-input-control"
                    id="hostname"
                    @input="debounceInput"
                    v-model="settings.general.hostname">
                  <span class="arkime-input-label">
                    <input
                      type="checkbox"
                      class="arkime-check-input me-2"
                      @input="debounceInput"
                      v-model="settings.general.includeUrl">
                    {{ $t('parliament.settings.includeDashboardUrl') }}
                  </span>
                </div>
                <small
                  class="d-block text-medium-emphasis mt-1"
                  v-html="$t('parliament.settings.parliamentHostnameHtml')" />
              </v-col>
            </v-row> <!-- /hostname -->

            <v-divider class="my-3" />

            <Notifiers
              parent-app="parliament"
              @display-message="displayMessage"
              help-intl-id="settings.notifiers.helpParliament" />
          </div> <!-- /notifiers tab -->

          <!-- themes tab -->
          <div v-if="visibleTab === 'themes'">
            <h1 class="mb-3">
              Themes
            </h1>
            <p class="text-medium-emphasis mb-4">
              Choose a theme or build your own. Themes are saved to your
              account and apply across all Arkime apps.
            </p>
            <ThemePicker
              :model-value="getTheme"
              :themes="themes"
              :custom-theme="getCustomTheme"
              @update:model-value="onThemeChange"
              @update:custom-theme="onCustomThemeChange" />
          </div> <!-- /themes tab -->
          <div v-if="visibleTab === 'banner'">
            <banner-settings />
          </div> <!-- /banner tab -->
        </v-col> <!-- /content -->
      </v-row> <!-- /page content -->
    </v-container>
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import SettingsService from './settings.service.js';
import UserService from '@/components/user.service.js';
import Notifiers from '@common/Notifiers.vue';
import ThemePicker from '@common/ThemePicker.vue';
import BannerSettings from '@common/BannerSettings.vue';
import { THEMES } from '@common/themes/manifest.js';
import { registerVuetifyTheme } from '@common/themes/registerVuetifyTheme.js';

let inputDebounce;
let msgCloseTimeout;

export default {
  name: 'Settings',
  components: { Notifiers, ThemePicker, BannerSettings },
  data: function () {
    return {
      message: '',
      msgType: 'success',
      // page error
      error: '',
      networkError: false,
      // default tab
      visibleTab: 'general',
      // theme picker source list (the 10 baked-in themes)
      themes: THEMES
    };
  },
  computed: {
    ...mapGetters(['getTheme', 'getCustomTheme']),
    settings: {
      get () {
        const settings = this.$store.state.parliament?.settings || { general: {} };
        return { // Return derived object with defaults (without mutation)
          ...settings,
          general: {
            ...settings.general,
            lowDiskSpaceES: settings.general?.lowDiskSpaceES ?? 15,
            lowDiskSpaceESType: settings.general?.lowDiskSpaceESType || 'percentage',
            lowDiskSpace: settings.general?.lowDiskSpace ?? 4,
            lowDiskSpaceType: settings.general?.lowDiskSpaceType || 'percentage'
          }
        };
      },
      set (value) {
        this.$store.commit('setSettings', value);
      }
    },
    isAdmin: function () {
      return this.$store.state.isAdmin;
    }
  },
  mounted: function () {
    UserService.getRoles();

    // does the url specify a tab in hash
    let tab = window.location.hash;
    if (tab) { // if there is a tab specified and it's a valid tab
      tab = tab.replace(/^#/, '');
      if (tab === 'general' || tab === 'notifiers' || tab === 'themes' || tab === 'banner') {
        this.visibleTab = tab;
      }
    }
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    openView: function (tabName) {
      if (this.visibleTab === tabName) { return; }

      this.visibleTab = tabName;
      this.$router.push({
        hash: `#${tabName}`
      });
    },
    saveSettings: function () {
      this.message = '';
      if (msgCloseTimeout) { clearTimeout(msgCloseTimeout); }

      if (this.settings.general.noPackets === '' || this.settings.general.noPackets === undefined ||
        this.settings.general.noPackets > 100000 || this.settings.general.noPackets < -1) {
        this.displayMessage({ msg: this.$t('parliament.settings.noPacketsErr'), type: 'danger' });
        return;
      }
      if (!this.settings.general.noPacketsLength || this.settings.general.noPacketsLength > 100000 ||
          this.settings.general.noPacketsLength < 1) {
        this.displayMessage({ msg: this.$t('parliament.settings.noPacketsLengthErr'), type: 'danger' });
        return;
      }
      if (!this.settings.general.outOfDate || this.settings.general.outOfDate > 3600) {
        this.displayMessage({ msg: this.$t('parliament.settings.outOfDateErr'), type: 'danger' });
        return;
      }
      if (!this.settings.general.esQueryTimeout || this.settings.general.esQueryTimeout > 60) {
        this.displayMessage({ msg: this.$t('parliament.settings.esQueryTimeoutErr'), type: 'danger' });
        return;
      }
      if (this.settings.general.lowDiskSpace === '' || this.settings.general.lowDiskSpace === undefined ||
        this.settings.general.lowDiskSpace < 0) {
        this.displayMessage({ msg: this.$t('parliament.settings.lowDiskSpaceErr'), type: 'danger' });
        return;
      }
      if (this.settings.general.lowDiskSpaceType === 'percentage' && this.settings.general.lowDiskSpace > 100) {
        this.displayMessage({ msg: this.$t('parliament.settings.lowDiskSpacePercentErr'), type: 'danger' });
        return;
      }
      if (this.settings.general.lowDiskSpaceType === 'gb' && this.settings.general.lowDiskSpace > 100000) {
        this.displayMessage({ msg: this.$t('parliament.settings.lowDiskSpaceGBErr'), type: 'danger' });
        return;
      }
      if (this.settings.general.lowDiskSpaceES === '' || this.settings.general.lowDiskSpaceES === undefined ||
        this.settings.general.lowDiskSpaceES < 0) {
        this.displayMessage({ msg: this.$t('parliament.settings.lowDiskSpaceESErr'), type: 'danger' });
        return;
      }
      if (this.settings.general.lowDiskSpaceESType === 'percentage' && this.settings.general.lowDiskSpaceES > 100) {
        this.displayMessage({ msg: this.$t('parliament.settings.lowDiskSpaceESPercentErr'), type: 'danger' });
        return;
      }
      if (this.settings.general.lowDiskSpaceESType === 'gb' && this.settings.general.lowDiskSpaceES > 100000) {
        this.displayMessage({ msg: this.$t('parliament.settings.lowDiskSpaceESGBErr'), type: 'danger' });
        return;
      }
      if (!this.settings.general.removeIssuesAfter || this.settings.general.removeIssuesAfter > 10080) {
        this.displayMessage({ msg: this.$t('parliament.settings.removeIssuesAfterErr'), type: 'danger' });
        return;
      }
      if (!this.settings.general.removeAcknowledgedAfter || this.settings.general.removeAcknowledgedAfter > 10080) {
        this.displayMessage({ msg: this.$t('parliament.settings.removeAcknowledgedAfterErr'), type: 'danger' });
        return;
      }

      SettingsService.saveSettings(this.settings).then((data) => {
        this.displayMessage({ msg: data.text || 'Saved your settings.', type: 'success' });
        this.clearMessage();
      }).catch((error) => {
        this.displayMessage({ msg: error || 'Error saving your settings.', type: 'danger' });
      });
    },
    getFieldInputType: function (field) {
      if (field.type === 'checkbox') {
        return 'checkbox';
      } else if (field.type === 'secret' && !field.showValue) {
        return 'password';
      } else {
        return 'text';
      }
    },
    debounceInput: function () {
      this.message = '';
      if (msgCloseTimeout) { clearTimeout(msgCloseTimeout); }
      if (inputDebounce) { clearTimeout(inputDebounce); }
      inputDebounce = setTimeout(() => {
        this.saveSettings();
      }, 500);
    },
    toggleVisibleSecretField: function (field) {
      field.showValue = !field.showValue;
    },
    restoreDefaults: function (type) {
      SettingsService.restoreDefaults(type).then((data) => {
        this.settings = data.settings;
        this.displayMessage({ msg: data.text || `Successfully restored ${type} default settings.`, type: 'success' });
        this.clearMessage();
      }).catch((error) => {
        this.displayMessage({ msg: error || `Error restoring ${type} default settings.`, type: 'danger' });
      });
    },
    displayMessage: function ({ msg, type }) {
      this.message = msg;
      this.msgType = type;
    },
    /* THEME --------------------------------------------------- */
    onThemeChange (newThemeId) {
      this.$store.commit('setTheme', newThemeId);
    },
    onCustomThemeChange (newCustomTheme) {
      if (!newCustomTheme || typeof newCustomTheme.colors !== 'object' || !newCustomTheme.colors) return;
      const safe = {
        dark: !!newCustomTheme.dark,
        colors: { ...newCustomTheme.colors }
      };
      registerVuetifyTheme(this.$vuetify, 'custom1', safe);
      this.$store.commit('setCustomTheme', safe);
      if (this.getTheme !== 'custom1') {
        this.$store.commit('setTheme', 'custom1');
      }
    },
    /* helper functions ---------------------------------------------------- */
    clearMessage: function (time) {
      if (msgCloseTimeout) { clearTimeout(msgCloseTimeout); }
      msgCloseTimeout = setTimeout(() => {
        this.message = '';
      }, time || 5000);
    }
  }
};
</script>

<style scoped>
/* Tighten the vertical tab strip to match viewer's settings nav. */
.v-tab {
  min-height: 28px !important;
  height: 28px !important;
  padding: 0 12px !important;
  font-size: 0.85rem !important;
  justify-content: flex-start !important;
}

/* General settings: keep each input visually paired with its helper
   text (very tight together), but give clear separation between
   groups. v-col padding-bottom carries the inter-group gap; the small
   element hugs its input so it's obvious which row it belongs to. */
.compact-form :deep(.v-row) {
  margin-top: 0 !important;
  margin-bottom: 0 !important;
  row-gap: 0 !important;
}
.compact-form :deep(.v-col),
.compact-form :deep([class*="v-col-"]) {
  padding-top: 0 !important;
  padding-bottom: 14px !important;
}
.compact-form small {
  margin-top: 2px !important;
  margin-bottom: 0 !important;
  line-height: 1.2;
  font-size: 0.78rem;
}

</style>
