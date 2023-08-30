<template>
  <div>

    <h3>
      Notifiers
      <template v-if="notifierTypes">
        <b-button
          size="sm"
          variant="primary"
          :key="notifier.name"
          class="pull-right ml-1"
          v-for="notifier of notifierTypes"
          @click="createNewNotifier(notifier)">
          <span class="fa fa-plus-circle mr-1" />
          New {{ notifier.name }} Notifier
        </b-button>
      </template>
    </h3>

    <p class="lead">{{ helpText }}</p>

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
        <span class="fa fa-folder-open fa-3x text-muted" />
      </h3>
      <h5 class="lead">
        Create one by clicking one of the buttons above.
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
          <span
            v-if="parentApp === 'parliament'"
            @click="toggleNotifier(notifier, index)"
            :class="{'fa-toggle-on text-success':notifier.on,'fa-toggle-off':!notifier.on}"
            class="fa fa-lg pull-right cursor-pointer"
            :title="`Turn this notifier ${notifier.on ? 'off' : 'on'}`"
            v-b-tooltip.hover.bottom-right>
          </span>
        </template>
        <b-card-text>
          MY NOTIFIER {{ notifier.name }}
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
          <template v-if="parentApp === 'parliament' && notifier.alerts">
            <hr>
            <!-- notifier alerts -->
            <h5>Notify on</h5>
            <div class="row">
              <div class="col-12">
                <div v-for="(alert, aKey) of notifier.alerts"
                  :key="aKey"
                  class="form-check form-check-inline"
                  :title="`Notify if ${notifierTypes[notifier.type].alerts[aKey].description}`"
                  v-b-tooltip.hover.top>
                  <label class="form-check-label">
                    <input class="form-check-input"
                      type="checkbox"
                      :id="notifierTypes[notifier.type].alerts[aKey].name+notifier.name"
                      :name="notifierTypes[notifier.type].alerts[aKey].name+notifier.name"
                      v-model="notifier.alerts[aKey]"
                    />
                    {{ notifierTypes[notifier.type].alerts[aKey].name }}
                  </label>
                </div>
              </div>
            </div>
          </template> <!-- /notifier alerts -->
          <!-- notifier info -->
          <div class="row mt-2">
            <div class="col-12 small">
              <p v-if="notifier.created || notifier.user" class="m-0">
                Created by {{ notifier.user }} at
                {{ notifier.created * 1000 | timezoneDateString(tz, false) }}
              </p>
              <p v-if="notifier.updated" class="m-0">
                Last updated at {{ notifier.updated * 1000 | timezoneDateString(tz, false) }}
              </p>
            </div>
          </div> <!-- /notifier info -->
        </b-card-text>
        <!-- notifier actions -->
        <template #footer>
          <b-button
            size="sm"
            variant="outline-warning"
            :disabled="notifier.loading"
            @click="testNotifier(notifier.id, index)">
            <span v-if="notifier.loading"
              class="fa fa-spinner fa-spin fa-fw mr-1"
            />
            <span v-else class="fa fa-bell fa-fw mr-1" />
            Test
          </b-button>
          <span class="pull-right">
            <b-button
              size="sm"
              variant="danger"
              @click="removeNotifier(notifier.id, index)">
              <span class="fa fa-trash-o fa-fw mr-1" />
              Delete
            </b-button>
            <b-button
              size="sm"
              variant="success"
              @click="updateNotifier(notifier.id, index, notifier)">
              <span class="fa fa-save fa-fw mr-1" />
              Save
            </b-button>
          </span>
        </template> <!-- /notifier actions -->
      </b-card>
    </b-card-group> <!-- notifiers -->

  </div>
</template>

<script>
import setReqHeaders from './setReqHeaders';
import RoleDropdown from './RoleDropdown';

export default {
  name: 'Notifiers',
  components: { RoleDropdown },
  props: {
    helpText: {
      type: String,
      required: true
    },
    parentApp: {
      type: String,
      required: true
    }
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
    roles () {
      return this.$store.state.roles;
    },
    tz () {
      return this.$store.state.user?.settings?.timezone || 'local';
    }
  },
  mounted () {
    // retrieves the types of notifiers that can be configured (slack, email, twilio)
    fetch('api/notifierTypes', {
      method: 'GET',
      headers: setReqHeaders({ 'Content-Type': 'application/json' })
    }).then((response) => {
      return response.json();
    }).then((response) => {
      this.notifierTypes = response;
    }).catch((error) => {
      this.error = error.text || error;
    });

    // if notifiers haven't been fetched by the parent application, fetch them now
    if (!this.$store.state.notifiers?.length) {
      fetch('api/notifiers', {
        method: 'GET',
        headers: setReqHeaders({ 'Content-Type': 'application/json' })
      }).then((response) => {
        return response.json();
      }).then((response) => {
        this.$store.commit('setNotifiers', response);
      }).catch((error) => {
        this.error = error.text || error;
      });
    }
  },
  methods: {
    /* opens the form to create a new notifier */
    createNewNotifier (notifier) {
      const clone = JSON.parse(JSON.stringify(notifier));
      // flatten notifer alerts (we only need on state)
      for (const a in clone.alerts) {
        clone.alerts[a] = false; // off by default
      }
      clone.roles = ['arkimeUser', 'parliamentUser'];
      clone.users = '';
      this.newNotifier = clone;
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

      fetch('api/notifier', {
        method: 'POST',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
        body: JSON.stringify(this.newNotifier)
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (!response.success) {
          this.newNotifierError = response.text;
          return;
        }

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
      fetch(`api/notifier/${id}`, {
        method: 'DELETE',
        headers: setReqHeaders({ 'Content-Type': 'application/json' })
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (!response.success) {
          this.$emit('display-message', { msg: response.text || 'Error deleting notifier.', type: 'danger' });
          return;
        }

        this.notifiers.splice(index, 1);
        // display success message to user
        this.$emit('display-message', { msg: response.text || 'Successfully deleted notifier.' });
      }).catch((error) => {
        this.$emit('display-message', { msg: error.text || 'Error deleting notifier.', type: 'danger' });
      });
    },
    /* updates a notifier */
    updateNotifier (id, index, notifier) {
      fetch(`api/notifier/${id}`, {
        method: 'PUT',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
        body: JSON.stringify(notifier)
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (!response.success) {
          this.$emit('display-message', { msg: response.text || 'Error updating notifier.', type: 'danger' });
          return;
        }

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

      fetch(`api/notifier/${id}/test`, {
        method: 'POST',
        headers: setReqHeaders({})
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (!response.success) {
          this.$emit('display-message', { msg: response.text || 'Error testing notifier.', type: 'danger' });
          return;
        }

        this.$set(this.notifiers[index], 'loading', false);
        // display success message to user
        this.$emit('display-message', { msg: response.text || 'Successfully tested notifier.' });
        this.newNotifier = {};
      }).catch((error) => {
        this.$set(this.notifiers[index], 'loading', false);
        this.$emit('display-message', { msg: error.text || 'Error testing notifier.', type: 'danger' });
      });
    },
    /* toggles a notifier on/off (parliament use only) */
    toggleNotifier: function (notifier, index) {
      this.$set(notifier, 'on', !notifier.on);
      this.updateNotifier(notifier.id, index, notifier);
    }
  }
};
</script>
