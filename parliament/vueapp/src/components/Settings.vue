<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="settings-content mb-4">
    <div class="container-fluid">
      <!-- page error -->
      <div
        v-if="error"
        class="alert alert-danger">
        <span class="fa fa-exclamation-triangle" />&nbsp;
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
        <button
          type="button"
          class="close cursor-pointer"
          @click="error = ''">
          <span>&times;</span>
        </button>
      </div> <!-- /page error -->

      <!-- page content -->
      <div
        class="row"
        v-if="isAdmin">
        <!-- navigation -->
        <div class="col-xl-2 col-lg-3 col-md-3 col-sm-4">
          <div class="nav flex-column nav-pills">
            <a
              class="nav-link cursor-pointer"
              @click="openView('general')"
              :class="{'active':visibleTab === 'general'}">
              <span class="fa fa-fw fa-cog" />&nbsp;
              {{ $t('parliament.settings.general') }}
            </a>
            <a
              class="nav-link cursor-pointer"
              @click="openView('notifiers')"
              :class="{'active':visibleTab === 'notifiers'}">
              <span class="fa fa-fw fa-bell" />&nbsp;
              {{ $t('parliament.settings.notifiers') }}
            </a>
          </div>

          <!-- bottom fixed messages -->
          <b-alert
            :show="!!message"
            class="position-fixed fixed-bottom m-0 rounded-0"
            style="z-index: 2000;"
            :variant="msgType"
            dismissible>
            <span class="fa fa-check mr-2" />
            {{ message }}
          </b-alert>
          <!-- /bottom fixed messages -->
        </div> <!-- /navigation -->

        <!-- general -->
        <div
          v-if="visibleTab === 'general' && settings"
          class="col">
          <div class="row">
            <h3 class="col-xl-9 col-lg-12 form-group">
              <button
                type="button"
                class="btn btn-sm btn-outline-warning pull-right"
                @click="restoreDefaults('general')">
                {{ $t('parliament.settings.reset') }}
              </button>
              {{ $t('parliament.settings.general') }}
              <hr>
            </h3>
          </div>
          <div
            class="row"
            v-if="settings.general">
            <!-- out of date -->
            <div class="col-xl-9 col-lg-12 form-group">
              <div class="input-group">
                <span class="input-group-text">
                  {{ $t('parliament.settings.outOfDate') }}
                </span>
                <input
                  type="number"
                  class="form-control"
                  id="outOfDate"
                  @input="debounceInput"
                  v-model="settings.general.outOfDate"
                  max="3600">
                <span class="input-group-text">
                  {{ $t('common.seconds') }}
                </span>
              </div>
              <p
                class="form-text small text-muted"
                v-html="$t('parliament.settings.outOfDateHtml')" />
            </div> <!-- /out of date -->
            <!-- es query timeout -->
            <div class="col-xl-9 col-lg-12 form-group">
              <div class="input-group">
                <span class="input-group-text">
                  {{ $t('parliament.settings.esQueryTimeout') }}
                </span>
                <input
                  type="number"
                  class="form-control"
                  id="esQueryTimeout"
                  @input="debounceInput"
                  v-model="settings.general.esQueryTimeout"
                  max="60">
                <span class="input-group-text">
                  {{ $t('common.seconds') }}
                </span>
              </div>
              <p
                class="form-text small text-muted"
                v-html="$t('parliament.settings.esQueryTimeoutHtml')" />
            </div> <!-- /es query timeout -->
            <!-- low packets -->
            <div class="col-xl-9 col-lg-12 form-group">
              <div class="d-flex">
                <div class="input-group me-2">
                  <span class="input-group-text">
                    {{ $t('parliament.settings.noPackets') }}
                  </span>
                  <input
                    type="number"
                    class="form-control"
                    id="noPackets"
                    @input="debounceInput"
                    v-model="settings.general.noPackets"
                    max="100000"
                    min="-1">
                  <span class="input-group-text">
                    packets
                  </span>
                </div>
                <div class="input-group">
                  <span class="input-group-text">
                    {{ $t('parliament.settings.noPacketsLength') }}
                  </span>
                  <input
                    type="number"
                    class="form-control"
                    id="noPacketsLength"
                    @input="debounceInput"
                    v-model="settings.general.noPacketsLength"
                    max="100000"
                    min="1">
                  <span class="input-group-text">
                    {{ $t('common.seconds') }}
                  </span>
                </div>
              </div>
              <p
                class="form-text small text-muted"
                v-html="$t('parliament.settings.noPacketsHtml')" />
            </div> <!-- /low packets -->
            <!-- low disk space -->
            <div class="col-xl-9 col-lg-12 form-group">
              <div class="input-group">
                <span class="input-group-text">
                  {{ $t('parliament.settings.lowDiskSpace') }}
                </span>
                <input
                  type="number"
                  class="form-control"
                  id="lowDiskSpace"
                  @input="debounceInput"
                  v-model="settings.general.lowDiskSpace"
                  :max="settings.general.lowDiskSpaceType === 'percentage' ? 100 : 100000"
                  min="0"
                  :step="settings.general.lowDiskSpaceType === 'percentage' ? 0.1 : 1">
                <select
                  class="form-select"
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
              <p
                class="form-text small text-muted"
                v-html="$t('parliament.settings.lowDiskSpaceHtml')" />
            </div> <!-- /low disk space -->
            <!-- low disk space ES -->
            <div class="col-xl-9 col-lg-12 form-group">
              <div class="input-group">
                <span class="input-group-text">
                  {{ $t('parliament.settings.lowDiskSpaceES') }}
                </span>
                <input
                  type="number"
                  class="form-control"
                  id="lowDiskSpaceES"
                  @input="debounceInput"
                  v-model.number="settings.general.lowDiskSpaceES"
                  :max="settings.general.lowDiskSpaceESType === 'percentage' ? 100 : 100000"
                  min="0"
                  :step="settings.general.lowDiskSpaceESType === 'percentage' ? 0.1 : 1">
                <select
                  class="form-select"
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
              <p
                class="form-text small text-muted"
                v-html="$t('parliament.settings.lowDiskSpaceESHtml')" />
            </div> <!-- /low disk space ES -->
            <!-- remove issues after -->
            <div class="col-xl-9 col-lg-12 form-group">
              <div class="input-group">
                <span class="input-group-text">
                  {{ $t('parliament.settings.removeIssuesAfter') }}
                </span>
                <input
                  type="number"
                  class="form-control"
                  id="removeIssuesAfter"
                  @input="debounceInput"
                  v-model="settings.general.removeIssuesAfter"
                  max="10080">
                <span class="input-group-text">
                  {{ $t('common.minutes') }}
                </span>
              </div>
              <p
                class="form-text small text-muted"
                v-html="$t('parliament.settings.removeIssuesAfterHtml')" />
            </div> <!-- /remove issues after -->
            <!-- remove acknowledged issues after -->
            <div class="col-xl-9 col-lg-12 form-group">
              <div class="input-group">
                <span class="input-group-text">
                  {{ $t('parliament.settings.removeAcknowledgedAfter') }}
                </span>
                <input
                  type="number"
                  class="form-control"
                  id="removeAcknowledgedAfter"
                  @input="debounceInput"
                  v-model="settings.general.removeAcknowledgedAfter"
                  max="10080">
                <span class="input-group-text">
                  {{ $t('common.minutes') }}
                </span>
              </div>
              <p
                class="form-text small text-muted"
                v-html="$t('parliament.settings.removeAcknowledgedAfterHtml')" />
            </div> <!-- /remove acknowledged issues after -->
            <!-- wise url -->
            <div class="col-xl-9 col-lg-12 form-group">
              <div class="input-group">
                <span class="input-group-text">
                  {{ $t('parliament.settings.wiseUrl') }}
                </span>
                <input
                  type="text"
                  class="form-control"
                  id="wiseUrl"
                  @input="debounceInput"
                  v-model="settings.general.wiseUrl">
              </div>
              <p
                class="form-text small text-muted"
                v-html="$t('parliament.settings.wiseUrlHtml')" />
            </div> <!-- /wise url -->
            <!-- cont3xt url -->
            <div class="col-xl-9 col-lg-12 form-group">
              <div class="input-group">
                <span class="input-group-text">
                  {{ $t('parliament.settings.cont3xtUrl') }}
                </span>
                <input
                  type="text"
                  class="form-control"
                  id="cont3xtUrl"
                  @input="debounceInput"
                  v-model="settings.general.cont3xtUrl">
              </div>
              <p
                class="form-text small text-muted"
                v-html="$t('parliament.settings.cont3xtUrlHtml')" />
            </div> <!-- /cont3xt url -->
          </div>
        </div>
        <!-- /general -->

        <!-- notifiers tab -->
        <div
          v-if="visibleTab === 'notifiers' && settings"
          class="col">
          <!-- hostname -->
          <div class="row form-group">
            <div class="col-12">
              <div class="input-group">
                <span class="input-group-text">
                  {{ $t('parliament.settings.parliamentHostname') }}
                </span>
                <input
                  type="text"
                  class="form-control"
                  id="hostname"
                  @input="debounceInput"
                  v-model="settings.general.hostname">
                <span class="input-group-text">
                  <input
                    type="checkbox"
                    @input="debounceInput"
                    v-model="settings.general.includeUrl">
                  &nbsp; {{ $t('parliament.settings.includeDashboardUrl') }}
                </span>
              </div>
              <p
                class="form-text small text-muted"
                v-html="$t('parliament.settings.parliamentHostnameHtml')" />
            </div>
          </div> <!-- /hostname -->

          <hr>

          <Notifiers
            parent-app="parliament"
            @display-message="displayMessage"
            help-intl-id="settings.notifiers.helpParliament" />
        </div> <!-- /notifiers tab -->
      </div> <!-- /page content -->
    </div>
  </div>
</template>

<script>
import SettingsService from './settings.service.js';
import UserService from '@/components/user.service.js';
import Notifiers from '@common/Notifiers.vue';

let inputDebounce;
let msgCloseTimeout;

export default {
  name: 'Settings',
  components: { Notifiers },
  data: function () {
    return {
      message: '',
      msgType: 'success',
      // page error
      error: '',
      networkError: false,
      // default tab
      visibleTab: 'general'
    };
  },
  computed: {
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
      if (tab === 'general' || tab === 'notifiers') {
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
      this.msg = '';
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
        this.displayMessage({ msg: this.$t('parliament.settings.removeAcknowledgedAfter'), type: 'danger' });
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
      this.msg = '';
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
    /* helper functions ---------------------------------------------------- */
    clearMessage: function (time) {
      if (msgCloseTimeout) { clearTimeout(msgCloseTimeout); }
      msgCloseTimeout = setTimeout(() => {
        this.msg = '';
      }, time || 5000);
    }
  }
};
</script>

<style scoped>
.settings-content div.nav-pills {
  position: sticky;
  top: 70px;
}
</style>
