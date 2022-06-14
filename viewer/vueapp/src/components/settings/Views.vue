<template>
  <form class="form-horizontal">

    <h3>Views</h3>

    <p>
        Saved views provide an easier method to specify common base queries
        and can be activated in the search bar.
    </p>

    <div class="row mb-2">
      <div class="col-5">
        <div class="input-group input-group-sm">
          <div class="input-group-prepend">
            <div class="input-group-text">
              <span class="fa fa-search"></span>
            </div>
          </div>
          <b-form-input
            debounce="400"
            v-model="viewsQuery.search"
          />
        </div>
      </div>
      <div class="col-7">
        <moloch-paging
          v-if="views"
          class="pull-right"
          :length-default="size"
          :records-total="recordsTotal"
          :records-filtered="recordsFiltered"
          @changePaging="changeViewsPaging">
        </moloch-paging>
      </div>
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
          <th>Roles</th>
          <th>Users</th>
          <th>Expression</th>
          <th width="30%">Sessions Columns</th>
          <th>Sessions Sort</th>
          <th>&nbsp;</th>
        </tr>
      </thead>
      <tbody>
        <!-- views -->
        <tr v-for="(item, index) in views"
          @keyup.enter="updateView(index)"
          @keyup.esc="cancelViewChange(index)"
          :key="index">
          <td>
            <input
              type="text"
              maxlength="20"
              v-model="item.name"
              @input="viewChanged(index)"
              :disabled="!canEditView(item)"
              class="form-control form-control-sm"
            />
          </td>
          <td>
            <template v-if="canEditView(item)">
              <RoleDropdown
                :id="item.id"
                :roles="roles"
                :selected-roles="item.roles"
                @selected-roles-updated="updateViewRoles"
                :display-text="item.roles && item.roles.length ? undefined : 'Share with roles'"
              />
            </template>
          </td>
          <td>
            <template v-if="canEditView(item)">
              <input type="text"
                v-model="item.users"
                @input="viewChanged(index)"
                class="form-control form-control-sm"
                placeholder="Comma separated list of users to share this view with"
              />
            </template>
          </td>
          <td>
            <input type="text"
              v-model="item.expression"
              @input="viewChanged(index)"
              :disabled="!canEditView(item)"
              class="form-control form-control-sm"
            />
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
            <span class="pull-right">
              <button type="button"
                v-b-tooltip.hover
                title="Copy this views's expression"
                class="btn btn-sm btn-theme-secondary"
                @click="$emit('copy-value', item.expression)">
                <span class="fa fa-clipboard fa-fw">
                </span>
              </button>
              <span v-if="canEditView(item)">
                <span v-if="item.changed">
                  <button type="button"
                    v-b-tooltip.hover
                    @click="updateView(index)"
                    title="Save changes to this view"
                    class="btn btn-sm btn-theme-tertiary">
                    <span class="fa fa-save fa-fw">
                    </span>
                  </button>
                  <button type="button"
                    v-b-tooltip.hover
                    class="btn btn-sm btn-warning"
                    @click="cancelViewChange(index)"
                    title="Undo changes to this view">
                    <span class="fa fa-ban fa-fw">
                    </span>
                  </button>
                </span>
                <button v-else
                  type="button"
                  v-b-tooltip.hover
                  title="Delete this view"
                  class="btn btn-sm btn-danger"
                  @click="deleteView(item.id, index)">
                  <span class="fa fa-trash-o fa-fw">
                  </span>
                </button>
              </span>
            </span>
          </td>
        </tr> <!-- /views -->
        <!-- view list error -->
        <tr v-if="viewListError">
          <td colspan="7">
            <p class="text-danger mb-0">
              <span class="fa fa-exclamation-triangle">
              </span>&nbsp;
              {{ viewListError }}
            </p>
          </td>
        </tr> <!-- /view list error -->
        <!-- new view form -->
        <tr @keyup.enter="createView">
          <td>
            <input type="text"
              maxlength="20"
              v-model="newViewName"
              class="form-control form-control-sm"
              placeholder="Enter a new view name (20 chars or less)"
            />
          </td>
          <td>
            <RoleDropdown
              :roles="roles"
              :selected-roles="newViewRoles"
              display-text="Share with roles"
              @selected-roles-updated="updateNewViewRoles"
            />
          </td>
          <td>
            <input type="text"
              v-model="newViewUsers"
              class="form-control form-control-sm"
              placeholder="Comma separated list of users to share this new view with"
            />
          </td>
          <td colspan="2">
            <input type="text"
              v-model="newViewExpression"
              class="form-control form-control-sm"
              placeholder="Enter a new view expression"
            />
          </td>
          <td>&nbsp;</td>
          <td>
            <button
              type="button"
              @click="createView"
              title="Create new view"
              class="btn btn-theme-tertiary btn-sm pull-right">
              <span class="fa fa-plus-circle">
              </span>&nbsp;
              Create
            </button>
          </td>
        </tr> <!-- /new view form -->
        <!-- view form error -->
        <tr v-if="viewFormError">
          <td colspan="7">
            <p class="text-danger mb-0">
              <span class="fa fa-exclamation-triangle">
              </span>&nbsp;
              {{ viewFormError }}
            </p>
          </td>
        </tr> <!-- /view form error -->
      </tbody>
    </table>

  </form>
</template>

<script>
// services
import SettingsService from './SettingsService';
// components
import MolochPaging from '../utils/Pagination';
import RoleDropdown from '../../../../../common/vueapp/RoleDropdown';

export default {
  name: 'Views',
  components: {
    MolochPaging,
    RoleDropdown
  },
  data () {
    return {
      viewListError: '',
      viewFormError: '',
      newViewName: '',
      newViewExpression: '',
      newViewRoles: [],
      newViewUsers: '',
      size: 50,
      start: 0,
      viewsQuery: {
        search: '',
        desc: false,
        sortField: 'name'
      },
      recordsTotal: 0,
      recordsFiltered: 0
    };
  },
  props: {
    userId: String // the setting user id
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
    canEditView (view) {
      return this.user.roles.includes('arkimeAdmin') || (view.user && view.user === this.user.userId);
    },
    /* updates the roles on a view object from the RoleDropdown component */
    updateViewRoles (roles, id) {
      for (const view of this.views) {
        if (view.id === id) {
          this.$set(view, 'roles', roles);
          this.$set(view, 'changed', true);
          return;
        }
      }
    },
    updateNewViewRoles (roles) {
      this.newViewRoles = roles;
    },
    /* creates a view given the view name and expression */
    createView () {
      if (!this.newViewName) {
        this.viewFormError = 'No view name specified.';
        return;
      }

      if (!this.newViewExpression) {
        this.viewFormError = 'No view expression specified.';
        return;
      }

      const data = {
        name: this.newViewName,
        roles: this.newViewRoles,
        users: this.newViewUsers,
        expression: this.newViewExpression
      };

      SettingsService.createView(data, this.userId).then((response) => {
        // clear the inputs and any error
        this.viewFormError = false;
        this.newViewRoles = [];
        this.newViewUsers = '';
        this.newViewName = null;
        this.newViewExpression = null;
        // add the view to the view list
        this.views.push(response.view);
        // display success message to user
        let msg = response.text || 'Successfully created view.';
        if (response.invalidUsers && response.invalidUsers.length) {
          msg += ` Could not add these users: ${response.invalidUsers.join(',')}`;
        }
        this.$emit('display-message', { msg });
      }).catch((error) => {
        // display error message to user
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
    /**
     * Sets a view as having been changed
     * @param {number} index The index of the changed view
     */
    viewChanged (index) {
      this.$set(this.views[index], 'changed', true);
    },
    /**
     * Cancels a view change by retrieving the view
     * @param {string} index The index of the view
     */
    cancelViewChange (index) {
      SettingsService.getViews(this.userId).then((response) => {
        this.$set(this.views, index, response.find(view => view.id === this.views[index].id));
      }).catch((error) => {
        this.viewListError = error.text;
      });
    },
    /**
     * Updates a view
     * @param {string} index The index of the view to update
     */
    updateView (index) {
      if (!this.views[index].changed) {
        this.$emit('display-message', { msg: 'This view has not changed', type: 'dangwarninger' });
        return;
      }

      SettingsService.updateView(this.views[index], this.userId).then((response) => {
        // set the view as unchanged
        this.$set(this.views, index, response.view);
        // display success message to user
        let msg = response.text || 'Successfully created view.';
        if (response.invalidUsers && response.invalidUsers.length) {
          msg += ` Could not add these users: ${response.invalidUsers.join(',')}`;
        }
        this.$emit('display-message', { msg });
      }).catch((error) => {
        // display error message to user
        this.$emit('display-message', { msg: error.text, type: 'danger' });
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

      if (this.viewsQuery.search) { queryParams.searchTerm = this.viewsQuery.search; }
      if (this.userId) { queryParams.userId = this.userId; }

      SettingsService.getViews(queryParams).then((response) => {
        this.viewListError = '';
        this.recordsTotal = response.recordsTotal;
        this.recordsFiltered = response.recordsFiltered;
      }).catch((error) => {
        this.viewListError = error.text;
      });
    }
  }
};
</script>
