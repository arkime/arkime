<template>
  <div>

    <h3>
      Shortcuts
      <b-button
        size="sm"
        variant="success"
        class="pull-right"
        v-b-modal.shortcut-modal>
        <span class="fa fa-plus-circle mr-1" />
        New Shortcut
      </b-button>
    </h3>

    <p>
      Create a list of values that can be used in queries as shortcuts.
      For example, create a list of IPs and use them in a query
      expression <code>ip.src == $MY_IPS</code>.
    </p>
    <p>
      <strong>Tip:</strong>
      Use <code>$</code> to autocomplete shortcuts in search expressions.
    </p>
    <p>
      <strong>Note:</strong>
      <template v-if="hasUsersES">
        These shortcuts will be synced across clusters.
        It can take up to one minute to sync to all clusters.
        But you can use the shortcut on this cluster immediately.
      </template>
      <template v-else>
        These shortcuts are local to this cluster only.
      </template>
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
            v-model="shortcutsQuery.search"
          />
        </b-input-group>
      </div>
      <b-form-checkbox
        button
        size="sm"
        class="mr-2"
        v-model="seeAll"
        @input="getShortcuts"
        v-b-tooltip.hover
        v-if="user.roles.includes('arkimeAdmin')"
        :title="seeAll ? 'Just show the shortcuts created by you and shared with you' : 'See all the shortcuts that exist for all users (you can because you are an ADMIN!)'">
        <span class="fa fa-user-circle mr-1" />
        See {{ seeAll ? ' MY ' : ' ALL ' }} Shortcuts
      </b-form-checkbox>
      <moloch-paging
        v-if="shortcuts.data"
        :length-default="shortcutsSize"
        @changePaging="changeShortcutsPaging"
        :records-total="shortcuts.recordsTotal"
        :records-filtered="shortcuts.recordsFiltered">
      </moloch-paging>
    </div>

    <table v-if="shortcuts.data"
      class="table table-striped table-sm">
      <thead>
        <tr>
          <th class="cursor-pointer"
            @click.self="sortShortcuts('name')">
            Name
            <span v-show="shortcutsQuery.sortField === 'name' && !shortcutsQuery.desc" class="fa fa-sort-asc"></span>
            <span v-show="shortcutsQuery.sortField === 'name' && shortcutsQuery.desc" class="fa fa-sort-desc"></span>
            <span v-show="shortcutsQuery.sortField !== 'name'" class="fa fa-sort"></span>
          </th>
          <th class="cursor-pointer"
            @click.self="sortShortcuts('description')">
            Description
            <span v-show="shortcutsQuery.sortField === 'description' && !shortcutsQuery.desc" class="fa fa-sort-asc"></span>
            <span v-show="shortcutsQuery.sortField === 'description' && shortcutsQuery.desc" class="fa fa-sort-desc"></span>
            <span v-show="shortcutsQuery.sortField !== 'description'" class="fa fa-sort"></span>
          </th>
          <th>Value(s)</th>
          <th>Type</th>
          <th>Creator</th>
          <th>Roles</th>
          <th>Users</th>
          <th>&nbsp;</th>
        </tr>
      </thead>
      <tbody>
        <!-- shortcuts -->
        <tr v-if="loading">
          <td colspan="9">
            <p class="text-center mb-0">
              <span class="fa fa-spinner fa-spin" />
              Loading shortcuts...
            </p>
          </td>
        </tr>
        <template v-for="(item, index) in shortcuts.data">
          <tr :key="`${item.id}-content`">
            <td class="shortcut-value narrow cursor-help"
              v-b-tooltip.hover="item.name">
              {{ item.name }}
            </td>
            <td class="shortcut-value cursor-help"
              v-b-tooltip.hover="item.description">
              {{ item.description }}
            </td>
            <td class="shortcut-value"
              :class="{'show-all':item.showAll}">
              <span
                v-if="item.value.length > 50"
                @click="toggleDisplayAllShortcut(item)"
                class="fa pull-right cursor-pointer mt-1"
                :class="{'fa-chevron-down':!item.showAll,'fa-chevron-up':item.showAll}"
              />
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
            <td>
              {{ item.roles ? item.roles.join(',') : '' }}
            </td>
            <td>
              {{ item.users }}
            </td>
            <td class="shortcut-btns">
              <span class="pull-right">
                <b-button
                  size="sm"
                  v-b-tooltip.hover
                  variant="theme-secondary"
                  title="Copy this shortcut's value"
                  @click="$emit('copy-value', item.value)">
                  <span class="fa fa-clipboard fa-fw" />
                </b-button>
                <span v-if="user.roles.includes('arkimeAdmin') || item.userId === user.userId">
                  <b-button
                    size="sm"
                    variant="danger"
                    v-b-tooltip.hover
                    title="Delete this shortcut"
                    @click="deleteShortcut(item, index)">
                    <span class="fa fa-trash-o fa-fw" v-if="!item.loading" />
                    <span class="fa fa-spinner fa-spin fa-fw" v-else />
                  </b-button>
                  <span>
                    <div
                      v-if="item.locked"
                      v-b-tooltip.hover
                      style="display:inline-block"
                      title="Locked shortcut. Ask your admin to use db.pl to update this shortcut.">
                      <b-button
                        size="sm"
                        :disabled="true"
                        variant="warning"
                        class="disabled cursor-help">
                        <span class="fa fa-lock fa-fw" />
                      </b-button>
                    </div>
                    <b-button
                      v-else
                      size="sm"
                      v-b-tooltip.hover
                      variant="theme-tertiary"
                      @click="editShortcut(item)"
                      title="Update this shortcut">
                      <span class="fa fa-pencil fa-fw" v-if="!item.loading" />
                      <span class="fa fa-spinner fa-spin fa-fw" v-else />
                    </b-button>
                  </span>
                </span>
              </span>
            </td>
          </tr>
        </template> <!-- /shortcuts -->
      </tbody>
    </table>

    <!-- shortcuts list error -->
    <b-alert
      variant="danger"
      class="mt-2 mb-0"
      :show="!!shortcutsListError">
      <span class="fa fa-exclamation-triangle mr-1" />
      {{ shortcutsListError }}
    </b-alert> <!-- /shortcuts list error -->

    <!-- no results -->
    <div class="text-center mt-4"
      v-if="shortcuts.data && shortcuts.data.length === 0">
      <h3>
        <span class="fa fa-folder-open fa-2x" />
      </h3>
      <h5>
        No shortcuts or none that match your search.
        <br>
        Click the create button above to create one!
      </h5>
    </div> <!-- /no results -->

    <!-- new shortcut form -->
    <b-modal
      size="xl"
      id="shortcut-modal"
      :title="editingShortcut ? 'Edit Shortcut' : 'Create New Shortcut'">
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
          v-model="newShortcutName"
          placeholder="MY_ARKIME_VAR"
        />
      </b-input-group>
      <b-input-group
        size="sm"
        class="mb-2">
        <template #prepend>
          <b-input-group-text
            v-b-tooltip.hover
            class="cursor-help"
            title="Enter an optional description to explain the shortcut">
            Description
          </b-input-group-text>
        </template>
        <b-form-input
          v-model="newShortcutDescription"
          placeholder="Shortcut description"
        />
      </b-input-group>
      <b-input-group
        size="sm"
        class="mb-2">
        <template #prepend>
          <b-input-group-text
            v-b-tooltip.hover
            class="cursor-help"
            title="Enter an optional description to explain the reason for this query">
            Value(s)<sup>*</sup>
          </b-input-group-text>
        </template>
        <b-form-textarea
          rows="5"
          v-model="newShortcutValue"
          placeholder="Enter a comma or newline separated list of values"
        />
      </b-input-group>
      <b-input-group
        size="sm"
        class="mb-2">
        <template #prepend>
          <b-input-group-text
            v-b-tooltip.hover
            class="cursor-help"
            title="The type of shortcut this is">
            Type<sup>*</sup>
          </b-input-group-text>
        </template>
        <select
          v-model="newShortcutType"
          class="form-control form-control-sm">
          <option value="ip">IP(s)</option>
          <option value="string">String(s)</option>
          <option value="number">Number(s)</option>
        </select>
      </b-input-group>
      <div class="d-flex">
        <div class="mr-3">
          <RoleDropdown
            :roles="roles"
            display-text="Share with roles"
            @selected-roles-updated="updateNewShortcutRoles"
          />
        </div>
        <b-input-group
          size="sm"
          class="flex-grow-1"
          prepend="Share with users">
          <b-form-input
            v-model="newShortcutUsers"
            placeholder="comma separated list of userIds"
          />
        </b-input-group>
      </div>
      <!-- create form error -->
      <b-alert
        variant="danger"
        class="mt-2 mb-0"
        :show="!!shortcutFormError">
        <span class="fa fa-exclamation-triangle mr-1" />
        {{ shortcutFormError }}
      </b-alert> <!-- /create form error -->
      <template #modal-footer>
        <div class="w-100 d-flex justify-content-between">
          <b-button
            title="Cancel"
            variant="danger"
            @click="$bvModal.hide('shortcut-modal')">
            <span class="fa fa-times" />
            Cancel
          </b-button>
          <b-button
            variant="success"
            v-b-tooltip.hover
            v-if="!editingShortcut"
            @click="createShortcut"
            title="Create new shortcut"
            :disabled="createShortcutLoading"
            :class="{'disabled':createShortcutLoading}">
            <template v-if="!createShortcutLoading">
              <span class="fa fa-plus-circle mr-1" />
              Create
            </template>
            <template v-else>
              <span class="fa fa-spinner fa-spin mr-1" />
              Creating
            </template>
          </b-button>
          <b-button
            v-else
            variant="success"
            v-b-tooltip.hover
            @click="updateShortcut"
            title="Update shortcut"
            :disabled="createShortcutLoading"
            :class="{'disabled':createShortcutLoading}">
            <template v-if="!createShortcutLoading">
              <span class="fa fa-save mr-1" />
              Save
            </template>
            <template v-else>
              <span class="fa fa-spinner fa-spin mr-1" />
              Saving
            </template>
          </b-button>
        </div>
      </template> <!-- /modal footer -->
    </b-modal> <!-- /new shortcut form -->

  </div> <!-- / shortcut settings -->
</template>

<script>
// services
import SettingsService from './SettingsService';
// components
import MolochPaging from '../utils/Pagination';
import RoleDropdown from '../../../../../common/vueapp/RoleDropdown';

export default {
  name: 'Shortcuts',
  components: {
    MolochPaging,
    RoleDropdown
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
      editingShortcut: false,
      shortcutsStart: 0,
      shortcutsSize: 50,
      shortcutsQuery: {
        desc: false,
        sortField: 'name',
        search: ''
      },
      createShortcutLoading: false,
      hasUsersES: this.$constants.MOLOCH_HASUSERSES,
      showAll: false,
      seeAll: false
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
      this.newShortcutType = shortcut.type || 'string';
      this.newShortcutDescription = shortcut.description || '';
      this.$bvModal.show('shortcut-modal');
    },
    /* show/hide the entire shortcut value */
    toggleDisplayAllShortcut (shortcut) {
      this.$set(shortcut, 'showAll', !shortcut.showAll);
    },
    updateNewShortcutRoles (roles) {
      this.newShortcutRoles = roles;
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
        description: this.newShortcutDescription
      };

      SettingsService.createShortcut(data).then((response) => {
        this.getShortcuts();
        this.clearShortcutForm();
        this.$bvModal.hide('shortcut-modal');
        this.displaySuccess(response);
      }).catch((error) => {
        this.shortcutFormError = error.text;
        this.createShortcutLoading = false;
      });
    },
    updateShortcutRoles (roles, id) {
      for (const shortcut of this.shortcuts.data) {
        if (shortcut.id === id) {
          this.$set(shortcut, 'newRoles', roles);
          return;
        }
      }
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
        description: this.newShortcutDescription
      };

      SettingsService.updateShortcut(this.editingShortcut, data).then((response) => {
        response.shortcut.id = this.editingShortcut; // server doesn't return id
        response.shortcut.type = this.newShortcutType; // server doesn't return type
        // update the shortcut in the table
        for (let i = 0; i < this.shortcuts.data.length; i++) {
          if (this.shortcuts.data[i].id === this.editingShortcut) {
            this.$set(this.shortcuts.data, i, response.shortcut);
          }
        }
        this.clearShortcutForm();
        this.editingShortcut = undefined;
        this.$bvModal.hide('shortcut-modal');
        this.displaySuccess(response);
      }).catch((error) => {
        this.shortcutFormError = error.text;
        this.createShortcutLoading = false;
      });
    },
    /* deletes a shortcut and removes it from the shortcuts array */
    deleteShortcut (shortcut, index) {
      this.$set(shortcut, 'loading', true);

      SettingsService.deleteShortcut(shortcut.id).then((response) => {
        // remove it from the array
        this.shortcuts.data.splice(index, 1);
        this.shortcuts.recordsTotal--;
        this.shortcuts.recordsFiltered--;
        this.$set(shortcut, 'loading', false);
        this.displaySuccess(response);
      }).catch((error) => {
        // display error message to user
        this.$emit('display-message', { msg: error.text, type: 'danger' });
        this.$set(shortcut, 'loading', false);
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
        this.shortcutsListError = error.text || error;
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
      this.newShortcutDescription = '';
      this.createShortcutLoading = false;
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
