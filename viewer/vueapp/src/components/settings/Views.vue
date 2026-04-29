<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <h3>
      {{ $t('settings.views.title') }}
      <button
        type="button"
        class="btn btn-sm btn-success pull-right"
        @click="showViewModal = !showViewModal">
        <span class="fa fa-plus-circle me-1" />
        {{ $t('settings.views.newView') }}
      </button>
    </h3>

    <p>
      {{ $t('settings.views.info') }}
    </p>

    <div class="d-flex align-items-center">
      <div class="flex-grow-1 me-2">
        <v-text-field
          density="compact"
          variant="outlined"
          hide-details
          prepend-inner-icon="fa-search"
          :model-value="viewsQuery.search"
          @update:model-value="updateSearch"
          :placeholder="$t('settings.views.searchPlaceholder')" />
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
        <v-btn value="seeAll" id="seeAllViews">
          <span class="fa fa-user-circle me-1" />
          {{ $t(seeAll ? 'settings.views.seeMy' : 'settings.views.seeAll') }}
          <v-tooltip activator="parent" location="top">
            {{ $t(seeAll ? 'settings.views.seeMyTip' : 'settings.views.seeAllTip') }}
          </v-tooltip>
        </v-btn>
      </v-btn-toggle>
      <arkime-paging
        v-if="views"
        :length-default="size"
        :records-total="recordsTotal"
        :records-filtered="recordsFiltered"
        @change-paging="changeViewsPaging" />
    </div>

    <table class="table table-striped table-sm">
      <thead>
        <tr>
          <th
            class="cursor-pointer"
            @click.self="sortViews('name')">
            {{ $t('settings.views.table-name') }}
            <span
              v-show="viewsQuery.sortField === 'name' && !viewsQuery.desc"
              class="fa fa-sort-asc" />
            <span
              v-show="viewsQuery.sortField === 'name' && viewsQuery.desc"
              class="fa fa-sort-desc" />
            <span
              v-show="viewsQuery.sortField !== 'name'"
              class="fa fa-sort" />
          </th>
          <th>{{ $t('settings.views.table-creator') }}</th>
          <th>{{ $t('settings.views.table-expression') }}</th>
          <th width="30%">
            {{ $t('settings.views.table-columns') }}
          </th>
          <th>{{ $t('settings.views.table-sort') }}</th>
          <th>&nbsp;</th>
        </tr>
      </thead>
      <tbody>
        <!-- views -->
        <tr
          :key="item.id"
          v-for="(item, index) in views">
          <td>
            {{ item.name }}
          </td>
          <td>
            {{ item.user }}
          </td>
          <td>
            {{ item.expression }}
          </td>
          <td>
            <span v-if="item.sessionsColConfig">
              <template
                v-for="col in item.sessionsColConfig.visibleHeaders"
                :key="col">
                <label
                  class="badge bg-secondary me-1 mb-0 help-cursor"
                  v-if="fieldsMap[col]"
                  :id="`viewField-${col}`">
                  {{ fieldsMap[col].friendlyName }}
                  <v-tooltip :activator="`[id='viewField-${col}']`">
                    {{ fieldsMap[col].help }}
                  </v-tooltip>
                </label>
              </template>
            </span>
          </td>
          <td>
            <span v-if="item.sessionsColConfig">
              <template
                v-for="order in item.sessionsColConfig.order"
                :key="order[0]">
                <label
                  class="badge bg-secondary me-1 help-cursor"
                  v-if="fieldsMap[order[0]]"
                  :id="`viewFieldOrder-${order[0]}`">
                  {{ fieldsMap[order[0]].friendlyName }}&nbsp;
                  ({{ order[1] }})
                  <v-tooltip :activator="`[id='viewFieldOrder-${order[0]}']`">
                    {{ fieldsMap[order[0]].help }}
                  </v-tooltip>
                </label>
              </template>
            </span>
          </td>
          <td>
            <span class="pull-right no-wrap">
              <button
                type="button"
                class="btn btn-sm btn-theme-secondary ms-1"
                :id="`copyView-${item.id}`"
                @click="$emit('copy-value', item.expression)">
                <span class="fa fa-clipboard fa-fw" />
                <v-tooltip :activator="`#copyView-${item.id}`">
                  {{ $t('settings.views.copyTip') }}
                </v-tooltip>
              </button>
              <template
                v-if="canEdit(item)">
                <button
                  type="button"
                  class="btn btn-sm btn-info ms-1"
                  :id="`transferView-${item.id}`"
                  v-if="canTransfer(item)"
                  @click="openTransferView(item)">
                  <span class="fa fa-share fa-fw" />
                  <v-tooltip :activator="`#transferView-${item.id}`">
                    {{ $t('settings.views.transferTip') }}
                  </v-tooltip>
                </button>
                <button
                  type="button"
                  class="btn btn-sm btn-danger ms-1"
                  :id="`deleteView-${item.id}`"
                  @click="deleteView(item.id, index)">
                  <span class="fa fa-trash-o fa-fw" />
                  <v-tooltip :activator="`#deleteView-${item.id}`">
                    {{ $t('settings.views.deleteTip') }}
                  </v-tooltip>
                </button>
                <button
                  type="button"
                  class="btn btn-sm btn-theme-tertiary ms-1"
                  :id="`editView-${item.id}`"
                  @click="editView(item)">
                  <span class="fa fa-pencil fa-fw" />
                  <v-tooltip :activator="`#editView-${item.id}`">
                    {{ $t('settings.views.editTip') }}
                  </v-tooltip>
                </button>
              </template>
            </span>
          </td>
        </tr> <!-- /views -->
      </tbody>
    </table>

    <!-- view list error -->
    <div
      v-if="viewListError"
      style="z-index: 2000;"
      class="mt-2 mb-2 alert alert-danger">
      <span class="fa fa-exclamation-triangle me-1" />
      {{ viewListError }}
    </div> <!-- /view list error -->

    <!-- no results -->
    <div
      class="text-center mt-4"
      v-if="!views || !views.length">
      <h3>
        <span class="fa fa-folder-open fa-2x" />
      </h3>
      <h5>
        {{ $t('settings.views.noMatch') }}
        <br>
        {{ $t('settings.views.useCreate') }}
      </h5>
    </div> <!-- /no results -->

    <!-- new view form -->
    <v-dialog
      :model-value="showViewModal"
      @update:model-value="(val) => { if (!val) showViewModal = false; }"
      max-width="1140">
      <v-card density="compact">
        <v-card-title>
          {{ $t(editingView ? 'settings.views.editView' : 'settings.views.newView') }}
        </v-card-title>
        <v-card-text>
          <div class="input-group input-group-sm mb-2">
            <span id="viewFormName" class="input-group-text cursor-help">
              {{ $t('settings.views.viewFormName') }}<sup>*</sup>
              <v-tooltip activator="#viewFormName">
                {{ $t('settings.views.viewFormNameTip') }}
              </v-tooltip>
            </span>
            <input
              type="text"
              class="form-control"
              :value="newViewName"
              @input="newViewName = $event.target.value"
              :placeholder="$t('settings.views.viewFormNamePlaceholder')">
          </div>
          <div class="input-group input-group-sm mb-2">
            <span id="viewFormExpression" class="input-group-text cursor-help">
              {{ $t('settings.views.viewFormExpression') }}<sup>*</sup>
              <v-tooltip activator="#viewFormExpression">
                {{ $t('settings.views.viewFormExpressionTip') }}
              </v-tooltip>
            </span>
            <ExpressionAutocompleteInput
              textarea
              rows="3"
              :model-value="newViewExpression"
              @update:model-value="newViewExpression = $event"
              :placeholder="$t('settings.views.viewFormExpressionPlaceholder')" />
          </div>
          <div class="d-flex">
            <div class="me-3 flex-grow-1 no-wrap">
              <RoleDropdown
                :roles="roles"
                class="d-inline"
                :selected-roles="newViewRoles"
                :display-text="$t('common.rolesCanView')"
                @selected-roles-updated="updateNewViewRoles" />
              <RoleDropdown
                :roles="roles"
                class="d-inline ms-1"
                :display-text="$t('common.rolesCanEdit')"
                :selected-roles="newViewEditRoles"
                @selected-roles-updated="updateNewViewEditRoles" />
            </div>
            <div class="input-group input-group-sm">
              <span id="viewFormUsers" class="input-group-text cursor-help">
                {{ $t('common.shareWithUsers') }}
                <v-tooltip activator="#viewFormUsers">
                  {{ $t('settings.views.viewFormUsersTip') }}
                </v-tooltip>
              </span>
              <input
                type="text"
                class="form-control"
                :value="newViewUsers"
                @input="newViewUsers = $event.target.value"
                :placeholder="$t('settings.views.viewFormUsersPlaceholder')">
            </div>
          </div>
          <!-- form error -->
          <div
            v-if="viewFormError"
            class="alert alert-danger alert-sm mt-2 mb-0">
            <span class="fa fa-exclamation-triangle me-1" />
            {{ viewFormError }}
          </div> <!-- /form error -->
        </v-card-text>
        <v-card-actions>
          <div class="w-100 d-flex justify-content-between">
            <button
              type="button"
              class="btn btn-danger"
              @click="showViewModal = false">
              <span class="fa fa-times" />
              {{ $t('common.cancel') }}
            </button>
            <button
              type="button"
              class="btn btn-success"
              @click="createView"
              v-if="!editingView">
              <span class="fa fa-plus-circle me-1" />
              {{ $t('common.create') }}
            </button>
            <button
              type="button"
              class="btn btn-success"
              v-else
              @click="updateView">
              <span class="fa fa-save me-1" />
              {{ $t('common.save') }}
            </button>
          </div>
        </v-card-actions>
      </v-card>
    </v-dialog> <!-- /new view form -->

    <transfer-resource
      :show-modal="showTransferModal"
      @transfer-resource="submitTransferView" />
  </div>
</template>

<script>
// services
import SettingsService from './SettingsService';
import UserService from '@common/UserService';
// utilities
import { resolveMessage } from '@common/resolveI18nMessage';
// components
import ArkimePaging from '../utils/Pagination.vue';
import RoleDropdown from '@common/RoleDropdown.vue';
import TransferResource from '@common/TransferResource.vue';
import ExpressionAutocompleteInput from '../search/ExpressionAutocompleteInput.vue';

export default {
  name: 'Views',
  emits: ['copy-value', 'display-message'],
  components: {
    ArkimePaging,
    RoleDropdown,
    TransferResource,
    ExpressionAutocompleteInput
  },
  data () {
    return {
      showViewModal: false,
      showTransferModal: false,
      editingView: undefined,
      viewListError: '',
      viewFormError: '',
      newViewName: undefined,
      newViewExpression: '',
      newViewRoles: [],
      newViewEditRoles: [],
      newViewUsers: '',
      size: 50,
      start: 0,
      viewsQuery: {
        search: '',
        desc: false,
        sortField: 'name'
      },
      recordsTotal: 0,
      recordsFiltered: 0,
      seeAll: false,
      transferView: undefined
    };
  },
  props: {
    userId: {
      type: String,
      default: ''
    }, // the setting user id
    fieldsMap: {
      type: Object,
      default: () => ({})
    } // the map of fields to field objects
  },
  computed: {
    views: {
      get () {
        return this.$store.state.views;
      },
      set (newValue) {
        this.$store.commit('setViews', newValue);
      }
    },
    user () {
      return this.$store.state.user;
    },
    roles () {
      return this.$store.state.roles;
    }
  },
  watch: {
    'viewsQuery.search' () {
      this.getViews();
    }
  },
  mounted () {
    // need to fetch views even though they're in the store because
    // we might be getting views for another user
    this.getViews();
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    /**
     * triggered when a sortable column is clicked
     * if the sort field is the same as the current sort field, toggle the desc
     * flag, otherwise set it to default (false)
     * @param {string} sort The field to sort on
     */
    sortViews (sort) {
      this.viewsQuery.desc = this.viewsQuery.sortField === sort ? !this.viewsQuery.desc : false;
      this.viewsQuery.sortField = sort;
      this.getViews();
    },
    /**
     * triggered when paging is changed
     * @param {object} newParams Object containing length & start
     */
    changeViewsPaging (newParams) {
      this.size = newParams.length;
      this.start = newParams.start;
      this.getViews();
    },
    updateSearch (newSearch) {
      this.viewsQuery.search = newSearch; // NOTE watch will trigger getViews
    },
    updateSeeAll (newSeeAll) {
      this.seeAll = newSeeAll;
      this.getViews();
    },
    canEdit (view) {
      return this.user.roles.includes('arkimeAdmin') ||
        (view.user && view.user === this.user.userId) ||
        (view.editRoles && UserService.hasRole(this.user, view.editRoles.join(',')));
    },
    canTransfer (view) {
      return this.user.roles.includes('arkimeAdmin') ||
        (view.user && view.user === this.user.userId);
    },
    /* updates the roles on a view object from the RoleDropdown component */
    updateViewRoles (roles, id) {
      for (const view of this.views) {
        if (view.id === id) {
          view.roles = roles;
          return;
        }
      }
    },
    updateNewViewRoles (roles) {
      this.newViewRoles = roles;
    },
    updateNewViewEditRoles (roles) {
      this.newViewEditRoles = roles;
    },
    /* creates a view given the view name and expression */
    createView () {
      if (!this.validViewForm()) { return; }

      const data = {
        name: this.newViewName,
        roles: this.newViewRoles,
        users: this.newViewUsers,
        editRoles: this.newViewEditRoles,
        expression: this.newViewExpression
      };

      SettingsService.createView(data, this.userId).then((response) => {
        this.clearViewForm();
        this.views.push(response.view);
        this.showViewModal = false;
        this.displaySuccess(response);
      }).catch((error) => {
        this.viewFormError = resolveMessage(error, this.$t);
      });
    },
    /**
     * Opens the transfer resource modal
     * @param {Object} view The view to transfer
     */
    openTransferView (view) {
      this.transferView = view;
      this.showTransferModal = true;
    },
    /**
     * Submits the transfer resource modal contents and updates the view
     * @param {Object} userId The user id to transfer the view to
     */
    submitTransferView ({ userId }) {
      this.showTransferModal = false;

      if (!userId) {
        this.transferView = undefined;
        return;
      }

      const data = JSON.parse(JSON.stringify(this.transferView));
      data.user = userId;

      SettingsService.updateView(data, this.userId).then((response) => {
        this.getViews();
        this.transferView = undefined;
        this.$emit('display-message', { msg: resolveMessage(response, this.$t), type: 'success' });
        this.showTransferModal = false;
      }).catch((error) => {
        this.$emit('display-message', { msg: resolveMessage(error, this.$t), type: 'danger' });
      });
    },
    /**
     * Deletes a view given its name
     * @param {Object} viewId The id of the view to delete
     * @param {number} index The index of the view to delete
     */
    deleteView (viewId, index) {
      SettingsService.deleteView(viewId, this.userId).then((response) => {
        // remove the view from the view list
        this.views.splice(index, 1);
        // display success message to user
        this.$emit('display-message', { msg: resolveMessage(response, this.$t), type: 'success' });
      }).catch((error) => {
        // display error message to user
        this.$emit('display-message', { msg: resolveMessage(error, this.$t), type: 'danger' });
      });
    },
    /* opens up modal to edit view */
    editView (view) {
      this.viewFormError = '';
      this.editingView = view.id;
      this.newViewName = view.name || '';
      this.newViewRoles = view.roles || [];
      this.newViewUsers = view.users || '';
      this.newViewEditRoles = view.editRoles || [];
      this.newViewExpression = view.expression || '';
      this.showViewModal = true;
    },
    /* Updates a view */
    updateView () {
      if (!this.validViewForm()) { return; }

      const data = {
        id: this.editingView,
        name: this.newViewName,
        roles: this.newViewRoles,
        users: this.newViewUsers,
        editRoles: this.newViewEditRoles,
        expression: this.newViewExpression
      };

      SettingsService.updateView(data, this.userId).then((response) => {
        // update the view in the table
        for (let i = 0; i < this.views.length; i++) {
          if (this.views[i].id === this.editingView) {
            this.views[i] = response.view;
          }
        }
        this.clearViewForm();
        this.editingView = undefined;
        this.showViewModal = false;
        this.displaySuccess(response);
      }).catch((error) => {
        this.viewFormError = resolveMessage(error, this.$t);
      });
    },
    /* helper functions ---------------------------------------------------- */
    /* retrieves the specified user's views */
    getViews () {
      const queryParams = {
        length: this.size,
        start: this.start,
        desc: this.viewsQuery.desc,
        sort: this.viewsQuery.sortField
      };

      if (this.seeAll) { queryParams.all = true; }
      if (this.userId) { queryParams.userId = this.userId; }
      if (this.viewsQuery.search) { queryParams.searchTerm = this.viewsQuery.search; }

      SettingsService.getViews(queryParams).then((response) => {
        this.viewListError = '';
        this.recordsTotal = response.recordsTotal;
        this.recordsFiltered = response.recordsFiltered;
      }).catch((error) => {
        this.viewListError = resolveMessage(error, this.$t);
      });
    },
    /* validates the view form. returns false if form is not valid and true otherwise.
     * sets the view form error if the form in invalid */
    validViewForm () {
      this.viewFormError = '';

      if (!this.newViewName) {
        this.viewFormError = 'View name required';
        return false;
      }

      if (!this.newViewExpression) {
        this.viewFormError = 'View expression required';
        return false;
      }

      return true;
    },
    clearViewForm () {
      this.viewFormError = '';
      this.newViewRoles = [];
      this.newViewUsers = '';
      this.newViewName = null;
      this.newViewEditRoles = [];
      this.newViewExpression = null;
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
