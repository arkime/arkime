<template>
  <form class="form-horizontal"
    v-has-role="{user:user,roles:'arkimeAdmin'}"
    id="notifiers">

    <h3>
      Notifiers
      <template v-if="notifierTypes">
        <button v-for="notifier of notifierTypes"
          :key="notifier.name"
          class="btn btn-theme-tertiary btn-sm pull-right ml-1"
          type="button"
          @click="createNewNotifier(notifier)">
          <span class="fa fa-plus-circle">
          </span>&nbsp;
          Create {{ notifier.name }} Notifier
        </button>
      </template>
    </h3>

    <p>
      Configure notifiers that can be added to cron queries and hunt jobs.
    </p>

    <hr>

    <div v-if="!notifiers || !Object.keys(notifiers).length"
      class="alert alert-info">
      <span class="fa fa-info-circle fa-lg">
      </span>
      <strong>
        You have no notifiers configured.
      </strong>
      <br>
      <br>
      Create one by clicking one of the create buttons above.
      Then use it by adding it to your cron queries or hunt jobs!
    </div>

    <div v-if="error"
      class="alert alert-danger">
      <span class="fa fa-exclamation-triangle mr-2" />
      {{ error }}
    </div>

    <!-- new notifier -->
    <div class="row"
      v-if="newNotifier">
      <div class="col">
        <div class="card mb-3">
          <div class="card-body">
            <!-- newNotifier title -->
            <h4 class="mb-3">
              Create new {{ newNotifier.type }} notifier
              <span v-if="newNotifierError"
                class="alert alert-sm alert-danger pull-right pr-2">
                {{ newNotifierError }}
                <span class="fa fa-close cursor-pointer"
                  @click="newNotifierError = ''">
                </span>
              </span>
            </h4> <!-- /new notifier title -->
            <!-- new notifier name -->
            <div class="input-group">
              <span class="input-group-prepend cursor-help"
                :title="`Give your ${newNotifier.type} notifier a unique name`"
                v-b-tooltip.hover.bottom-left>
                <span class="input-group-text">
                  Name
                  <sup>*</sup>
                </span>
              </span>
              <input class="form-control"
                v-model="newNotifier.name"
                type="text"
              />
            </div>
            <small class="form-text text-muted mb-2 mt-0">
              Be specific! There can be multiple {{ newNotifier.type }}
              notifiers.
            </small> <!-- /new notifier name -->
            <!-- new notifier fields -->
            <div v-for="field of newNotifier.fields"
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
            </div> <!-- /new notifier fields -->
            <!-- new notifier sharing -->
            <div class="form-group row">
              <label class="col-2 col-form-label">
                Sharing
              </label>
              <div class="col-10 d-flex">
                <div>
                  <RoleDropdown
                    :roles="roles"
                    display-text="Share with roles"
                    :selected-roles="newNotifier.roles"
                    @selected-roles-updated="updateNewNotifierRoles"
                  />
                </div>
                <div class="ml-2 flex-grow-1">
                  <b-input-group
                    size="sm"
                    prepend="Share with users">
                    <b-form-input
                      v-model="newNotifier.users"
                      placeholder="comma separated list of userIds"
                    />
                  </b-input-group>
                </div>
              </div>
            </div> <!-- /new notifier sharing -->
            <!-- new notifier actions -->
            <div class="row mt-3">
              <div class="col-12">
                <button type="button"
                  class="btn btn-sm btn-outline-warning cursor-pointer"
                  @click="clearNotifierFields">
                  Clear fields
                </button>
                <button type="button"
                  class="btn btn-sm btn-success cursor-pointer pull-right ml-1"
                  @click="createNotifier">
                  <span class="fa fa-plus">
                  </span>&nbsp;
                  Create {{ newNotifier.type }} Notifier
                </button>
                <button type="button"
                  class="btn btn-sm btn-warning cursor-pointer pull-right"
                  @click="newNotifier = undefined">
                  <span class="fa fa-ban">
                  </span>&nbsp;
                  Cancel
                </button>
              </div>
            </div> <!-- /new notifier actions -->
          </div>
        </div>
      </div>
    </div> <!-- new notifier -->

    <!-- notifiers -->
    <div class="row"
      v-if="notifiers">
      <div class="col-12 col-xl-6"
        v-for="(notifier, index) of notifiers"
        :key="notifier.key">
        <div class="card mb-3">
          <div class="card-body">
            <!-- notifier title -->
            <h4 class="mb-3">
              {{ notifier.type }} Notifier
            </h4> <!-- /notifier title -->
            <!-- notifier name -->
            <div class="input-group mb-2">
              <span class="input-group-prepend cursor-help"
                :title="`Give your notifier a unique name`"
                v-b-tooltip.hover.bottom-left>
                <span class="input-group-text">
                  Name
                  <sup>*</sup>
                </span>
              </span>
              <input class="form-control"
                v-model="notifier.name"
                type="text"
              />
            </div> <!-- /notifier name -->
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
            <!-- notifier sharing -->
            <div class="form-group row">
              <label class="col-2 col-form-label">
                Sharing
              </label>
              <div class="col-10 d-flex">
                <div>
                  <RoleDropdown
                    :roles="roles"
                    :id="notifier.id"
                    :selected-roles="notifier.roles"
                    @selected-roles-updated="updateNotifierRoles"
                    :display-text="notifier.roles && notifier.roles.length ? undefined : 'Share with roles'"
                  />
                </div>
                <div class="ml-2 flex-grow-1">
                  <b-input-group
                    size="sm"
                    prepend="Share with users">
                    <b-form-input
                      v-model="notifier.users"
                      placeholder="comma separated list of userIds"
                    />
                  </b-input-group>
                </div>
              </div>
            </div> <!-- /notifier sharing -->
            <!-- notifier info -->
            <div class="row">
              <div class="col-12 small">
                <span v-if="notifier.created || notifier.user">
                  Created by {{ notifier.user }} at
                  {{ notifier.created * 1000 | timezoneDateString(user.settings.timezone, false) }}
                </span>
                <span v-if="notifier.updated"
                  class="pull-right">
                  Last updated at {{ notifier.updated * 1000 | timezoneDateString(user.settings.timezone, false) }}
                </span>
              </div>
            </div> <!-- /notifier info -->
            <!-- notifier actions -->
            <div class="row mt-3">
              <div class="col-12">
                <button type="button"
                  :disabled="notifier.loading"
                  class="btn btn-sm btn-outline-warning cursor-pointer"
                  @click="testNotifier(notifier.id, index)">
                  <span v-if="notifier.loading"
                    class="fa fa-spinner fa-spin">
                  </span>
                  <span v-else class="fa fa-bell">
                  </span>&nbsp;
                  Test
                </button>
                <button type="button"
                  class="btn btn-sm btn-success cursor-pointer pull-right ml-1"
                  @click="updateNotifier(notifier.id, index, notifier)">
                  <span class="fa fa-save">
                  </span>&nbsp;
                  Save
                </button>
                <button type="button"
                  class="btn btn-sm btn-danger cursor-pointer pull-right"
                  @click="removeNotifier(notifier.id, index)">
                  <span class="fa fa-trash-o">
                  </span>&nbsp;
                  Delete
                </button>
              </div>
            </div> <!-- /notifier actions -->
          </div>
        </div>
      </div>
    </div> <!-- notifiers -->

  </form>
</template>

<script>
// services
import SettingsService from './SettingsService';
// components
import RoleDropdown from '../../../../../common/vueapp/RoleDropdown';

export default {
  name: 'Notifiers',
  components: {
    RoleDropdown
  },
  data () {
    return {
      error: '',
      notifierTypes: {},
      newNotifierError: '',
      newNotifier: undefined
    };
  },
  computed: {
    notifiers: {
      get () { return this.$store.state.notifiers || []; },
      set (notifiers) { this.$store.commit('setNotifiers', notifiers); }
    },
    user () {
      return this.$store.state.user;
    },
    roles () {
      return this.$store.state.roles;
    }
  },
  mounted () {
    // retrieves the types of notifiers that can be configured (slack, email, twilio)
    SettingsService.getNotifierTypes().then((response) => {
      this.notifierTypes = response;
    }).catch((error) => {
      this.error = error.text || error;
    });
  },
  methods: {
    /* opens the form to create a new notifier */
    createNewNotifier (notifier) {
      this.newNotifier = JSON.parse(JSON.stringify(notifier));
      this.newNotifier.roles = ['arkimeUser', 'parliamentUser'];
      this.newNotifier.users = '';
    },
    /* updates the roles on the new notifier object from the RoleDropdown component */
    updateNewNotifierRoles (roles) {
      this.$set(this.newNotifier, 'roles', roles);
    },
    /* updates the roles on a notifier object from the RoleDropdown component */
    updateNotifierRoles (roles, id) {
      for (const notifier of this.notifiers) {
        if (notifier.id === id) {
          this.$set(notifier, 'roles', roles);
          return;
        }
      }
    },
    /* gets the type of input associated with a field */
    getFieldInputType (field) {
      if (field.type === 'checkbox') {
        return 'checkbox';
      } else if (field.type === 'secret' && !field.showValue) {
        return 'password';
      } else {
        return 'text';
      }
    },
    /* clears the fields of the new notifier form */
    clearNotifierFields () {
      this.newNotifier.name = '';
      for (const field of this.newNotifier.fields) {
        field.value = '';
      }
    },
    /* creates a new notifier */
    createNotifier: function () {
      if (!this.newNotifier) {
        this.newNotifierError = 'No notifier chosen';
        return;
      }

      if (!this.newNotifier.name) {
        this.newNotifierError = 'Your new notifier must have a unique name';
        return;
      }

      // make sure required fields are filled
      for (const field of this.newNotifier.fields) {
        if (!field.value && field.required) {
          this.newNotifierError = `${field.name} is required`;
          return;
        }
      }

      SettingsService.createNotifier(this.newNotifier).then((response) => {
        // add notifier to the list
        this.notifiers.push(response.notifier);
        this.newNotifier = undefined;
        // display success message to user
        this.$emit('display-message', { msg: response.text || 'Successfully created notifier.', type: 'success' });
      }).catch((error) => {
        this.$emit('display-message', { msg: error.text || 'Error creating notifier.', type: 'danger' });
      });
    },
    /* toggles the visibility of the value of secret fields */
    toggleVisibleSecretField (field) {
      this.$set(field, 'showValue', !field.showValue);
    },
    /* deletes a notifier */
    removeNotifier (id, index) {
      SettingsService.deleteNotifier(id).then((response) => {
        this.notifiers.splice(index, 1);
        // display success message to user
        this.$emit('display-message', { msg: response.text || 'Successfully deleted notifier.', type: 'success' });
      }).catch((error) => {
        this.$emit('display-message', { msg: error.text || 'Error deleting notifier.', type: 'danger' });
      });
    },
    /* updates a notifier */
    updateNotifier (id, index, notifier) {
      SettingsService.updateNotifier(id, notifier).then((response) => {
        this.notifiers.splice(index, 1, response.notifier);
        // display success message to user
        this.$emit('display-message', { msg: response.text || 'Successfully updated notifier.', type: 'success' });
      }).catch((error) => {
        this.$emit('display-message', { msg: error.text || 'Error updating notifier.', type: 'danger' });
      });
    },
    /* tests a notifier */
    testNotifier (id, index) {
      if (this.notifiers[index].loading) {
        return;
      }

      this.$set(this.notifiers[index], 'loading', true);

      SettingsService.testNotifier(id).then((response) => {
        this.$set(this.notifiers[index], 'loading', false);
        // display success message to user
        this.$emit('display-message', { msg: response.text || 'Successfully tested notifier.', type: 'success' });
      }).catch((error) => {
        this.$set(this.notifiers[index], 'loading', false);
        this.$emit('display-message', { msg: error.text || 'Error testing notifier.', type: 'danger' });
      });
    }
  }
};
</script>
