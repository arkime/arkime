<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>

    <h3>
      Views
      <b-button
        size="sm"
        variant="success"
        class="pull-right"
        v-b-modal.view-modal>
        <span class="fa fa-plus-circle mr-1" />
        New View
      </b-button>
    </h3>

    <p>
      Saved views provide an easier method to specify common base queries
      and can be activated in the search bar.
    </p>

    <div class="d-flex">
      <div class="flex-grow-1 mr-2">
        <b-input-group size="sm">
          <template #prepend>
            <b-input-group-text>
              <span class="fa fa-search" />
            </b-input-group-text>
          </template>
          <b-form-input
            debounce="400"
            v-model="viewsQuery.search"
          />
        </b-input-group>
      </div>
      <b-form-checkbox
        button
        size="sm"
        class="mr-2"
        v-model="seeAll"
        @input="getViews"
        v-b-tooltip.hover
        v-if="user.roles.includes('arkimeAdmin')"
        :title="seeAll ? 'Just show the views created by you and shared with you' : 'See all the views that exist for all users (you can because you are an ADMIN!)'">
        <span class="fa fa-user-circle mr-1" />
        See {{ seeAll ? ' MY ' : ' ALL ' }} Views
      </b-form-checkbox>
      <moloch-paging
        v-if="views"
        :length-default="size"
        :records-total="recordsTotal"
        :records-filtered="recordsFiltered"
        @changePaging="changeViewsPaging">
      </moloch-paging>
    </div>

    <table class="table table-striped table-sm">
      <thead>
        <tr>
          <th class="cursor-pointer"
            @click.self="sortViews('name')">
            Name
            <span v-show="viewsQuery.sortField === 'name' && !viewsQuery.desc" class="fa fa-sort-asc"></span>
            <span v-show="viewsQuery.sortField === 'name' && viewsQuery.desc" class="fa fa-sort-desc"></span>
            <span v-show="viewsQuery.sortField !== 'name'" class="fa fa-sort"></span>
          </th>
          <th>Creator</th>
          <th>Expression</th>
          <th width="30%">Sessions Columns</th>
          <th>Sessions Sort</th>
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
              <template v-for="col in item.sessionsColConfig.visibleHeaders">
                <label class="badge badge-secondary mr-1 mb-0 help-cursor"
                  v-if="fieldsMap[col]"
                  v-b-tooltip.hover
                  :title="fieldsMap[col].help"
                  :key="col">
                  {{ fieldsMap[col].friendlyName }}
                </label>
              </template>
            </span>
          </td>
          <td>
            <span v-if="item.sessionsColConfig">
              <template v-for="order in item.sessionsColConfig.order">
                <label class="badge badge-secondary mr-1 help-cursor"
                  :title="fieldsMap[order[0]].help"
                  v-if="fieldsMap[order[0]]"
                  v-b-tooltip.hover
                  :key="order[0]">
                  {{ fieldsMap[order[0]].friendlyName }}&nbsp;
                  ({{ order[1] }})
                </label>
              </template>
            </span>
          </td>
          <td>
            <span class="pull-right no-wrap">
              <b-button
                size="sm"
                v-b-tooltip.hover
                variant="theme-secondary"
                title="Copy this views's expression"
                @click="$emit('copy-value', item.expression)">
                <span class="fa fa-clipboard fa-fw" />
              </b-button>
              <template
                v-if="canEdit(item)">
                <b-button
                  size="sm"
                  variant="info"
                  v-b-tooltip.hover
                  v-if="canTransfer(item)"
                  title="Transfer ownership of this view"
                  @click="openTransferView(item)">
                  <span class="fa fa-share fa-fw" />
                </b-button>
                <b-button
                  size="sm"
                  variant="danger"
                  v-b-tooltip.hover
                  title="Delete this view"
                  @click="deleteView(item.id, index)">
                  <span class="fa fa-trash-o fa-fw" />
                </b-button>
                <b-button
                  size="sm"
                  v-b-tooltip.hover
                  @click="editView(item)"
                  variant="theme-tertiary"
                  title="Update this view">
                  <span class="fa fa-pencil fa-fw" />
                </b-button>
              </template>
            </span>
          </td>
        </tr> <!-- /views -->
      </tbody>
    </table>

    <!-- view list error -->
    <b-alert
      variant="danger"
      class="mt-2 mb-2"
      :show="!!viewListError">
      <span class="fa fa-exclamation-triangle mr-1" />
      {{ viewListError }}
    </b-alert> <!-- /view list error -->

    <!-- no results -->
    <div class="text-center mt-4"
      v-if="!views || !views.length">
      <h3>
        <span class="fa fa-folder-open fa-2x" />
      </h3>
      <h5>
        No views or none that match your search.
        <br>
        Create one by clicking the create button above.
      </h5>
    </div> <!-- /no results -->

    <!-- new view form -->
    <b-modal
      size="xl"
      id="view-modal"
      :title="editingView ? 'Edit View' : 'Create New View'">
      <b-input-group
        size="sm"
        class="mb-2">
        <template #prepend>
          <b-input-group-text
            v-b-tooltip.hover
            class="cursor-help"
            title="Enter a descriptive name">
            Name<sup>*</sup>
          </b-input-group-text>
        </template>
        <b-form-input
          v-model="newViewName"
          placeholder="View name (20 chars or less)"
        />
      </b-input-group>
      <b-input-group
        size="sm"
        class="mb-2">
        <template #prepend>
          <b-input-group-text
            v-b-tooltip.hover
            class="cursor-help"
            title="Enter a sessions search expression">
            Search Expression<sup>*</sup>
          </b-input-group-text>
        </template>
        <b-form-input
          v-model="newViewExpression"
          placeholder="View expression"
        />
      </b-input-group>
      <div class="d-flex">
        <div class="mr-3 flex-grow-1 no-wrap">
          <RoleDropdown
            :roles="roles"
            :selected-roles="newViewRoles"
            display-text="Who can view"
            @selected-roles-updated="updateNewViewRoles"
          />
          <RoleDropdown
            :roles="roles"
            display-text="Who can edit"
            :selected-roles="newViewEditRoles"
            @selected-roles-updated="updateNewViewEditRoles"
          />
        </div>
        <b-input-group
          size="sm">
          <template #prepend>
            <b-input-group-text
              v-b-tooltip.hover
              class="cursor-help"
              title="Enter a comma separated list of users that can use this view">
              Share with users
            </b-input-group-text>
          </template>
          <b-form-input
            v-model="newViewUsers"
            placeholder="Comma separated list of users"
          />
        </b-input-group>
      </div>
      <!-- form error -->
      <b-alert
        variant="danger"
        class="mt-2 mb-0"
        :show="!!viewFormError">
        <span class="fa fa-exclamation-triangle mr-1" />
        {{ viewFormError }}
      </b-alert> <!-- /form error -->
      <template #modal-footer>
        <div class="w-100 d-flex justify-content-between">
          <b-button
            title="Cancel"
            variant="danger"
            @click="$bvModal.hide('view-modal')">
            <span class="fa fa-times" />
            Cancel
          </b-button>
          <b-button
            variant="success"
            v-b-tooltip.hover
            @click="createView"
            v-if="!editingView"
            title="Create new view">
            <span class="fa fa-plus-circle mr-1" />
            Create
          </b-button>
          <b-button
            v-else
            variant="success"
            v-b-tooltip.hover
            @click="updateView"
            title="Update view">
            <span class="fa fa-save mr-1" />
            Save
          </b-button>
        </div>
      </template> <!-- /modal footer -->
    </b-modal> <!-- /new view form -->

    <transfer-resource
      @transfer-resource="submitTransferView"
    />

  </div>
</template>

<script>
// services
import SettingsService from './SettingsService';
import UserService from '../../../../../common/vueapp/UserService';
// components
import MolochPaging from '../utils/Pagination';
import RoleDropdown from '../../../../../common/vueapp/RoleDropdown';
import TransferResource from '../../../../../common/vueapp/TransferResource';

export default {
  name: 'Views',
  components: {
    MolochPaging,
    RoleDropdown,
    TransferResource
  },
  data () {
    return {
      editingView: undefined,
      viewListError: '',
      viewFormError: '',
      newViewName: '',
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
          this.$set(view, 'roles', roles);
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
        this.$bvModal.hide('view-modal');
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
      this.$bvModal.show('transfer-modal');
    },
    /**
     * Submits the transfer resource modal contents and updates the view
     * @param {Object} userId The user id to transfer the view to
     */
    submitTransferView ({ userId }) {
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
        this.$bvModal.hide('transfer-modal');
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
      this.$bvModal.show('view-modal');
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
            this.$set(this.views, i, response.view);
          }
        }
        this.clearViewForm();
        this.editingView = undefined;
        this.$bvModal.hide('view-modal');
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
