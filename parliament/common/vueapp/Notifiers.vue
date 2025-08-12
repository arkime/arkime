<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>

    <h3>
      Notifiers
      <template v-if="notifierTypes">
        <b-button
          size="sm"
          variant="primary"
          :key="notifier.name"
          class="pull-right ms-1"
          v-for="notifier of notifierTypes"
          @click="createNewNotifier(notifier)">
          <span class="fa fa-plus-circle me-1" />
          New {{ notifier.name }} Notifier
        </b-button>
      </template>
    </h3>

    <p class="lead">{{ helpText }}</p>

    <hr>

    <!-- notifiers list error -->
    <div
      v-if="error"
      class="alert alert-danger mt-2 mb-2">
      <span class="fa fa-exclamation-triangle me-1" />
      {{ error }}
  </div> <!-- /notifiers list error -->

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
      :model-value="showNotifierModal"
      :title="`Create New ${newNotifier.type ? newNotifier.type.charAt(0).toUpperCase() + newNotifier.type.slice(1) : ''} Notifier`">
      <!-- new notifier name -->
      <BInputGroup size="sm" class="mb-2">
        <BInputGroupText
          id="newNotifierName"
          class="cursor-help">
          Name
          <sup>*</sup>
          <BTooltip target="newNotifierName">
            Give your {{newNotifier.type}} notifier a unique name
          </BTooltip>
        </BInputGroupText>
        <input
          class="form-control"
          v-model="newNotifier.name"
          placeholder="Notifier name (be specific)"
        />
        <BInputGroupText class="cursor-help" id="newNotifierNameHelp">
          <span class="fa fa-info-circle" />
          <BTooltip target="newNotifierNameHelp">
            Be specific with your notifier name.
            There can be multiple {{ newNotifier.type }} notifiers.
          </BTooltip>
        </BInputGroupText>
      </BInputGroup> <!-- /new notifier name -->
      <!-- new notifier fields -->
      <div v-for="field of newNotifier.fields"
        :key="field.name">
        <span class="mb-2"
          :class="{'input-group input-group-sm':field.type !== 'checkbox'}">
          <span v-if="field.type !== 'checkbox'"
            class="input-group-text cursor-help"
            :id="`newNotifierField-${field.name}`">
            {{ field.name }}
            <sup v-if="field.required">*</sup>
            <BTooltip :target="`newNotifierField-${field.name}`">
              {{ field.description }}
            </BTooltip>
          </span>
          <input :class="{'form-control':field.type !== 'checkbox'}"
            v-model="field.value"
            :type="getFieldInputType(field)"
            :placeholder="field.description"
          />
          <span v-if="field.type === 'secret'"
            class="input-group-text cursor-pointer"
            @click="toggleVisibleSecretField(field)">
            <span class="fa"
              :class="{'fa-eye':field.type === 'secret' && !field.showValue, 'fa-eye-slash':field.type === 'secret' && field.showValue}">
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
          <div class="ms-2 flex-grow-1">
            <b-input-group
              size="sm"
              prepend="Share with users">
              <b-form-input
                :model-value="newNotifier.users"
                @update:model-value="newNotifier.users = $event"
                placeholder="comma separated list of userIds"
              />
            </b-input-group>
          </div>
        </div>
      </div> <!-- /new notifier sharing -->
      <!-- create form error -->
      <div
        v-if="newNotifierError"
        class="alert alert-danger mt-2 mb-0">
        <span class="fa fa-exclamation-triangle me-1" />
        {{ newNotifierError }}
      </div> <!-- /create form error -->
      <!-- new notifier actions -->
      <template #footer>
        <div class="w-100 d-flex justify-content-between">
          <b-button
            title="Cancel"
            variant="danger"
            @click="showNotifierModal = false">
            <span class="fa fa-times me-1" />
            Cancel
          </b-button>
          <div>
            <b-button
              class="me-1"
              variant="warning"
              @click="clearNotifierFields">
              <span class="fa fa-ban me-1" />
              Clear fields
            </b-button>
            <b-button
              variant="success"
              @click="createNotifier">
              <span class="fa fa-plus me-1" />
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
            :id="`toggleNotifier-${index}`"
            @click="toggleNotifier(notifier, index)"
            :class="{'fa-toggle-on text-success':notifier.on,'fa-toggle-off':!notifier.on}"
            class="fa fa-lg pull-right cursor-pointer">
            <BTooltip :target="`toggleNotifier-${index}`">
              Turn this notifier {{notifier.on ? 'off' : 'on'}}
            </BTooltip>
          </span>
        </template>
        <b-card-text>
          <!-- notifier name -->
          <b-input-group
            size="sm"
            class="mb-2">
            <b-input-group-text
              class="cursor-help"
              :id="`notifierName-${index}`"
              title="Give your notifier a unique name">
              Name<sup>*</sup>
            </b-input-group-text>
            <input
              class="form-control"
              v-model="notifier.name"
            />
          </b-input-group> <!-- /notifier name -->
          <!-- notifier fields -->
          <div v-for="field of notifier.fields"
            :key="field.name">
            <span class="mb-2"
              :class="{'input-group input-group-sm':field.type !== 'checkbox'}">
              <span class="input-group-text cursor-help"
                v-if="field.type !== 'checkbox'"
                :id="`notifierField-${field.name}-${index}`">
                {{ field.name }}
                <sup v-if="field.required">*</sup>
                <BTooltip :target="`notifierField-${field.name}-${index}`">
                  {{ field.description }}
                </BTooltip>
              </span>
              <input :class="{'form-control':field.type !== 'checkbox'}"
                v-model="field.value"
                :type="getFieldInputType(field)"
              />
              <span v-if="field.type === 'secret'"
                class="input-group-text cursor-pointer"
                @click="toggleVisibleSecretField(field)">
                <span class="fa"
                  :class="{'fa-eye':field.type === 'secret' && !field.showValue, 'fa-eye-slash':field.type === 'secret' && field.showValue}">
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
            <input class="form-control"
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
                <template v-for="(alert, aKey) of notifier.alerts">
                  <span :key="aKey"
                    v-if="notifierTypes[notifier.type.toLowerCase()].alerts && notifierTypes[notifier.type.toLowerCase()].alerts[aKey]"
                    :id="aKey + notifier.name">
                      <BFormCheckbox
                        inline
                        :id="notifierTypes[notifier.type.toLowerCase()].alerts[aKey].name+notifier.name"
                        :name="notifierTypes[notifier.type.toLowerCase()].alerts[aKey].name+notifier.name"
                        :model-value="notifier.alerts[aKey]"
                        @update:model-value="notifier.alerts[aKey] = $event">
                      {{ notifierTypes[notifier.type.toLowerCase()].alerts[aKey].name }}
                    </BFormCheckbox>
                    <BTooltip :target="aKey + notifier.name">
                      Notify if {{ notifierTypes[notifier.type.toLowerCase()].alerts[aKey].description }}
                    </BTooltip>
                  </span>
                </template>
              </div>
            </div>
          </template> <!-- /notifier alerts -->
          <!-- notifier info -->
          <div class="row mt-2">
            <div class="col-12 small">
              <p v-if="notifier.created || notifier.user" class="m-0">
                Created by {{ notifier.user }} at
                {{ timezoneDateString(notifier.created * 1000, tz, false) }}
              </p>
              <p v-if="notifier.updated" class="m-0">
                Last updated at {{ timezoneDateString(notifier.updated * 1000, tz, false) }}
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
              class="fa fa-spinner fa-spin fa-fw me-1"
            />
            <span v-else class="fa fa-bell fa-fw me-1" />
            Test
          </b-button>
          <span class="pull-right">
            <b-button
              size="sm"
              class="me-1"
              variant="danger"
              @click="removeNotifier(notifier.id, index)">
              <span class="fa fa-trash-o fa-fw me-1" />
              Delete
            </b-button>
            <b-button
              size="sm"
              variant="success"
              @click="updateNotifier(notifier.id, index, notifier)">
              <span class="fa fa-save fa-fw me-1" />
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
import RoleDropdown from './RoleDropdown.vue';
import { timezoneDateString } from '@real_common/vueFilters.js';

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
      newNotifierError: '',
      showNotifierModal: false
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
    timezoneDateString,
    /* opens the form to create a new notifier */
    createNewNotifier (notifier) {
      const clone = JSON.parse(JSON.stringify(notifier));
      // flatten notifier alerts (we only need on state)
      for (const a in clone.alerts) {
        clone.alerts[a] = false; // off by default
      }
      clone.roles = ['arkimeUser', 'parliamentUser'];
      clone.users = '';
      this.newNotifier = clone;
      this.showNotifierModal = true;
    },
    /* updates the roles on the new notifier object from the RoleDropdown component */
    updateNewNotifierRoles (roles) {
      this.newNotifier.roles = roles;
    },
    /* updates the roles on a notifier object from the RoleDropdown component */
    updateNotifierRoles (roles, id) {
      for (const notifier of this.notifiers) {
        if (notifier.id === id) {
          notifier.roles = roles;
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

        this.showNotifierModal = false;
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
      field.showValue = !field.showValue;
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

      this.notifiers[index].loading = true;

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

        this.notifiers[index].loading = false;
        // display success message to user
        this.$emit('display-message', { msg: response.text || 'Successfully tested notifier.' });
        this.newNotifier = {};
      }).catch((error) => {
        this.notifiers[index].loading = false;
        this.$emit('display-message', { msg: error.text || 'Error testing notifier.', type: 'danger' });
      });
    },
    /* toggles a notifier on/off (parliament use only) */
    toggleNotifier: function (notifier, index) {
      notifier.on = !notifier.on;
      this.updateNotifier(notifier.id, index, notifier);
    }
  }
};
</script>
