<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <h3>
      Shortcuts
      <button
        type="button"
        class="btn btn-sm btn-success pull-right"
        @click="showShortcutModal = true">
        <span class="fa fa-plus-circle me-1" />
        {{ $t('settings.shortcuts.newShortcut') }}
      </button>
    </h3>

    <span v-html="$t('settings.shortcuts.infoHtml')" />
    <p>
      <strong>{{ $t('settings.shortcuts.note') }}:</strong>
      <template v-if="hasUsersES">
        {{ $t('settings.shortcuts.synced') }}
      </template>
      <template v-else>
        {{ $t('settings.shortcuts.notSynced') }}
      </template>
    </p>

    <div class="d-flex align-items-center">
      <div class="flex-grow-1 me-2">
        <v-text-field
          density="compact"
          variant="outlined"
          hide-details
          clearable
          prepend-inner-icon="fa-search"
          :model-value="shortcutsQuery.search"
          @update:model-value="updateSearch"
          placeholder="Search shortcuts" />
      </div>
      <v-btn-toggle
        v-if="user.roles.includes('arkimeAdmin')"
        density="compact"
        variant="outlined"
        color="secondary"
        class="me-2"
        multiple
        :model-value="seeAll ? ['seeAll'] : []"
        @update:model-value="(val) => updateSeeAll(val.includes('seeAll'))">
        <v-btn
          value="seeAll"
          id="seeAllShortcuts">
          <span class="fa fa-user-circle me-1" />
          {{ $t(seeAll ? 'settings.shortcuts.seeMy' : 'settings.shortcuts.seeAll') }}
          <v-tooltip
            activator="parent"
            location="top">
            {{ $t(seeAll ? 'settings.shortcuts.seeMyTip' : 'settings.shortcuts.seeAllTip') }}
          </v-tooltip>
        </v-btn>
      </v-btn-toggle>
      <arkime-paging
        v-if="shortcuts.data"
        :length-default="shortcutsSize"
        @change-paging="changeShortcutsPaging"
        :records-total="shortcuts.recordsTotal"
        :records-filtered="shortcuts.recordsFiltered" />
    </div>

    <table
      v-if="shortcuts.data"
      class="table table-striped table-sm">
      <thead>
        <tr>
          <th
            class="cursor-pointer"
            @click.self="sortShortcuts('name')">
            {{ $t('settings.shortcuts.table-name') }}
            <span
              v-show="shortcutsQuery.sortField === 'name' && !shortcutsQuery.desc"
              class="fa fa-sort-asc" />
            <span
              v-show="shortcutsQuery.sortField === 'name' && shortcutsQuery.desc"
              class="fa fa-sort-desc" />
            <span
              v-show="shortcutsQuery.sortField !== 'name'"
              class="fa fa-sort" />
          </th>
          <th
            class="cursor-pointer"
            @click.self="sortShortcuts('description')">
            {{ $t('settings.shortcuts.table-description') }}
            <span
              v-show="shortcutsQuery.sortField === 'description' && !shortcutsQuery.desc"
              class="fa fa-sort-asc" />
            <span
              v-show="shortcutsQuery.sortField === 'description' && shortcutsQuery.desc"
              class="fa fa-sort-desc" />
            <span
              v-show="shortcutsQuery.sortField !== 'description'"
              class="fa fa-sort" />
          </th>
          <th>{{ $t('settings.shortcuts.table-values') }}</th>
          <th>{{ $t('settings.shortcuts.table-type') }}</th>
          <th>{{ $t('settings.shortcuts.table-creator') }}</th>
          <th>&nbsp;</th>
        </tr>
      </thead>
      <tbody>
        <!-- shortcuts -->
        <tr v-if="loading">
          <td colspan="9">
            <p class="text-center mb-0">
              <span class="fa fa-spinner fa-spin" />
              {{ $t('common.loading') }}
            </p>
          </td>
        </tr>
        <template
          v-for="(item, index) in shortcuts.data"
          :key="`${item.id}-content`">
          <tr>
            <td
              :id="`shortcut-${item.id}`"
              class="shortcut-value narrow cursor-help">
              {{ item.name }}
              <v-tooltip :activator="`#shortcut-${item.id}`">
                {{ item.name }}
              </v-tooltip>
            </td>
            <td
              :id="`shortcut-${item.id}-desc`"
              class="shortcut-value cursor-help">
              {{ item.description }}
              <v-tooltip :activator="`#shortcut-${item.id}-desc`">
                {{ item.description }}
              </v-tooltip>
            </td>
            <td
              class="shortcut-value"
              :class="{'show-all':item.showAll}">
              <span
                v-if="item.value.length > 50"
                @click="toggleDisplayAllShortcut(item)"
                class="fa pull-right cursor-pointer mt-1"
                :class="{'fa-chevron-down':!item.showAll,'fa-chevron-up':item.showAll}" />
              <span v-if="!item.showAll">
                {{ item.value.substring(0, 50) }}
                <span v-if="item.value.length > 50">...</span>
              </span>
              <span v-else>{{ item.value }}</span>
            </td>
            <td>
              {{ item.type }}
            </td>
            <td>
              {{ item.userId }}
            </td>
            <td class="shortcut-btns">
              <span class="pull-right">
                <button
                  type="button"
                  class="btn btn-sm btn-theme-secondary ms-1"
                  :id="`copy-${item.id}`"
                  @click="$emit('copy-value', item.value)">
                  <span class="fa fa-clipboard fa-fw" />
                  <v-tooltip :activator="`#copy-${item.id}`">
                    {{ $t('settings.shortcuts.copyTip') }}
                  </v-tooltip>
                </button>
                <span v-if="canEdit(item)">
                  <button
                    type="button"
                    class="btn btn-sm btn-info ms-1"
                    :id="`transfer-${item.id}`"
                    v-if="canTransfer(item)"
                    @click="openTransferShortcut(item)">
                    <span class="fa fa-share fa-fw" />
                    <v-tooltip :activator="`#transfer-${item.id}`">
                      {{ $t('settings.shortcuts.transferTip') }}
                    </v-tooltip>
                  </button>
                  <button
                    type="button"
                    class="btn btn-sm btn-danger ms-1"
                    :id="`delete-${item.id}`"
                    @click="deleteShortcut(item, index)">
                    <span
                      class="fa fa-trash-o fa-fw"
                      v-if="!item.loading" />
                    <span
                      class="fa fa-spinner fa-spin fa-fw"
                      v-else />
                    <v-tooltip :activator="`#delete-${item.id}`">
                      {{ $t('settings.shortcuts.deleteTip') }}
                    </v-tooltip>
                  </button>
                  <span>
                    <div
                      v-if="item.locked"
                      style="display:inline-block">
                      <button
                        type="button"
                        :disabled="true"
                        class="btn btn-sm btn-warning disabled cursor-help ms-1"
                        :id="`locked-${item.id}`">
                        <span class="fa fa-lock fa-fw" />
                      </button>
                      <v-tooltip :activator="`#locked-${item.id}`">
                        {{ $t('settings.shortcuts.lockedTip') }}
                      </v-tooltip>
                    </div>
                    <button
                      type="button"
                      v-else
                      class="btn btn-sm btn-theme-tertiary ms-1"
                      :id="`update-${item.id}`"
                      @click="editShortcut(item)">
                      <span
                        class="fa fa-pencil fa-fw"
                        v-if="!item.loading" />
                      <span
                        class="fa fa-spinner fa-spin fa-fw"
                        v-else />
                      <v-tooltip :activator="`#update-${item.id}`">
                        {{ $t('settings.shortcuts.updateTip') }}
                      </v-tooltip>
                    </button>
                  </span>
                </span>
              </span>
            </td>
          </tr>
        </template> <!-- /shortcuts -->
      </tbody>
    </table>

    <!-- shortcuts list error -->
    <div
      v-if="shortcutsListError"
      style="z-index: 2000;"
      class="mt-2 mb-0 alert alert-danger">
      <span class="fa fa-exclamation-triangle me-1" />
      {{ shortcutsListError }}
    </div> <!-- /shortcuts list error -->

    <!-- no results -->
    <div
      class="text-center mt-4"
      v-if="shortcuts.data && shortcuts.data.length === 0">
      <h3>
        <span class="fa fa-folder-open fa-2x" />
      </h3>
      <h5>
        {{ $t('settings.shortcuts.noMatch') }}
        <br>
        {{ $t('settings.shortcuts.useCreate') }}
      </h5>
    </div> <!-- /no results -->

    <!-- new shortcut form -->
    <v-dialog
      :model-value="showShortcutModal"
      @update:model-value="(val) => { if (!val) showShortcutModal = false; }"
      max-width="1140">
      <v-card density="compact">
        <v-card-title>
          {{ $t(editingShortcut ? 'settings.shortcuts.editShortcut' : 'settings.shortcuts.newShortcut') }}
        </v-card-title>
        <v-card-text>
          <div class="input-group input-group-sm mb-2">
            <span
              id="shortcutFormName"
              class="input-group-text cursor-help">
              {{ $t('settings.shortcuts.shortcutFormName') }}<sup>*</sup>
              <v-tooltip activator="#shortcutFormName">
                {{ $t('settings.shortcuts.shortcutFormNameTip') }}
              </v-tooltip>
            </span>
            <input
              type="text"
              class="form-control"
              :value="newShortcutName"
              placeholder="MY_ARKIME_VAR"
              @input="newShortcutName = $event.target.value">
          </div>
          <div class="input-group input-group-sm mb-2">
            <span
              id="shortcutFormDesc"
              class="input-group-text cursor-help">
              {{ $t('settings.shortcuts.shortcutFormDesc') }}
              <v-tooltip activator="#shortcutFormDesc">
                {{ $t('settings.shortcuts.shortcutFormDescTip') }}
              </v-tooltip>
            </span>
            <input
              type="text"
              class="form-control"
              :value="newShortcutDescription"
              :placeholder="$t('settings.shortcuts.shortcutFormDescPlaceholder')"
              @input="newShortcutDescription = $event.target.value">
          </div>
          <div class="input-group input-group-sm mb-2">
            <span
              id="shortCutFormValue"
              class="input-group-text cursor-help">
              {{ $t('settings.shortcuts.shortcutFormValue') }}<sup>*</sup>
              <v-tooltip activator="#shortCutFormValue">
                {{ $t('settings.shortcuts.shortcutFormValueTip') }}
              </v-tooltip>
            </span>
            <textarea
              class="form-control"
              rows="5"
              :value="newShortcutValue"
              :placeholder="$t('settings.shortcuts.shortcutFormValuePlaceholder')"
              @input="newShortcutValue = $event.target.value" />
          </div>
          <div class="input-group input-group-sm mb-2">
            <span
              id="shortcutFormType"
              class="input-group-text cursor-help">
              {{ $t('settings.shortcuts.shortcutFormType') }}<sup>*</sup>
              <v-tooltip activator="#shortcutFormType">
                {{ $t('settings.shortcuts.shortcutFormTypeTip') }}
              </v-tooltip>
            </span>
            <select
              v-model="newShortcutType"
              class="form-control form-control-sm">
              <option
                value="ip"
                v-i18n-value="'settings.shortcuts.newShortcutType-'" />
              <option
                value="string"
                v-i18n-value="'settings.shortcuts.newShortcutType-'" />
              <option
                value="number"
                v-i18n-value="'settings.shortcuts.newShortcutType-'" />
            </select>
          </div>
          <div class="d-flex">
            <div class="me-3 flex-grow-1 no-wrap">
              <RoleDropdown
                :roles="roles"
                class="d-inline me-1"
                :display-text="$t('common.rolesCanView')"
                :selected-roles="newShortcutRoles"
                @selected-roles-updated="updateNewShortcutRoles" />
              <RoleDropdown
                :roles="roles"
                class="d-inline"
                :display-text="$t('common.rolesCanEdit')"
                :selected-roles="newShortcutEditRoles"
                @selected-roles-updated="updateNewShortcutEditRoles" />
            </div>
            <div class="input-group input-group-sm">
              <span
                id="shortcutFormUsers"
                class="input-group-text cursor-help">
                {{ $t('common.shareWithUsers') }}
                <v-tooltip activator="#shortcutFormUsers">
                  {{ $t('settings.shortcuts.shortcutFormUsersTip') }}
                </v-tooltip>
              </span>
              <input
                type="text"
                class="form-control"
                :value="newShortcutUsers"
                @input="newShortcutUsers = $event.target.value"
                :placeholder="$t('settings.shortcuts.shortcutFormUsersPlaceholder')">
            </div>
          </div>
          <!-- create form error -->
          <div
            v-if="shortcutFormError"
            class="alert alert-danger alert-sm mt-2 mb-0">
            <span class="fa fa-exclamation-triangle me-1" />
            {{ shortcutFormError }}
          </div> <!-- /create form error -->
        </v-card-text>
        <v-card-actions>
          <div class="w-100 d-flex justify-content-between">
            <button
              type="button"
              class="btn btn-danger"
              @click="showShortcutModal = false">
              <span class="fa fa-times" />
              {{ $t('common.cancel') }}
            </button>
            <button
              type="button"
              class="btn btn-success"
              v-if="!editingShortcut"
              @click="createShortcut"
              :disabled="createShortcutLoading"
              :class="{'disabled':createShortcutLoading}">
              <template v-if="!createShortcutLoading">
                <span class="fa fa-plus-circle me-1" />
                {{ $t('common.create') }}
              </template>
              <template v-else>
                <span class="fa fa-spinner fa-spin me-1" />
                {{ $t('common.creating') }}
              </template>
            </button>
            <button
              type="button"
              v-else
              class="btn btn-success"
              @click="updateShortcut"
              :disabled="createShortcutLoading"
              :class="{'disabled':createShortcutLoading}">
              <template v-if="!createShortcutLoading">
                <span class="fa fa-save me-1" />
                {{ $t('common.save') }}
              </template>
              <template v-else>
                <span class="fa fa-spinner fa-spin me-1" />
                {{ $t('common.saving') }}
              </template>
            </button>
          </div>
        </v-card-actions>
      </v-card>
    </v-dialog> <!-- /new shortcut form -->

    <transfer-resource
      :show-modal="showTransferModal"
      @transfer-resource="submitTransferShortcut" />
  </div> <!-- / shortcut settings -->
</template>

<script>
// services
import SettingsService from './SettingsService';
import UserService from '@common/UserService';
// components
import ArkimePaging from '../utils/Pagination.vue';
import RoleDropdown from '@common/RoleDropdown.vue';
import TransferResource from '@common/TransferResource.vue';
import { resolveMessage } from '@common/resolveI18nMessage';

export default {
  name: 'Shortcuts',
  emits: ['copy-value', 'display-message'],
  components: {
    ArkimePaging,
    RoleDropdown,
    TransferResource
  },
  data () {
    return {
      loading: true,
      shortcuts: {},
      shortcutsListError: '',
      shortcutFormError: '',
      newShortcutName: '',
      newShortcutDescription: '',
      newShortcutValue: '',
      newShortcutType: 'string',
      newShortcutUsers: '',
      newShortcutRoles: [],
      newShortcutEditRoles: [],
      editingShortcut: false,
      shortcutsStart: 0,
      shortcutsSize: 50,
      shortcutsQuery: {
        desc: false,
        sortField: 'name',
        search: ''
      },
      createShortcutLoading: false,
      hasUsersES: this.$constants.HASUSERSES,
      showAll: false,
      seeAll: false,
      transferResource: undefined,
      showShortcutModal: false,
      showTransferModal: false
    };
  },
  computed: {
    user () {
      return this.$store.state.user;
    },
    roles () {
      return this.$store.state.roles;
    }
  },
  watch: {
    'shortcutsQuery.search' () {
      this.getShortcuts();
    }
  },
  mounted () {
    this.getShortcuts();
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    canEdit (shortcut) {
      return this.user.roles.includes('arkimeAdmin') ||
        (shortcut.userId && shortcut.userId === this.user.userId) ||
        (shortcut.editRoles && UserService.hasRole(this.user, shortcut.editRoles.join(',')));
    },
    canTransfer (shortcut) {
      return this.user.roles.includes('arkimeAdmin') ||
        (shortcut.userId && shortcut.userId === this.user.userId);
    },
    updateSearch (newSearch) {
      this.shortcutsQuery.search = newSearch; // NOTE watch will trigger getShortcuts
    },
    updateSeeAll (newSeeAll) {
      this.seeAll = newSeeAll;
      this.getShortcuts();
    },
    /**
     * triggered when shortcuts paging is changed
     * @param {object} newParams Object containing length & start
     */
    changeShortcutsPaging (newParams) {
      this.shortcutsSize = newParams.length;
      this.shortcutsStart = newParams.start;
      this.getShortcuts();
    },
    /**
     * triggered when a sortable shortcuts column is clicked
     * if the sort field is the same as the current sort field, toggle the desc
     * flag, otherwise set it to default (false)
     * @param {string} sort The field to sort on
     */
    sortShortcuts (sort) {
      this.shortcutsQuery.desc = this.shortcutsQuery.sortField === sort ? !this.shortcutsQuery.desc : false;
      this.shortcutsQuery.sortField = sort;
      this.getShortcuts();
    },
    /* opens up modal to edit shortcut */
    editShortcut (shortcut) {
      this.shortcutFormError = '';
      this.editingShortcut = shortcut.id;
      this.newShortcutName = shortcut.name || '';
      this.newShortcutValue = shortcut.value || '';
      this.newShortcutUsers = shortcut.users || '';
      this.newShortcutRoles = shortcut.roles || [];
      this.newShortcutEditRoles = shortcut.editRoles || [];
      this.newShortcutType = shortcut.type || 'string';
      this.newShortcutDescription = shortcut.description || '';
      this.showShortcutModal = true;
    },
    /* show/hide the entire shortcut value */
    toggleDisplayAllShortcut (shortcut) {
      shortcut.showAll = !shortcut.showAll;
    },
    updateNewShortcutRoles (roles) {
      this.newShortcutRoles = roles;
    },
    updateNewShortcutEditRoles (roles) {
      this.newShortcutEditRoles = roles;
    },
    /* creates a new shortcut */
    createShortcut () {
      if (!this.validShortcutForm()) { return; }

      this.createShortcutLoading = true;

      const data = {
        name: this.newShortcutName,
        type: this.newShortcutType,
        value: this.newShortcutValue,
        users: this.newShortcutUsers,
        roles: this.newShortcutRoles,
        editRoles: this.newShortcutEditRoles,
        description: this.newShortcutDescription
      };

      SettingsService.createShortcut(data).then((response) => {
        this.getShortcuts();
        this.clearShortcutForm();
        this.showShortcutModal = false;
        this.displaySuccess(response);
      }).catch((error) => {
        this.shortcutFormError = resolveMessage(error, this.$t);
        this.createShortcutLoading = false;
      });
    },
    updateShortcutRoles (roles, id) {
      for (const shortcut of this.shortcuts.data) {
        if (shortcut.id === id) {
          shortcut.newRoles = roles;
          return;
        }
      }
    },
    /**
     * Opens the transfer resource modal
     * @param {Object} shortcut The shortcut to transfer
     */
    openTransferShortcut (shortcut) {
      this.transferShortcut = shortcut;
      this.showTransferModal = true;
    },
    /**
     * Submits the transfer resource modal contents and updates the shortcut
     * @param {Object} userId The user id to transfer the shortcut to
     */
    submitTransferShortcut ({ userId }) {
      if (!userId) {
        this.transferShortcut = undefined;
        this.showTransferModal = false;
        return;
      }

      const data = JSON.parse(JSON.stringify(this.transferShortcut));
      const id = data.id;
      delete data.id;
      data.userId = userId;

      SettingsService.updateShortcut(id, data).then((response) => {
        this.getShortcuts();
        this.transferShortcut = undefined;
        this.$emit('display-message', { msg: resolveMessage(response, this.$t), type: 'success' });
        this.showTransferModal = false;
      }).catch((error) => {
        this.$emit('display-message', { msg: resolveMessage(error, this.$t), type: 'danger' });
      });
    },
    /* updates a specified shortcut */
    updateShortcut () {
      if (!this.validShortcutForm()) { return; }

      this.createShortcutLoading = true;

      const data = {
        name: this.newShortcutName,
        type: this.newShortcutType,
        value: this.newShortcutValue,
        users: this.newShortcutUsers,
        roles: this.newShortcutRoles,
        editRoles: this.newShortcutEditRoles,
        description: this.newShortcutDescription
      };

      SettingsService.updateShortcut(this.editingShortcut, data).then((response) => {
        response.shortcut.id = this.editingShortcut; // server doesn't return id
        response.shortcut.type = this.newShortcutType; // server doesn't return type
        // update the shortcut in the table
        for (let i = 0; i < this.shortcuts.data.length; i++) {
          if (this.shortcuts.data[i].id === this.editingShortcut) {
            this.shortcuts.data[i] = response.shortcut;
          }
        }
        this.clearShortcutForm();
        this.editingShortcut = undefined;
        this.showShortcutModal = false;
        this.displaySuccess(response);
      }).catch((error) => {
        this.shortcutFormError = resolveMessage(error, this.$t);
        this.createShortcutLoading = false;
      });
    },
    /* deletes a shortcut and removes it from the shortcuts array */
    deleteShortcut (shortcut, index) {
      shortcut.loading = true;

      SettingsService.deleteShortcut(shortcut.id).then((response) => {
        // remove it from the array
        this.shortcuts.data.splice(index, 1);
        this.shortcuts.recordsTotal--;
        this.shortcuts.recordsFiltered--;
        shortcut.loading = false;
        this.displaySuccess(response);
      }).catch((error) => {
        // display error message to user
        this.$emit('display-message', { msg: resolveMessage(error, this.$t), type: 'danger' });
        shortcut.loading = false;
      });
    },
    /* helpers ------------------------------------------------------------- */
    getShortcuts () {
      const queryParams = {
        length: this.shortcutsSize,
        start: this.shortcutsStart,
        desc: this.shortcutsQuery.desc,
        sort: this.shortcutsQuery.sortField
      };

      if (this.seeAll) { queryParams.all = true; }
      if (this.userId) { queryParams.userId = this.userId; }
      if (this.shortcutsQuery.search) { queryParams.searchTerm = this.shortcutsQuery.search; }

      SettingsService.getShortcuts(queryParams).then((response) => {
        this.loading = false;
        this.shortcuts = response;
        this.shortcutsListError = '';
      }).catch((error) => {
        this.loading = false;
        this.shortcutsListError = resolveMessage(error, this.$t);
      });
    },
    /* validates the shortcut form. returns false if form is not valid and true otherwise.
     * sets the shortcut form error if the form in invalid */
    validShortcutForm () {
      if (!this.newShortcutName) {
        this.shortcutFormError = 'Shortcut name required';
        return false;
      }

      if (!this.newShortcutValue) {
        this.shortcutFormError = 'Shortcut value(s) required';
        return false;
      }

      if (!this.newShortcutType) {
        this.shortcutFormError = 'Shortcut type required';
        return false;
      }

      return true;
    },
    /* clear shortcut form inputs, errors, and loading state */
    clearShortcutForm () {
      this.shortcutFormError = '';
      this.newShortcutName = '';
      this.newShortcutValue = '';
      this.newShortcutUsers = '';
      this.newShortcutRoles = [];
      this.newShortcutEditRoles = [];
      this.newShortcutDescription = '';
      this.createShortcutLoading = false;
    },
    /* display success message to user and add any invalid users if they exist */
    displaySuccess (response) {
      let msg = resolveMessage(response, this.$t);
      if (response.invalidUsers && response.invalidUsers.length) {
        msg += ` Could not add these users: ${response.invalidUsers.join(',')}`;
      }
      this.$emit('display-message', { msg });
    }
  }
};
</script>

<style>
/* shortcuts table */
.settings-page .shortcut-value {
  max-width: 340px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.settings-page .shortcut-value.show-all {
  overflow: visible;
  white-space: normal;
}
.settings-page .shortcut-value.narrow {
  max-width: 160px;
}
.settings-page .shortcut-btns {
  min-width: 140px;
  white-space: nowrap;
}
</style>
