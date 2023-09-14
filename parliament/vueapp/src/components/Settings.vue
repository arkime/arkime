<template>

  <div class="settings-content mb-4">

    <div class="container-fluid">

      <!-- page error -->
      <div v-if="error"
        class="alert alert-danger">
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        {{ error }}
        <span v-if="!settings && !networkError">
          If the problem persists, try
          <a class="no-decoration"
            href="javascript:void(0)"
            @click="restoreDefaults('all')">
            restoring them to the defaults
          </a>
        </span>
        <button type="button"
          class="close cursor-pointer"
          @click="error = ''">
          <span>&times;</span>
        </button>
      </div> <!-- /page error -->

      <!-- page content -->
      <div class="row" v-if="isAdmin">

        <!-- navigation -->
        <div class="col-xl-2 col-lg-3 col-md-3 col-sm-4">

          <div class="nav flex-column nav-pills">
            <a class="nav-link cursor-pointer"
              @click="openView('general')"
              :class="{'active':visibleTab === 'general'}">
              <span class="fa fa-fw fa-cog">
              </span>&nbsp;
              General
            </a>
            <a class="nav-link cursor-pointer"
              @click="openView('notifiers')"
              :class="{'active':visibleTab === 'notifiers'}">
              <span class="fa fa-fw fa-bell">
              </span>&nbsp;
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
            <span class="fa fa-check mr-2"></span>
            {{ message }}
          </b-alert>
          <!-- /bottom fixed messages -->

        </div> <!-- /navigation -->

        <!-- general -->
        <div v-if="visibleTab === 'general' && settings"
          class="col">
          <div class="row">
            <h3 class="col-xl-9 col-lg-12 form-group">
              <button type="button"
                class="btn btn-sm btn-outline-warning pull-right"
                @click="restoreDefaults('general')"
                v-b-tooltip.hover.bottomleft
                title="Restore general settings to the original defaults">
                Reset Defaults
              </button>
              General
              <hr>
            </h3>
          </div>
          <div class="row"
            v-if="settings.general">
            <!-- out of date -->
            <div class="col-xl-9 col-lg-12 form-group">
              <div class="input-group">
                <span class="input-group-prepend">
                  <span class="input-group-text">
                    Capture nodes must check in this often
                  </span>
                </span>
                <input type="number"
                  class="form-control"
                  id="outOfDate"
                  @input="debounceInput"
                  v-model="settings.general.outOfDate"
                  max="3600"
                />
                <span class="input-group-append">
                  <span class="input-group-text">
                    seconds
                  </span>
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
                <span class="input-group-prepend">
                  <span class="input-group-text">
                    Elasticsearch query timeout
                  </span>
                </span>
                <input type="number"
                  class="form-control"
                  id="esQueryTimeout"
                  @input="debounceInput"
                  v-model="settings.general.esQueryTimeout"
                  max="60"
                />
                <span class="input-group-append">
                  <span class="input-group-text">
                    seconds
                  </span>
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
              <div class="row">
                <div class="col-8 input-group">
                  <span class="input-group-prepend">
                    <span class="input-group-text">
                      Low Packets Threshold
                    </span>
                  </span>
                  <input type="number"
                    class="form-control"
                    id="noPackets"
                    @input="debounceInput"
                    v-model="settings.general.noPackets"
                    max="100000"
                    min="-1"
                  />
                  <span class="input-group-append">
                    <span class="input-group-text">
                      packets
                    </span>
                  </span>
                </div>
                <div class="col-4 input-group">
                  <span class="input-group-prepend">
                    <span class="input-group-text">
                      If persisting for
                    </span>
                  </span>
                  <input type="number"
                    class="form-control"
                    id="noPacketsLength"
                    @input="debounceInput"
                    v-model="settings.general.noPacketsLength"
                    max="100000"
                    min="1"
                  />
                  <span class="input-group-append">
                    <span class="input-group-text">
                      seconds
                    </span>
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
            <!-- remove issues after -->
            <div class="col-xl-9 col-lg-12 form-group">
              <div class="input-group">
                <span class="input-group-prepend">
                  <span class="input-group-text">
                    Remove all issues after
                  </span>
                </span>
                <input type="number"
                  class="form-control"
                  id="removeIssuesAfter"
                  @input="debounceInput"
                  v-model="settings.general.removeIssuesAfter"
                  max="10080"
                />
                <span class="input-group-append">
                  <span class="input-group-text">
                    minutes
                  </span>
                </span>
              </div>
              <p class="form-text small text-muted">
                Removes issues that have not been seen again after the specified time.
              </p>
            </div> <!-- /remove issues after -->
            <!-- remove acknowledged issues after -->
            <div class="col-xl-9 col-lg-12 form-group">
              <div class="input-group">
                <span class="input-group-prepend">
                  <span class="input-group-text">
                    Remove acknowledged issues after
                  </span>
                </span>
                <input type="number"
                  class="form-control"
                  id="removeAcknowledgedAfter"
                  @input="debounceInput"
                  v-model="settings.general.removeAcknowledgedAfter"
                  max="10080"
                />
                <span class="input-group-append">
                  <span class="input-group-text">
                    minutes
                  </span>
                </span>
              </div>
              <p class="form-text small text-muted">
                Removes <strong>acknowledged</strong>
                issues that have not been seen again after the specified time.
              </p>
            </div> <!-- /remove acknowledged issues after -->
          </div>
        </div>
        <!-- /general -->

        <!-- notifiers tab -->
        <div v-if="visibleTab === 'notifiers' && settings" class="col">
          <!-- hostname -->
          <div class="row form-group">
            <div class="col-12">
              <div class="input-group">
                <span class="input-group-prepend">
                  <span class="input-group-text">
                    Parliament Hostname
                  </span>
                </span>
                <input type="text"
                  class="form-control"
                  id="hostname"
                  @input="debounceInput"
                  v-model="settings.general.hostname"
                />
                <span class="input-group-append">
                  <span class="input-group-text">
                    <input type="checkbox"
                      @input="debounceInput"
                      v-model="settings.general.includeUrl"
                    />
                    &nbsp; include parliament dashboard url in notifications
                  </span>
                </span>
              </div>
              <p class="form-text small text-muted">
                Configure the Parliament's hostname to add a link to the Parliament Dashbaord to every alert
              </p>
            </div>
          </div> <!-- /hostname -->

          <hr>

          <Notifiers
            parent-app="parliament"
            @display-message="displayMessage"
            help-text="Configure notifiers that can be used to alert on issues within your Parliament"
          />
        </div> <!-- /notifiers tab -->

      </div> <!-- /page content -->

    </div>

  </div>

</template>

<script>
import SettingsService from './settings.service';
import Notifiers from '../../../../common/vueapp/Notifiers';
import setReqHeaders from '../../../../common/vueapp/setReqHeaders';

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
    // does the url specify a tab in hash
    let tab = window.location.hash;
    if (tab) { // if there is a tab specified and it's a valid tab
      tab = tab.replace(/^#/, '');
      if (tab === 'general' || tab === 'notifiers') {
        this.visibleTab = tab;
      }
    }

    this.loadRoles();
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    openView: function (tabName) {
      if (this.visibleTab === tabName) { return; }

      this.visibleTab = tabName;
      this.$router.push({
        hash: tabName
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
        this.clearMesssage();
      }).catch((error) => {
        this.displayMessage({ msg: error.text || 'Error saving your settings.', type: 'danger' });
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
      this.$set(field, 'showValue', !field.showValue);
    },
    restoreDefaults: function (type) {
      SettingsService.restoreDefaults(type).then((data) => {
        this.settings = data.settings;
        this.displayMessage({ msg: data.text || `Successfully restored ${type} default settings.`, type: 'success' });
        this.clearMesssage();
      }).catch((error) => {
        this.displayMessage({ msg: error.text || `Error restoring ${type} default settings.`, type: 'danger' });
      });
    },
    displayMessage: function ({ msg, type }) {
      this.message = msg;
      this.msgType = type;
    },
    /* helper functions ---------------------------------------------------- */
    loadRoles: function () {
      fetch('api/user/roles', {
        method: 'GET',
        headers: setReqHeaders({ 'Content-Type': 'application/json' })
      }).then((response) => {
        return response.json();
      }).then((response) => {
        this.$store.commit('setRoles', response.roles || []);
      });
    },
    clearMesssage: function (time) {
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
