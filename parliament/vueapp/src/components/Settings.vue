<template>

  <div>

    <div v-if="!dashboardOnly"
      class="container-fluid">

      <!-- page error -->
      <div v-if="error"
        class="alert alert-danger">
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        {{ error }}
        <span v-if="!settings && loggedIn && !networkError">
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

      <!-- password set but user is not logged in -->
      <div v-if="!error && hasAuth && !loggedIn"
        class="alert alert-danger">
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        This page requires admin privileges. Please login.
      </div> <!-- /password set but user is not logged in -->

      <!-- page content -->
      <div class="row">

        <!-- navigation -->
        <div v-if="hasAuth && loggedIn && settings"
          class="col-xl-2 col-lg-3 col-md-3 col-sm-4"
          role="tablist"
          aria-orientation="vertical">

          <div class="nav flex-column nav-pills">
            <a class="nav-link cursor-pointer"
              @click="openView('general')"
              :class="{'active':visibleTab === 'general'}">
              <span class="fa fa-fw fa-cog">
              </span>&nbsp;
              General
            </a>
            <a class="nav-link cursor-pointer"
              @click="openView('password')"
              :class="{'active':visibleTab === 'password'}">
              <span class="fa fa-fw fa-lock">
              </span>&nbsp;
              Password
            </a>
            <a class="nav-link cursor-pointer"
              @click="openView('notifiers')"
              :class="{'active':visibleTab === 'notifiers'}">
              <span class="fa fa-fw fa-bell">
              </span>&nbsp;
              Notifiers
            </a>
          </div>

          <!-- settings success -->
          <div v-if="success"
            class="alert alert-success mt-3">
            <button type="button"
              class="close cursor-pointer"
              @click="success = ''">
              <span>&times;</span>
            </button>
            <span class="fa fa-check">
            </span>&nbsp;
            {{ success }}
          </div> <!-- /settings success -->

          <!-- settings success -->
          <div v-if="settingsError"
            class="alert alert-danger mt-3">
            <button type="button"
              class="close cursor-pointer"
              @click="settingsError = ''">
              <span>&times;</span>
            </button>
            <span class="fa fa-exclamation-triangle">
            </span>&nbsp;
            {{ settingsError }}
          </div> <!-- /settings success -->

        </div> <!-- /navigation -->

        <!-- general -->
        <div v-if="visibleTab === 'general' && hasAuth && loggedIn && settings"
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

        <!-- password -->
        <div v-if="(visibleTab === 'password' && hasAuth && loggedIn) || (visibleTab === 'password' && !hasAuth)"
          class="col">
          <h3 class="mb-3">
            Password
            <span v-if="passwordChanged"
              class="pull-right">
              <!-- cancel password update button -->
              <a @click="cancelChangePassword"
                class="btn btn-outline-warning cursor-pointer">
                <span class="fa fa-ban">
                </span>&nbsp;
                Cancel
              </a> <!-- /cancel password update button -->
              <!-- update/create password button -->
              <a v-if="(hasAuth && loggedIn) || !hasAuth"
                @click="updatePassword"
                class="btn btn-outline-success cursor-pointer mr-1 ml-1">
                <span class="fa fa-key"></span>
                <span v-if="hasAuth && loggedIn">
                  Update
                </span>
                <span v-if="!hasAuth">
                  Create
                </span>
                Password
              </a> <!-- /update/create password button -->
            </span>
          </h3>
          <hr>
          <div v-if="hasAuth && loggedIn"
            class="input-group mb-2">
            <span class="input-group-prepend">
              <span class="input-group-text">
                Current Password
              </span>
            </span>
            <input class="form-control"
              @keyup.enter="updatePassword"
              name="currentPassword"
              @input="passwordChanged = true"
              v-model="currentPassword"
              type="password"
            />
          </div>
          <div class="input-group mb-2">
            <span class="input-group-prepend">
              <span class="input-group-text">
                New Password
              </span>
            </span>
            <input class="form-control"
              name="newPassword"
              @keyup.enter="updatePassword"
              @input="passwordChanged = true"
              v-model="newPassword"
              type="password"
            />
          </div>
          <div class="input-group mb-2">
            <span class="input-group-prepend">
              <span class="input-group-text">
                Confirm New Password
              </span>
            </span>
            <input class="form-control"
              name="newPasswordConfirm"
              @keyup.enter="updatePassword"
              @input="passwordChanged = true"
              v-model="newPasswordConfirm"
              type="password"
            />
          </div>
        </div> <!-- /password -->

        <!-- notifiers tab -->
        <div v-if="visibleTab === 'notifiers' && hasAuth && loggedIn && settings"
          class="col">
          <h3>
            Notifiers
          </h3>
          <hr>
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
          <!-- notifiers -->
          <div class="row"
            v-if="settings.notifiers">
            <div class="col-12 col-xl-6"
              v-for="notifier of settings.notifiers"
              :key="notifier.name">
              <div class="card bg-light mb-3">
                <div class="card-body">
                  <!-- notifier title -->
                  <h4 class="mb-3">
                    {{ notifier.name }}
                    <span v-if="!notifier.on"
                      @click="toggleNotifier(notifier)"
                      class="fa fa-toggle-off fa-lg pull-right cursor-pointer"
                      title="Turn this notifier on"
                      v-b-tooltip.hover.bottom-right>
                    </span>
                    <span v-if="notifier.on"
                      @click="toggleNotifier(notifier)"
                      class="fa fa-toggle-on fa-lg pull-right cursor-pointer text-success"
                      title="Turn this notifier off"
                      v-b-tooltip.hover.bottom-right>
                    </span>
                  </h4> <!-- /notifier title -->
                  <!-- notifier fields -->
                  <div v-for="field of notifier.fields"
                    :key="field.name">
                    <span class="mb-2"
                      :class="{'input-group':field.type !== 'checkbox'}">
                      <span class="input-group-prepend cursor-help"
                        v-if="field.type !== 'checkbox'"
                        :title="field.description"
                        v-b-tooltip.hover.bottom-left>
                        <span class="input-group-text">
                          {{ field.name }}
                          <sup v-if="field.required">*</sup>
                        </span>
                      </span>
                      <input :class="{'form-control':field.type !== 'checkbox'}"
                        v-model="field.value"
                        @input="debounceInput"
                        :type="getFieldInputType(field)"
                      />
                      <span v-if="field.type === 'secret'"
                        class="input-group-append cursor-pointer"
                        @click="toggleVisibleSecretField(field)">
                        <span class="input-group-text">
                          <span class="fa"
                            :class="{'fa-eye':field.type === 'secret' && !field.showValue, 'fa-eye-slash':field.type === 'secret' && field.showValue}">
                          </span>
                        </span>
                      </span>
                    </span>
                    <label v-if="field.type === 'checkbox'">
                      &nbsp;{{ field.name }}
                    </label>
                  </div> <!-- /notifier fields -->
                  <!-- notifier actions -->
                  <div class="row mt-3">
                    <div class="col-12">
                      <a class="btn btn-sm btn-outline-warning cursor-pointer"
                        @click="clearNotifierFields(notifier)">
                        <span class="fa fa-ban">
                        </span>&nbsp;
                        Clear fields
                      </a>
                      <a class="btn btn-sm btn-outline-success cursor-pointer pull-right"
                        @click="testNotifier(notifier)">
                        <span class="fa fa-bell">
                        </span>&nbsp;
                        Test Notifier
                      </a>
                    </div>
                  </div> <!-- /notifier actions -->
                  <hr>
                  <!-- notifier alerts -->
                  <h5>Notify on</h5>
                  <div class="row">
                    <div class="col-12">
                      <div v-for="alert of notifier.alerts"
                        :key="alert.name"
                        class="form-check form-check-inline"
                        :title="`Notify if ${alert.description}`"
                        v-b-tooltip.hover.top>
                        <label class="form-check-label">
                          <input class="form-check-input"
                            type="checkbox"
                            @input="updateAlert(alert)"
                            :id="alert.id"
                            :name="alert.id"
                            v-model="alert.on"
                          />
                          {{ alert.name }}
                        </label>
                      </div>
                    </div>
                  </div> <!-- /notifier alerts -->
                </div>
              </div>
            </div>
          </div> <!-- notifiers -->
        </div> <!-- /notifiers tab -->

      </div> <!-- /page content -->

    </div>

    <div v-else
      class="container-fluid">
      <div class="alert alert-danger">
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        This Parliament is for display only! You shouldn't be here.
      </div>
    </div>

  </div>

</template>

<script>
import AuthService from '../auth';
import SettingsService from './settings.service';

let initialized;
let inputDebounce;
let successCloseTimeout;

export default {
  name: 'Settings',
  data: function () {
    return {
      // page error
      error: '',
      networkError: false,
      // page success message
      success: '',
      // default tab
      visibleTab: 'general',
      // page data
      settings: undefined,
      // settings error
      settingsError: '',
      // password settings
      currentPassword: '',
      newPassword: '',
      newPasswordConfirm: '',
      passwordChanged: false
    };
  },
  computed: {
    // auth vars
    hasAuth: function () {
      return this.$store.state.hasAuth;
    },
    loggedIn: function () {
      return this.$store.state.loggedIn;
    },
    dashboardOnly: function () {
      return this.$store.state.dashboardOnly;
    }
  },
  watch: {
    loggedIn: function (newVal) {
      if (newVal && initialized) { this.loadData(); }
    }
  },
  mounted: function () {
    // does the url specify a tab in hash
    let tab = window.location.hash;
    if (tab) { // if there is a tab specified and it's a valid tab
      tab = tab.replace(/^#/, '');
      if (tab === 'general' || tab === 'notifiers' || tab === 'password') {
        this.visibleTab = tab;
      }
    }

    this.loadData();
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    openView: function (tabName) {
      this.visibleTab = tabName;
      this.$router.push({
        hash: tabName
      });
    },
    updateAlert: function (alert) {
      alert.on = !alert.on;
      this.saveSettings();
    },
    saveSettings: function () {
      this.success = '';
      if (successCloseTimeout) { clearTimeout(successCloseTimeout); }

      if (this.settings.general.noPackets === '' || this.settings.general.noPackets === undefined ||
        this.settings.general.noPackets > 100000 || this.settings.general.noPackets < -1) {
        this.settingsError = 'Low packets threshold must contain a number between -1 and 100,000.';
        return;
      }
      if (!this.settings.general.noPacketsLength || this.settings.general.noPacketsLength > 100000 ||
          this.settings.general.noPacketsLength < 1) {
        this.settingsError = 'Low packets time threshold must contain a number between 1 and 100,000.';
        return;
      }
      if (!this.settings.general.outOfDate || this.settings.general.outOfDate > 3600) {
        this.settingsError = 'Capture node\'s checkin must contain a number less than or equal to 3600 seconds (1 hour)';
        return;
      }
      if (!this.settings.general.esQueryTimeout || this.settings.general.esQueryTimeout > 60) {
        this.settingsError = 'Elasticsearch query timeout must contain a number less than or equal to 60 seconds';
        return;
      }
      if (!this.settings.general.removeIssuesAfter || this.settings.general.removeIssuesAfter > 10080) {
        this.settingsError = 'Remove all issues after must contain a number less than or equal to 10080 minutes (1 week)';
        return;
      }
      if (!this.settings.general.removeAcknowledgedAfter || this.settings.general.removeAcknowledgedAfter > 10080) {
        this.settingsError = 'Remove acknowledged issues after must contain a number less than or equal to 10080 minutes (1 week)';
        return;
      }

      SettingsService.saveSettings(this.settings)
        .then((data) => {
          this.settingsError = '';
          this.success = data.text || 'Saved your settings.';
          this.closeSuccess();
        })
        .catch((error) => {
          this.settingsError = error.text || 'Error saving settings.';
        });
    },
    toggleNotifier: function (notifier) {
      this.$set(notifier, 'on', !notifier.on);
      this.saveSettings();
    },
    testNotifier: function (notifier) {
      SettingsService.testNotifier(notifier.name)
        .then((data) => {
          this.settingsError = '';
          this.success = data.text || 'Successfully issued alert.';
          this.closeSuccess();
        })
        .catch((error) => {
          this.settingsError = error.text || 'Error issuing alert.';
        });
    },
    clearNotifierFields: function (notifier) {
      for (let f in notifier.fields) {
        notifier.fields[f].value = '';
      }
      this.saveSettings();
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
    cancelChangePassword: function () {
      this.currentPassword = '';
      this.newPassword = '';
      this.newPasswordConfirm = '';
      this.passwordChanged = false;
    },
    updatePassword: function () {
      this.settingsError = '';

      if (!this.currentPassword && this.hasAuth) {
        this.settingsError = 'You must provide your current password.';
      }

      if (!this.newPassword) {
        this.settingsError = 'You must provide a new password.';
        return;
      }

      if (!this.newPasswordConfirm) {
        this.settingsError = 'You must confirm your new password.';
        return;
      }

      if (this.newPassword !== this.newPasswordConfirm) {
        this.settingsError = 'Passwords must match.';
        this.newPassword = '';
        this.newPasswordConfirm = '';
        return;
      }

      let success = 'Password successfully ';
      success += this.hasAuth ? 'updated' : 'created';

      AuthService.updatePassword(this.currentPassword, this.newPassword)
        .then((response) => {
          this.settingsError = '';
          this.success = success;
          this.closeSuccess();
          this.cancelChangePassword();
        })
        .catch((error) => {
          this.settingsError = error.text || 'Error saving password.';
          this.cancelChangePassword();
        });
    },
    debounceInput: function () {
      this.success = '';
      if (successCloseTimeout) { clearTimeout(successCloseTimeout); }
      if (inputDebounce) { clearTimeout(inputDebounce); }
      inputDebounce = setTimeout(() => {
        this.saveSettings();
      }, 500);
    },
    toggleVisibleSecretField: function (field) {
      this.$set(field, 'showValue', !field.showValue);
    },
    restoreDefaults: function (type) {
      SettingsService.restoreDefaults(type)
        .then((data) => {
          this.settingsError = '';
          this.settings = data.settings;
          this.success = data.text || `Successfully restored ${type} default settings.`;
          this.closeSuccess();
        })
        .catch((error) => {
          this.settingsError = error.text || `Error restoring ${type} default settings.`;
        });
    },
    /* helper functions ---------------------------------------------------- */
    loadData: function () {
      this.error = '';
      this.settingsError = '';

      SettingsService.getSettings()
        .then((data) => {
          initialized = true;
          this.error = '';
          this.settings = data;
        })
        .catch((error) => {
          initialized = true;
          if (this.hasAuth) {
            this.error = error.text || 'Error fetching settings.';
            this.networkError = error.networkError;
          } else {
            this.error = 'No password set for your Parliament. Set a password so you can do more stuff!';
            this.openView('password'); // redirect the user to possibly create a password
          }
        });
    },
    closeSuccess: function (time) {
      if (successCloseTimeout) { clearTimeout(successCloseTimeout); }
      successCloseTimeout = setTimeout(() => {
        this.success = '';
      }, time || 5000);
    }
  }
};
</script>
