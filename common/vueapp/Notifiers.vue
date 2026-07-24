<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <h3>
      {{ $t('settings.notifiers.title') }}
      <v-menu
        v-if="notifierTypes"
        location="bottom end">
        <template #activator="{ props: activatorProps }">
          <v-btn
            v-bind="activatorProps"
            color="primary"
            variant="flat"
            class="float-right">
            <v-icon
              icon="mdi-plus-circle"
              start />
            {{ $t('settings.notifiers.newGeneric', 'New Notifier') }}
            <v-icon
              icon="mdi-menu-down"
              end />
          </v-btn>
        </template>
        <v-list density="compact">
          <v-list-item
            v-for="notifier of sortedNotifierTypes"
            :key="notifier.name"
            @click="createNewNotifier(notifier)">
            <v-list-item-title>
              {{ notifier.name.charAt(0).toUpperCase() + notifier.name.slice(1) }}
            </v-list-item-title>
          </v-list-item>
        </v-list>
      </v-menu>
    </h3>

    <p class="lead">
      {{ $t(helpIntlId) }}
    </p>

    <v-divider class="my-3" />

    <!-- notifiers list error -->
    <v-alert
      v-if="error"
      type="error"
      variant="tonal"
      density="compact"
      class="mt-2 mb-2">
      {{ error }}
    </v-alert> <!-- /notifiers list error -->

    <!-- no results -->
    <div
      class="text-center mt-4"
      v-if="!notifiers || !Object.keys(notifiers).length">
      <h3>
        <v-icon
          icon="mdi-folder-open"
          size="x-large"
          class="text-medium-emphasis" />
      </h3>
      <h5 class="lead">
        {{ $t('settings.notifiers.createHelp') }}
      </h5>
    </div> <!-- /no results -->

    <!-- new notifier -->
    <v-dialog
      :model-value="showNotifierModal"
      @update:model-value="(val) => { if (!val) showNotifierModal = false; }"
      max-width="1140">
      <v-card density="compact">
        <v-card-title>
          {{ $t('settings.notifiers.createNew', { type: newNotifier.type ? newNotifier.type.charAt(0).toUpperCase() + newNotifier.type.slice(1) : '' }) }}
        </v-card-title>
        <v-card-text>
          <!-- new notifier name -->
          <div class="arkime-input-group arkime-input-group--fluid mb-2">
            <span
              id="newNotifierName"
              class="arkime-input-label cursor-help">
              {{ $t('settings.notifiers.name') }}
              <sup>*</sup>
              <v-tooltip activator="#newNotifierName">
                {{ $t('settings.notifiers.uniqueName', { type: newNotifier.type }) }}
              </v-tooltip>
            </span>
            <input
              class="arkime-input-control"
              v-model="newNotifier.name"
              :placeholder="$t('settings.notifiers.namePlaceholder')">
            <span
              id="newNotifierNameHelp"
              class="arkime-input-label arkime-input-label-fw cursor-help">
              <v-icon icon="mdi-information" />
              <v-tooltip activator="#newNotifierNameHelp">
                {{ $t('settings.notifiers.nameInfo', { type: newNotifier.type }) }}
              </v-tooltip>
            </span>
          </div> <!-- /new notifier name -->
          <!-- new notifier fields -->
          <div
            v-for="field of newNotifier.fields"
            :key="field.name">
            <span
              class="mb-2"
              :class="{'arkime-input-group arkime-input-group--fluid':field.type !== 'checkbox'}">
              <span
                v-if="field.type !== 'checkbox'"
                class="arkime-input-label cursor-help"
                :id="`newNotifierField-${field.name}`">
                {{ field.name }}
                <sup v-if="field.required">*</sup>
                <v-tooltip :activator="`#newNotifierField-${field.name}`">
                  {{ field.description }}
                </v-tooltip>
              </span>
              <input
                :class="{'arkime-input-control':field.type !== 'checkbox'}"
                v-model="field.value"
                :type="getFieldInputType(field)"
                :placeholder="field.description">
              <span
                v-if="field.type === 'secret'"
                class="arkime-input-label arkime-input-label-fw cursor-pointer"
                @click="toggleVisibleSecretField(field)">
                <v-icon :icon="field.showValue ? 'mdi-eye-off' : 'mdi-eye'" />
              </span>
            </span>
            <label v-if="field.type === 'checkbox'">
              &nbsp;{{ field.name }}
            </label>
          </div> <!-- /new notifier fields -->
          <!-- new notifier sharing -->
          <div class="row">
            <div class="col d-flex">
              <div>
                <RoleDropdown
                  :roles="roles"
                  :display-text="$t('common.shareWithRoles')"
                  :selected-roles="newNotifier.roles"
                  @selected-roles-updated="updateNewNotifierRoles" />
              </div>
              <div class="ms-2 flex-grow-1">
                <div class="arkime-input-group arkime-input-group--fluid">
                  <span class="arkime-input-label">{{ $t('common.shareWithUsers') }}</span>
                  <input
                    type="text"
                    class="arkime-input-control"
                    :value="newNotifier.users"
                    @input="newNotifier.users = $event.target.value"
                    :placeholder="$t('common.listOfUserIds')">
                </div>
              </div>
            </div>
          </div> <!-- /new notifier sharing -->
          <!-- create form error -->
          <v-alert
            v-if="newNotifierError"
            type="error"
            variant="tonal"
            density="compact"
            class="mt-2 mb-0">
            {{ newNotifierError }}
          </v-alert> <!-- /create form error -->
        </v-card-text>
        <!-- new notifier actions -->
        <v-card-actions>
          <div class="w-100 d-flex justify-space-between">
            <v-btn
              size="large"
              color="error"
              variant="flat"
              :title="$t('common.cancel')"
              @click="showNotifierModal = false">
              <v-icon
                start
                icon="mdi-close" />
              {{ $t('common.cancel') }}
            </v-btn>
            <div>
              <v-btn
                size="large"
                color="warning"
                variant="flat"
                class="me-1"
                @click="clearNotifierFields">
                <v-icon
                  start
                  icon="mdi-cancel" />
                {{ $t('common.clear') }}
              </v-btn>
              <v-btn
                size="large"
                color="success"
                variant="flat"
                @click="createNotifier">
                <v-icon
                  start
                  icon="mdi-plus" />
                {{ $t('common.create') }}
              </v-btn>
            </div>
          </div>
        </v-card-actions> <!-- /new notifier actions -->
      </v-card>
    </v-dialog> <!-- new notifier -->

    <!-- notifiers -->
    <div
      v-if="notifiers"
      class="notifier-card-columns mb-2">
      <v-card
        :key="notifier.key"
        class="notifier-card mb-2"
        v-for="(notifier, index) of notifiers">
        <v-card-title>
          {{ notifier.type.charAt(0).toUpperCase() + notifier.type.slice(1) }} Notifier
          <v-icon
            v-if="parentApp === 'parliament'"
            :id="`toggleNotifier-${index}`"
            :icon="notifier.on ? 'mdi-toggle-switch' : 'mdi-toggle-switch-off'"
            :class="{'text-success': notifier.on}"
            class="float-right cursor-pointer"
            @click="toggleNotifier(notifier, index)">
            <v-tooltip :activator="`#toggleNotifier-${index}`">
              {{ $t('settings.notifiers.turn' + (notifier.on ? 'Off' : 'On')) }}
            </v-tooltip>
          </v-icon>
        </v-card-title>
        <v-card-text>
          <!-- notifier name -->
          <div class="arkime-input-group arkime-input-group--fluid mb-2">
            <span
              class="arkime-input-label cursor-help"
              :id="`notifierName-${index}`"
              :title="$t('settings.notifiers.uniqueName', { type: notifier.type })">
              {{ $t('settings.notifiers.name') }}
              <sup>*</sup>
            </span>
            <input
              class="arkime-input-control"
              v-model="notifier.name">
          </div> <!-- /notifier name -->
          <!-- notifier fields -->
          <div
            v-for="field of notifier.fields"
            :key="field.name">
            <span
              class="mb-2"
              :class="{'arkime-input-group arkime-input-group--fluid':field.type !== 'checkbox'}">
              <span
                class="arkime-input-label cursor-help"
                v-if="field.type !== 'checkbox'"
                :id="`notifierField-${field.name}-${index}`">
                {{ field.name }}
                <sup v-if="field.required">*</sup>
                <v-tooltip :activator="`#notifierField-${field.name}-${index}`">
                  {{ field.description }}
                </v-tooltip>
              </span>
              <input
                :class="{'arkime-input-control':field.type !== 'checkbox'}"
                v-model="field.value"
                :type="getFieldInputType(field)">
              <span
                v-if="field.type === 'secret'"
                class="arkime-input-label arkime-input-label-fw cursor-pointer"
                @click="toggleVisibleSecretField(field)">
                <v-icon :icon="field.showValue ? 'mdi-eye-off' : 'mdi-eye'" />
              </span>
            </span>
            <label v-if="field.type === 'checkbox'">
              &nbsp;{{ field.name }}
            </label>
          </div> <!-- /notifier fields -->
          <!-- notifier sharing -->
          <div class="arkime-input-group arkime-input-group--fluid mb-2">
            <span class="arkime-input-label">{{ $t('common.shareWithUsers') }}</span>
            <input
              class="arkime-input-control"
              v-model="notifier.users"
              :placeholder="$t('common.listOfUserIds')">
          </div>
          <RoleDropdown
            :roles="roles"
            :id="notifier.id"
            :selected-roles="notifier.roles"
            @selected-roles-updated="updateNotifierRoles"
            :display-text="notifier.roles && notifier.roles.length ? undefined : $t('common.shareWithRoles')" /> <!-- /notifier sharing -->
          <template v-if="parentApp === 'parliament' && notifier.alerts && notifierTypes[notifier.type.toLowerCase()]">
            <v-divider class="my-3" />
            <!-- notifier alerts -->
            <h5>Notify on</h5>
            <div class="row">
              <div class="w-100">
                <template v-for="(alert, aKey) of notifier.alerts">
                  <span
                    :key="aKey"
                    v-if="notifierTypes[notifier.type.toLowerCase()]?.alerts && notifierTypes[notifier.type.toLowerCase()].alerts[aKey]"
                    :id="aKey + notifier.name"
                    class="me-2 d-inline-flex align-items-center">
                    <input
                      type="checkbox"
                      class="arkime-check-input me-1"
                      :id="notifierTypes[notifier.type.toLowerCase()].alerts[aKey].name + notifier.name"
                      :name="notifierTypes[notifier.type.toLowerCase()].alerts[aKey].name + notifier.name"
                      :checked="notifier.alerts[aKey]"
                      @change="notifier.alerts[aKey] = $event.target.checked">
                    {{ notifierTypes[notifier.type.toLowerCase()].alerts[aKey].name }}
                    <v-tooltip :activator="`#${aKey}${notifier.name}`">
                      {{ $t('settings.notifiers.notifyIf', { when: notifierTypes[notifier.type.toLowerCase()].alerts[aKey].description }) }}
                    </v-tooltip>
                  </span>
                </template>
              </div>
            </div>
          </template> <!-- /notifier alerts -->
          <!-- notifier info -->
          <div class="row mt-2">
            <div class="w-100 text-caption">
              <p
                v-if="notifier.created || notifier.user"
                class="m-0">
                {{ $t('settings.notifiers.createdBy', { user: notifier.user, when: timezoneDateString(notifier.created * 1000, tz, false) }) }}
              </p>
              <p
                v-if="notifier.updated"
                class="m-0">
                {{ $t('settings.notifiers.updatedAt', { when: timezoneDateString(notifier.updated * 1000, tz, false) }) }}
              </p>
            </div>
          </div> <!-- /notifier info -->
        </v-card-text>
        <!-- notifier actions -->
        <v-card-actions>
          <v-btn
            size="large"
            color="warning"
            variant="outlined"
            :disabled="notifier.loading"
            @click="testNotifier(notifier.id, index)">
            <v-icon start>
              {{ notifier.loading ? 'mdi-loading mdi-spin' : 'mdi-bell' }}
            </v-icon>
            {{ $t('common.test') }}
          </v-btn>
          <span class="float-right">
            <v-btn
              size="large"
              color="error"
              variant="flat"
              class="me-1"
              @click="removeNotifier(notifier.id, index)">
              <v-icon
                start
                icon="mdi-trash-can-outline" />
              {{ $t('common.delete') }}
            </v-btn>
            <v-btn
              size="large"
              color="success"
              variant="flat"
              @click="updateNotifier(notifier.id, index, notifier)">
              <v-icon
                start
                icon="mdi-content-save" />
              {{ $t('common.save') }}
            </v-btn>
          </span>
        </v-card-actions> <!-- /notifier actions -->
      </v-card>
    </div> <!-- notifiers -->
  </div>
</template>

<script>
import setReqHeaders from './setReqHeaders';
import RoleDropdown from './RoleDropdown.vue';
import { timezoneDateString } from './vueFilters.js';
import { resolveMessage } from './resolveI18nMessage';

export default {
  name: 'Notifiers',
  emits: ['display-message'],
  components: { RoleDropdown },
  props: {
    helpIntlId: {
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
    sortedNotifierTypes () {
      return Object.values(this.notifierTypes).sort((a, b) => b.name.localeCompare(a.name));
    },
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
      this.error = resolveMessage(error, this.$t);
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
        this.error = resolveMessage(error, this.$t);
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
        this.newNotifierError = this.$t('settings.notifiers.noNotifierErr');
        return;
      }

      if (!this.newNotifier.name) {
        this.newNotifierError = this.$t('settings.notifiers.uniqueNameErr');
        return;
      }

      // make sure required fields are filled
      for (const field of this.newNotifier.fields) {
        if (!field.value && field.required) {
          this.newNotifierError = this.$t('settings.notifiers.fieldRequiredErr', { field: field.name });
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
          this.newNotifierError = resolveMessage(response, this.$t);
          return;
        }

        this.showNotifierModal = false;
        // add notifier to the list
        this.notifiers.push(response.notifier);
        // display success message to user
        let msg = resolveMessage(response, this.$t) || this.$t('settings.notifiers.successCreate');
        if (response.invalidUsers && response.invalidUsers.length) {
          msg += ' ' + this.$t('settings.notifiers.couldNotAddUsers', { users: response.invalidUsers.join(',') });
        }
        this.$emit('display-message', { msg });
        this.newNotifier = {};
      }).catch((error) => {
        this.newNotifierError = resolveMessage(error, this.$t);
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
          this.$emit('display-message', { msg: resolveMessage(response, this.$t) || this.$t('settings.notifiers.errorDelete'), type: 'danger' });
          return;
        }

        this.notifiers.splice(index, 1);
        // display success message to user
        this.$emit('display-message', { msg: resolveMessage(response, this.$t) || this.$t('settings.notifiers.successDelete') });
      }).catch((error) => {
        this.$emit('display-message', { msg: resolveMessage(error, this.$t) || this.$t('settings.notifiers.errorDelete'), type: 'danger' });
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
          this.$emit('display-message', { msg: resolveMessage(response, this.$t) || this.$t('settings.notifiers.errorUpdate'), type: 'danger' });
          return;
        }

        this.notifiers.splice(index, 1, response.notifier);
        // display success message to user
        let msg = resolveMessage(response, this.$t) || this.$t('settings.notifiers.successUpdate');
        if (response.invalidUsers && response.invalidUsers.length) {
          msg += ' ' + this.$t('settings.notifiers.couldNotAddUsers', { users: response.invalidUsers.join(',') });
        }
        this.$emit('display-message', { msg });
      }).catch((error) => {
        this.$emit('display-message', { msg: resolveMessage(error, this.$t) || this.$t('settings.notifiers.errorUpdate'), type: 'danger' });
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
          this.$emit('display-message', { msg: resolveMessage(response, this.$t) || this.$t('settings.notifiers.errorTest'), type: 'danger' });
          return;
        }

        this.notifiers[index].loading = false;
        // display success message to user
        this.$emit('display-message', { msg: resolveMessage(response, this.$t) || this.$t('settings.notifiers.successTest') });
        this.newNotifier = {};
      }).catch((error) => {
        this.notifiers[index].loading = false;
        this.$emit('display-message', { msg: resolveMessage(error, this.$t) || this.$t('settings.notifiers.errorTest'), type: 'danger' });
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

<style scoped>
/* Pinterest-style masonry layout to replace b-card-group's columns prop. */
.notifier-card-columns {
  column-count: 3;
  column-gap: 0.75rem;
}
.notifier-card {
  display: inline-block;
  width: 100%;
  break-inside: avoid;
}
@media (max-width: 1199.98px) {
  .notifier-card-columns { column-count: 2; }
}
@media (max-width: 767.98px) {
  .notifier-card-columns { column-count: 1; }
}
</style>
