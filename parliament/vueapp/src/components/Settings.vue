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
          If the problem persists, try
          <a
            class="no-decoration"
            href="javascript:void(0)"
            @click="restoreDefaults('all')">
            restoring them to the defaults
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
              General
            </a>
            <a
              class="nav-link cursor-pointer"
              @click="openView('notifiers')"
              :class="{'active':visibleTab === 'notifiers'}">
              <span class="fa fa-fw fa-bell" />&nbsp;
              Notifiers
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
                Reset Default Settings
              </button>
              General
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
                  Capture nodes must check in this often
                </span>
                <input
                  type="number"
                  class="form-control"
                  id="outOfDate"
                  @input="debounceInput"
                  v-model="settings.general.outOfDate"
                  max="3600">
                <span class="input-group-text">
                  seconds
                </span>
              </div>
              <p class="form-text small text-muted">
                We check in Elasticsearch if a capture node has checked in, and if not, an
                <strong>Out Of Date</strong>
                issue will be added to the node's cluster.
              </p>
            </div> <!-- /out of date -->
            <!-- es query timeout -->
            <div class="col-xl-9 col-lg-12 form-group">
              <div class="input-group">
                <span class="input-group-text">
                  Elasticsearch query timeout
                </span>
                <input
                  type="number"
                  class="form-control"
                  id="esQueryTimeout"
                  @input="debounceInput"
                  v-model="settings.general.esQueryTimeout"
                  max="60">
                <span class="input-group-text">
                  seconds
                </span>
              </div>
              <p class="form-text small text-muted">
                Aborts the queries and adds an
                <strong>ES Down</strong>
                issue if no response is received within the specified time.
              </p>
            </div> <!-- /es query timeout -->
            <!-- low packets -->
            <div class="col-xl-9 col-lg-12 form-group">
              <div class="d-flex">
                <div class="input-group me-2">
                  <span class="input-group-text">
                    Low Packets Threshold
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
                    If persisting for
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
                    seconds
                  </span>
                </div>
              </div>
              <p class="form-text small text-muted">
                Adds a
                <strong>Low Packets</strong>
                issue to the cluster if the capture node is receiving
                fewer packets than this value for the specified length of time.
                <strong>
                  Set this to -1 if you wish to ignore this issue.
                </strong>
              </p>
            </div> <!-- /low packets -->
            <!-- low disk space -->
            <div class="col-xl-9 col-lg-12 form-group">
              <div class="input-group">
                <span class="input-group-text">
                  Low Capture Node Disk space threshold
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
                    percent
                  </option>
                  <option value="gb">
                    GB
                  </option>
                </select>
              </div>
              <p class="form-text small text-muted">
                Adds a
                <strong>Low Capture Node Disk Space</strong>
                issue to the cluster if the capture node has free disk space at or below this percentage.
              </p>
            </div> <!-- /low disk space -->
            <!-- remove issues after -->
            <div class="col-xl-9 col-lg-12 form-group">
              <div class="input-group">
                <span class="input-group-text">
                  Remove all issues after
                </span>
                <input
                  type="number"
                  class="form-control"
                  id="removeIssuesAfter"
                  @input="debounceInput"
                  v-model="settings.general.removeIssuesAfter"
                  max="10080">
                <span class="input-group-text">
                  minutes
                </span>
              </div>
              <p class="form-text small text-muted">
                Removes issues that have not been seen again after the specified time.
              </p>
            </div> <!-- /remove issues after -->
            <!-- remove acknowledged issues after -->
            <div class="col-xl-9 col-lg-12 form-group">
              <div class="input-group">
                <span class="input-group-text">
                  Remove acknowledged issues after
                </span>
                <input
                  type="number"
                  class="form-control"
                  id="removeAcknowledgedAfter"
                  @input="debounceInput"
                  v-model="settings.general.removeAcknowledgedAfter"
                  max="10080">
                <span class="input-group-text">
                  minutes
                </span>
              </div>
              <p class="form-text small text-muted">
                Removes <strong>acknowledged</strong>
                issues that have not been seen again after the specified time.
              </p>
            </div> <!-- /remove acknowledged issues after -->
            <!-- wise url -->
            <div class="col-xl-9 col-lg-12 form-group">
              <div class="input-group">
                <span class="input-group-text">
                  WISE URL
                </span>
                <input
                  type="text"
                  class="form-control"
                  id="wiseUrl"
                  @input="debounceInput"
                  v-model="settings.general.wiseUrl">
              </div>
              <p class="form-text small text-muted">
                Add a button on the navbar to open WISE.
              </p>
            </div> <!-- /wise url -->
            <!-- cont3xt url -->
            <div class="col-xl-9 col-lg-12 form-group">
              <div class="input-group">
                <span class="input-group-text">
                  Cont3xt URL
                </span>
                <input
                  type="text"
                  class="form-control"
                  id="cont3xtUrl"
                  @input="debounceInput"
                  v-model="settings.general.cont3xtUrl">
              </div>
              <p class="form-text small text-muted">
                Add a button on the navbar to open Cont3xt.
              </p>
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
                  Parliament Hostname
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
                  &nbsp; include parliament dashboard url in notifications
                </span>
              </div>
              <p class="form-text small text-muted">
                Configure the Parliament's hostname to add a link to the Parliament Dashboard to every alert
              </p>
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
        return this.$store.state.parliament?.settings || { general: {} };
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
        this.displayMessage({ msg: 'Low packets threshold must contain a number between -1 and 100,000.', type: 'danger' });
        return;
      }
      if (!this.settings.general.noPacketsLength || this.settings.general.noPacketsLength > 100000 ||
          this.settings.general.noPacketsLength < 1) {
        this.displayMessage({ msg: 'Low packets time threshold must contain a number between 1 and 100,000.', type: 'danger' });
        return;
      }
      if (!this.settings.general.outOfDate || this.settings.general.outOfDate > 3600) {
        this.displayMessage({ msg: 'Capture node\'s checkin must contain a number less than or equal to 3600 seconds (1 hour)', type: 'danger' });
        return;
      }
      if (!this.settings.general.esQueryTimeout || this.settings.general.esQueryTimeout > 60) {
        this.displayMessage({ msg: 'Elasticsearch query timeout must contain a number less than or equal to 60 seconds', type: 'danger' });
        return;
      }
      if (this.settings.general.lowDiskSpace === '' || this.settings.general.lowDiskSpace === undefined ||
        this.settings.general.lowDiskSpace < 0) {
        this.displayMessage({ msg: 'Low disk space threshold must be a number greater than or equal to 0.', type: 'danger' });
        return;
      }
      if (this.settings.general.lowDiskSpaceType === 'percentage' && this.settings.general.lowDiskSpace > 100) {
        this.displayMessage({ msg: 'Low disk space threshold percentage must be between 0 and 100.', type: 'danger' });
        return;
      }
      if (this.settings.general.lowDiskSpaceType === 'gb' && this.settings.general.lowDiskSpace > 100000) {
        this.displayMessage({ msg: 'Low disk space threshold in GB must be between 0 and 100000.', type: 'danger' });
        return;
      }
      if (!this.settings.general.removeIssuesAfter || this.settings.general.removeIssuesAfter > 10080) {
        this.displayMessage({ msg: 'Remove all issues after must contain a number less than or equal to 10080 minutes (1 week)', type: 'danger' });
        return;
      }
      if (!this.settings.general.removeAcknowledgedAfter || this.settings.general.removeAcknowledgedAfter > 10080) {
        this.displayMessage({ msg: 'Remove acknowledged issues after must contain a number less than or equal to 10080 minutes (1 week)', type: 'danger' });
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
