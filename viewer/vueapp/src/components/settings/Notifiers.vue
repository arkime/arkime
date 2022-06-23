<template>
  <div>

    <h3>
      Notifiers
      <template v-if="notifierTypes">
        <b-button
          size="sm"
          :key="notifier.name"
          class="pull-right ml-1"
          variant="theme-tertiary"
          v-for="notifier of notifierTypes"
          @click="createNewNotifier(notifier)">
          <span class="fa fa-plus-circle mr-1" />
          New {{ notifier.name }} Notifier
        </b-button>
      </template>
    </h3>

    <p>
      Configure notifiers that can be added to periodic queries and hunt jobs.
    </p>

    <hr>

    <!-- notifiers list error -->
    <b-alert
      :show="!!error"
      variant="danger"
      class="mt-2 mb-2">
      <span class="fa fa-exclamation-triangle mr-1" />
      {{ error }}
    </b-alert> <!-- /notifiers list error -->

    <!-- no results -->
    <div class="text-center mt-4"
      v-if="!notifiers || !Object.keys(notifiers).length">
      <h3>
        <span class="fa fa-folder-open fa-2x" />
      </h3>
      <h5>
        Create one by clicking one of the buttons above.
        <br>
        Then use it by adding it to your periodic queries or hunt jobs!
      </h5>
    </div> <!-- /no results -->

    <!-- new notifier -->
    <b-modal
      size="xl"
      id="create-notifier-modal"
      :title="`Create New ${newNotifier.type ? newNotifier.type.charAt(0).toUpperCase() + newNotifier.type.slice(1) : ''} Notifier`">
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
        <b-form-input
          v-model="newNotifier.name"
          placeholder="Notifier name (be specific)"
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
            :placeholder="field.description"
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
        <div class="col d-flex">
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
      <!-- create form error -->
      <b-alert
        variant="danger"
        class="mt-2 mb-0"
        :show="!!newNotifierError">
        <span class="fa fa-exclamation-triangle mr-1" />
        {{ newNotifierError }}
      </b-alert> <!-- /create form error -->
      <!-- new notifier actions -->
      <template #modal-footer>
        <div class="w-100 d-flex justify-content-between">
          <b-button
            title="Cancel"
            variant="danger"
            @click="$bvModal.hide('create-notifier-modal')">
            <span class="fa fa-times mr-1" />
            Cancel
          </b-button>
          <div>
            <b-button
              variant="warning"
              @click="clearNotifierFields">
              <span class="fa fa-ban mr-1" />
              Clear fields
            </b-button>
            <b-button
              variant="success"
              @click="createNotifier">
              <span class="fa fa-plus mr-1" />
              Create
            </b-button>
          </div>
        </div>
      </template> <!-- /new notifier actions -->
    </b-modal> <!-- new notifier -->

    <!-- notifiers -->
    <b-card-group
      columns
      class="mb-2"
      v-if="notifiers">
      <b-card
        :key="notifier.key"
        v-for="(notifier, index) of notifiers">
        <template #header>
          {{notifier.type.charAt(0).toUpperCase() + notifier.type.slice(1)}} Notifier
        </template>
        <b-card-text>
          <!-- notifier name -->
          <b-input-group
            size="sm"
            class="mb-2">
            <template #prepend>
              <b-input-group-text
                class="cursor-help"
                v-b-tooltip.hover.bottom-left
                title="Give your notifier a unique name">
                Name<sup>*</sup>
              </b-input-group-text>
            </template>
            <b-form-input
              v-model="notifier.name"
            />
          </b-input-group> <!-- /notifier name -->
          <!-- notifier fields -->
          <div v-for="field of notifier.fields"
            :key="field.name">
            <span class="mb-2"
              :class="{'input-group input-group-sm':field.type !== 'checkbox'}">
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
          <b-input-group
            size="sm"
            class="mb-2"
            prepend="Share with users">
            <b-form-input
              v-model="notifier.users"
              placeholder="comma separated list of userIds"
            />
          </b-input-group>
          <RoleDropdown
            :roles="roles"
            :id="notifier.id"
            :selected-roles="notifier.roles"
            @selected-roles-updated="updateNotifierRoles"
            :display-text="notifier.roles && notifier.roles.length ? undefined : 'Share with roles'"
          /> <!-- /notifier sharing -->
          <!-- notifier info -->
          <div class="row mt-2">
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
        </b-card-text>
        <!-- notifier actions -->
        <template #footer>
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
        </template> <!-- /notifier actions -->
      </b-card>
    </b-card-group> <!-- notifiers -->

  </div>
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
      newNotifier: {},
      notifierTypes: {},
      newNotifierError: ''
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
      this.$bvModal.show('create-notifier-modal');
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
        this.$bvModal.hide('create-notifier-modal');
        // add notifier to the list
        this.notifiers.push(response.notifier);
        // display success message to user
        let msg = response.text || 'Successfully created notifier.';
        if (response.invalidUsers && response.invalidUsers.length) {
          msg += ` Could not add these users: ${response.invalidUsers.join(',')}`;
        }
        this.$emit('display-message', { msg });
        this.newNotifier = {};
      }).catch((error) => {
        this.newNotifierError = error.text;
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
        this.$emit('display-message', { msg: response.text || 'Successfully deleted notifier.' });
      }).catch((error) => {
        this.$emit('display-message', { msg: error.text || 'Error deleting notifier.', type: 'danger' });
      });
    },
    /* updates a notifier */
    updateNotifier (id, index, notifier) {
      SettingsService.updateNotifier(id, notifier).then((response) => {
        this.notifiers.splice(index, 1, response.notifier);
        // display success message to user
        let msg = response.text || 'Successfully updated notifier.';
        if (response.invalidUsers && response.invalidUsers.length) {
          msg += ` Could not add these users: ${response.invalidUsers.join(',')}`;
        }
        this.$emit('display-message', { msg });
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
        this.$emit('display-message', { msg: response.text || 'Successfully tested notifier.' });
      }).catch((error) => {
        this.$set(this.notifiers[index], 'loading', false);
        this.$emit('display-message', { msg: error.text || 'Error testing notifier.', type: 'danger' });
      });
    }
  }
};
</script>
