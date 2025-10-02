<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>

    <h3>
      {{ $t('settings.views.title') }}
      <BButton
        size="sm"
        variant="success"
        class="pull-right"
        @click="showViewModal = !showViewModal">
        <span class="fa fa-plus-circle me-1" />
        {{ $t('settings.views.newView') }}
      </BButton>
    </h3>

    <p>
      {{ $t('settings.views.info') }}
    </p>

    <div class="d-flex">
      <div class="flex-grow-1 me-2 mb-1">
        <b-input-group size="sm">
          <template #prepend>
            <b-input-group-text>
              <span class="fa fa-search" />
            </b-input-group-text>
          </template>
          <b-form-input
            debounce="400"
            :model-value="viewsQuery.search"
            @update:model-value="updateSearch"
            :placeholder="$t('settings.views.searchPlaceholder')"
          />
        </b-input-group>
      </div>
      <b-form-checkbox
        button
        size="sm"
        class="me-2"
        :model-value="seeAll"
        @update:model-value="updateSeeAll"
        id="seeAllViews"
        v-if="user.roles.includes('arkimeAdmin')">
        <span class="fa fa-user-circle me-1" />
        {{ $t(seeAll ? 'settings.views.seeMy' : 'settings.views.seeAll') }}
        <BTooltip target="seeAllViews">
          {{ $t(seeAll ? 'settings.views.seeMyTip' : 'settings.views.seeAllTip') }}
        </BTooltip>
      </b-form-checkbox>
      <arkime-paging
        v-if="views"
        :length-default="size"
        :records-total="recordsTotal"
        :records-filtered="recordsFiltered"
        @changePaging="changeViewsPaging">
      </arkime-paging>
    </div>

    <table class="table table-striped table-sm">
      <thead>
        <tr>
          <th class="cursor-pointer"
            @click.self="sortViews('name')">
            {{ $t('settings.views.table-name') }}
            <span v-show="viewsQuery.sortField === 'name' && !viewsQuery.desc" class="fa fa-sort-asc"></span>
            <span v-show="viewsQuery.sortField === 'name' && viewsQuery.desc" class="fa fa-sort-desc"></span>
            <span v-show="viewsQuery.sortField !== 'name'" class="fa fa-sort"></span>
          </th>
          <th>{{ $t('settings.views.table-creator') }}</th>
          <th>{{ $t('settings.views.table-expression') }}</th>
          <th width="30%">{{ $t('settings.views.table-columns') }}</th>
          <th>{{ $t('settings.views.table-sort') }}</th>
          <th>&nbsp;</th>
        </tr>
      </thead>
      <tbody>
        <!-- views -->
        <tr :key="item.id"
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
              <template v-for="col in item.sessionsColConfig.visibleHeaders" :key="col">
                <label class="badge bg-secondary me-1 mb-0 help-cursor"
                  v-if="fieldsMap[col]"
                  :id="`viewField-${col}`">
                  {{ fieldsMap[col].friendlyName }}
                  <BTooltip target="viewField-{{col}}">
                    {{ fieldsMap[col].help }}
                  </BTooltip>
                </label>
              </template>
            </span>
          </td>
          <td>
            <span v-if="item.sessionsColConfig">
              <template v-for="order in item.sessionsColConfig.order" :key="order[0]">
                <label class="badge bg-secondary me-1 help-cursor"
                  v-if="fieldsMap[order[0]]"
                  :id="`viewFieldOrder-${order[0]}`">
                  {{ fieldsMap[order[0]].friendlyName }}&nbsp;
                  ({{ order[1] }})
                  <BTooltip target="viewFieldOrder-{{order[0]}}">
                    {{ fieldsMap[order[0]].help }}
                  </BTooltip>
                </label>
              </template>
            </span>
          </td>
          <td>
            <span class="pull-right no-wrap">
              <b-button
                size="sm"
                class="ms-1"
                :id="`copyView-${item.id}`"
                variant="theme-secondary"
                @click="$emit('copy-value', item.expression)">
                <span class="fa fa-clipboard fa-fw" />
                <BTooltip :target="`copyView-${item.id}`">
                  {{ $t('settings.views.copyTip') }}
                </BTooltip>
              </b-button>
              <template
                v-if="canEdit(item)">
                <b-button
                  size="sm"
                  class="ms-1"
                  variant="info"
                  :id="`transferView-${item.id}`"
                  v-if="canTransfer(item)"
                  @click="openTransferView(item)">
                  <span class="fa fa-share fa-fw" />
                  <BTooltip :target="`transferView-${item.id}`">
                    {{ $t('settings.views.transferTip') }}
                  </BTooltip>
                </b-button>
                <b-button
                  size="sm"
                  class="ms-1"
                  variant="danger"
                  :id="`deleteView-${item.id}`"
                  @click="deleteView(item.id, index)">
                  <span class="fa fa-trash-o fa-fw" />
                  <BTooltip :target="`deleteView-${item.id}`">
                    {{ $t('settings.views.deleteTip') }}
                  </BTooltip>
                </b-button>
                <b-button
                  size="sm"
                  class="ms-1"
                  :id="`editView-${item.id}`"
                  @click="editView(item)"
                  variant="theme-tertiary">
                  <span class="fa fa-pencil fa-fw" />
                  <BTooltip :target="`editView-${item.id}`">
                    {{ $t('settings.views.editTip') }}
                  </BTooltip>
                </b-button>
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
    <div class="text-center mt-4"
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
    <BModal
      size="xl"
      :model-value="showViewModal"
      :title="$t(editingView ? 'settings.views.editView' : 'settings.views.newView')">
      <b-input-group
        size="sm"
        class="mb-2">
        <b-input-group-text
          id="viewFormName"
          class="cursor-help">
          {{ $t('settings.views.viewFormName') }}<sup>*</sup>
          <BTooltip target="viewFormName"><span v-i18n-btip="'settings.views.'" /></BTooltip>
        </b-input-group-text>
        <b-form-input
          :model-value="newViewName"
          @update:model-value="newViewName = $event"
          :placeholder="$t('settings.views.viewFormNamePlaceholder')"
        />
      </b-input-group>
      <b-input-group
        size="sm"
        class="mb-2">
        <b-input-group-text
          id="viewFormExpression"
          class="cursor-help">
          {{ $t('settings.views.viewFormExpression') }}<sup>*</sup>
          <BTooltip target="viewFormExpression"><span v-i18n-btip="'settings.views.'" /></BTooltip>
        </b-input-group-text>
        <b-form-input
          :model-value="newViewExpression"
          @update:model-value="newViewExpression = $event"
          :placeholder="$t('settings.views.viewFormExpressionPlaceholder')"
        />
      </b-input-group>
      <div class="d-flex">
        <div class="me-3 flex-grow-1 no-wrap">
          <RoleDropdown
            :roles="roles"
            class="d-inline"
            :selected-roles="newViewRoles"
            :display-text="$t('common.rolesCanView')"
            @selected-roles-updated="updateNewViewRoles"
          />
          <RoleDropdown
            :roles="roles"
            class="d-inline ms-1"
            :display-text="$t('common.rolesCanEdit')"
            :selected-roles="newViewEditRoles"
            @selected-roles-updated="updateNewViewEditRoles"
          />
        </div>
        <b-input-group
          size="sm">
          <b-input-group-text
            id="viewFormUsers"
            class="cursor-help">
            {{ $t('common.shareWithUsers') }}
            <BTooltip target="viewFormUsers"><span v-i18n-btip="'settings.views.'" /></BTooltip>
          </b-input-group-text>
          <b-form-input
            :model-value="newViewUsers"
            @update:model-value="newViewUsers = $event"
            :placeholder="$t('settings.views.viewFormUsersPlaceholder')"
          />
        </b-input-group>
      </div>
      <!-- form error -->
      <div
        v-if="viewFormError"
        class="alert alert-danger alert-sm mt-2 mb-0">
        <span class="fa fa-exclamation-triangle me-1" />
        {{ viewFormError }}
      </div> <!-- /form error -->
      <template #footer>
        <div class="w-100 d-flex justify-content-between">
          <b-button
            variant="danger"
            @click="showViewModal = false">
            <span class="fa fa-times" />
            {{ $t('common.cancel') }}
          </b-button>
          <b-button
            variant="success"
            @click="createView"
            v-if="!editingView">
            <span class="fa fa-plus-circle me-1" />
            {{ $t('common.create') }}
          </b-button>
          <b-button
            v-else
            variant="success"
            @click="updateView">
            <span class="fa fa-save me-1" />
            {{ $t('common.save') }}
          </b-button>
        </div>
      </template> <!-- /modal footer -->
    </BModal> <!-- /new view form -->

    <transfer-resource
      :show-modal="showTransferModal"
      @transfer-resource="submitTransferView"
    />

  </div>
</template>

<script>
// services
import SettingsService from './SettingsService';
import UserService from '@common/UserService';
// components
import ArkimePaging from '../utils/Pagination.vue';
import RoleDropdown from '@common/RoleDropdown.vue';
import TransferResource from '@common/TransferResource.vue';

export default {
  name: 'Views',
  components: {
    ArkimePaging,
    RoleDropdown,
    TransferResource
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
    userId: String, // the setting user id
    fieldsMap: Object // the map of fields to field objects
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
        this.displaySuccess(response);
      }).catch((error) => {
        this.viewFormError = error.text;
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
        this.$emit('display-message', { msg: response.text, type: 'success' });
        this.showTransferModal = false;
      }).catch((error) => {
        this.$emit('display-message', { msg: error.text, type: 'danger' });
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
        this.$emit('display-message', { msg: response.text, type: 'success' });
      }).catch((error) => {
        // display error message to user
        this.$emit('display-message', { msg: error.text, type: 'danger' });
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
        this.viewFormError = error.text;
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
        this.viewListError = error.text;
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
      this.viewFormError = false;
      this.newViewRoles = [];
      this.newViewUsers = '';
      this.newViewName = null;
      this.newViewEditRoles = [];
      this.newViewExpression = null;
    },
    /* display success message to user and add any invalid users if they exist */
    displaySuccess (response) {
      let msg = response.text;
      if (response.invalidUsers && response.invalidUsers.length) {
        msg += ` Could not add these users: ${response.invalidUsers.join(',')}`;
      }
      this.$emit('display-message', { msg });
    }
  }
};
</script>
