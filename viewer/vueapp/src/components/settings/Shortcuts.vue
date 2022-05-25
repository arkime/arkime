<template>
  <form class="form-horizontal mb-2">

    <h3>Shortcuts</h3>

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

    <div class="row">
      <div class="col-5">
        <div class="input-group input-group-sm">
          <div class="input-group-prepend">
            <div class="input-group-text">
              <span class="fa fa-search"></span>
            </div>
          </div>
          <b-form-input
            debounce="400"
            v-model="shortcutsQuery.search"
          />
        </div>
      </div>
      <div class="col-7">
        <moloch-paging v-if="shortcuts.data"
          class="pull-right"
          @changePaging="changeShortcutsPaging"
          :length-default="shortcutsSize"
          :records-total="shortcuts.recordsTotal"
          :records-filtered="shortcuts.recordsFiltered">
        </moloch-paging>
      </div>
    </div>

    <table v-if="shortcuts.data"
      class="table table-striped table-sm">
      <thead>
        <tr>
          <th class="cursor-help"
            v-b-tooltip.hover="'Whether this shortcut is shared with EVERY user'">
            Shared
          </th>
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
            <td>
              <input
                type="checkbox"
                v-model="item.shared"
                @input="toggleShortcutShared(item, index)"
                :disabled="(!user.roles.includes('arkimeAdmin') && item.userId !== user.userId) || item.locked"
                v-b-tooltip.hover="`click to ${item.shared ? 'unshare' : 'share'} this shortcut with EVERY user`"
              />
            </td>
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
                <button type="button"
                  v-b-tooltip.hover
                  title="Copy this shortcut's value"
                  class="btn btn-sm btn-theme-secondary"
                  @click="copyValue(item.value)">
                  <span class="fa fa-clipboard fa-fw" />
                </button>
                <span v-if="user.roles.includes('arkimeAdmin') || item.userId === user.userId">
                  <button type="button"
                    v-b-tooltip.hover
                    title="Delete this shortcut"
                    class="btn btn-sm btn-danger"
                    @click="deleteShortcut(item, index)">
                    <span class="fa fa-trash-o fa-fw" v-if="!item.loading" />
                    <span class="fa fa-spinner fa-spin fa-fw" v-else />
                  </button>
                  <span v-if="!item.editing">
                    <div v-if="item.locked"
                      v-b-tooltip.hover
                      style="display:inline-block"
                      title="Locked shortcut. Ask your admin to use db.pl to update this shortcut.">
                      <button :disabled="true"
                        type="button"
                        class="btn btn-sm btn-warning disabled cursor-help">
                        <span class="fa fa-lock fa-fw" />
                      </button>
                    </div>
                    <button type="button"
                      v-b-tooltip.hover
                      v-else
                      @click="toggleEditShortcut(item)"
                      title="Make changes to this shortcut's value"
                      class="btn btn-sm btn-theme-tertiary">
                      <span class="fa fa-pencil fa-fw" v-if="!item.loading" />
                      <span class="fa fa-spinner fa-spin fa-fw" v-else />
                    </button>
                  </span>
                  <span v-else>
                    <button type="button"
                      v-b-tooltip.hover
                      title="Cancel changes to this shortcut's value"
                      class="btn btn-sm btn-warning"
                      @click="toggleEditShortcut(item)">
                      <span class="fa fa-ban fa-fw" v-if="!item.loading" />
                      <span class="fa fa-spinner fa-spin fa-fw" v-else />
                    </button>
                    <button type="button"
                      v-b-tooltip.hover
                      @click="updateShortcut(item, index)"
                      title="Save changes to this shortcut's value"
                      class="btn btn-sm btn-theme-tertiary">
                      <span class="fa fa-save fa-fw" v-if="!item.loading" />
                      <span class="fa fa-spinner fa-spin fa-fw" v-else />
                    </button>
                  </span>
                </span>
              </span>
            </td>
          </tr>
          <!-- edit shortcut -->
          <tr :key="`${item.id}-edit`"
            v-if="item.editing">
            <td colspan="9">
              <div class="form-group row mt-2">
                <label for="updateShortcutName"
                  class="col-2 col-form-label text-right">
                  Name<sup>*</sup>
                </label>
                <div class="col-10">
                  <input id="updateShortcutName"
                    type="text"
                    class="form-control form-control-sm"
                    v-model="item.newName"
                  />
                </div>
              </div>
              <div class="form-group row">
                <label for="updateShortcutDescription"
                  class="col-2 col-form-label text-right">
                  Description
                </label>
                <div class="col-10">
                  <input id="updateShortcutDescription"
                    type="text"
                    class="form-control form-control-sm"
                    v-model="item.newDescription"
                  />
                </div>
              </div>
              <div class="form-group row">
                <label for="updateShortcutValue"
                  class="col-2 col-form-label text-right">
                  Value(s)<sup>*</sup>
                </label>
                <div class="col-10">
                  <textarea id="updateShortcutValue"
                    type="text"
                    rows="5"
                    class="form-control form-control-sm"
                    v-model="item.newValue">
                  </textarea>
                </div>
              </div>
              <div class="form-group row">
                <label for="updateShortcutType"
                  class="col-2 col-form-label text-right">
                  Type<sup>*</sup>
                </label>
                <div class="col-10">
                  <select id="updateShortcutType"
                    v-model="item.newType"
                    class="form-control form-control-sm">
                    <option value="ip">IP(s)</option>
                    <option value="string">String(s)</option>
                    <option value="number">Number(s)</option>
                  </select>
                </div>
              </div>
              <div class="form-group row">
                <label for="newShortcutShared"
                  class="col-2 col-form-label text-right">
                  Sharing
                </label>
                <div class="col-2">
                  <b-form-checkbox
                    class="mt-2"
                    v-model="item.shared"
                    id="updateShortcutShared"
                    v-b-tooltip.hover="'Share this shortcut with EVERY user'">
                    All Share
                  </b-form-checkbox>
                </div>
                <template v-if="!item.shared">
                  <div class="col-3">
                    <RoleDropdown
                      :id="item.id"
                      :roles="roles"
                      :selected-roles="item.newRoles"
                      @selected-roles-updated="updateShortcutRoles"
                      :display-text="item.newRoles && item.newRoles.length ? undefined : 'Share with roles'"
                    />
                  </div>
                  <div class="col-5">
                    <b-input-group
                      size="sm"
                      prepend="Share with users">
                      <b-form-input
                        v-model="item.newUsers"
                        placeholder="comma separated list of userIds"
                      />
                    </b-input-group>
                  </div>
                </template>
              </div>
            </td>
          </tr> <!-- /edit shortcut -->
        </template> <!-- /shortcuts -->
        <!-- no shortcuts -->
        <tr v-if="shortcuts.data && shortcuts.data.length === 0">
          <td colspan="9">
            <p class="text-center mb-0">
              <span class="fa fa-folder-open" />
              No shortcuts or none that match your search
            </p>
          </td>
        </tr> <!-- /no shortcuts -->
        <!-- shortcuts list error -->
        <tr v-if="shortcutsListError">
          <td colspan="9">
            <p class="text-danger mb-0">
              <span class="fa fa-exclamation-triangle" />
              {{ shortcutsListError }}
            </p>
          </td>
        </tr> <!-- /shortcuts list error -->
      </tbody>
    </table>
    <!-- new shortcut form -->
    <div class="row var-form mr-1 ml-1 mt-2">
      <div class="col">
        <div class="row mb-3 mt-4">
          <div class="col-10 offset-2">
            <h3 class="mt-3">
              New Shortcut
            </h3>
          </div>
        </div>
        <div class="form-group row">
          <label for="newShortcutName"
            class="col-2 col-form-label text-right">
            Name<sup>*</sup>
          </label>
          <div class="col-10">
            <input id="newShortcutName"
              type="text"
              class="form-control form-control-sm"
              v-model="newShortcutName"
              placeholder="MY_MOLOCH_VAR"
            />
          </div>
        </div>
        <div class="form-group row">
          <label for="newShortcutDescription"
            class="col-2 col-form-label text-right">
            Description
          </label>
          <div class="col-10">
            <input id="newShortcutDescription"
              type="text"
              class="form-control form-control-sm"
              v-model="newShortcutDescription"
            />
          </div>
        </div>
        <div class="form-group row">
          <label for="newShortcutValue"
            class="col-2 col-form-label text-right">
            Value(s)<sup>*</sup>
          </label>
          <div class="col-10">
            <textarea id="newShortcutValue"
              type="text"
              rows="5"
              class="form-control form-control-sm"
              v-model="newShortcutValue"
              placeholder="Enter a comma or newline separated list of values">
            </textarea>
          </div>
        </div>
        <div class="form-group row">
          <label for="newShortcutType"
            class="col-2 col-form-label text-right">
            Type<sup>*</sup>
          </label>
          <div class="col-10">
            <select id="newShortcutType"
              v-model="newShortcutType"
              class="form-control form-control-sm">
              <option value="ip">IP(s)</option>
              <option value="string">String(s)</option>
              <option value="number">Number(s)</option>
            </select>
          </div>
        </div>
        <div class="form-group row">
          <label for="newShortcutShared"
            class="col-2 col-form-label text-right">
            Sharing
          </label>
          <div class="col-2">
            <b-form-checkbox
              class="mt-2"
              id="newShortcutShared"
              v-model="newShortcutShared"
              v-b-tooltip.hover="'Share this shortcut with EVERY user'">
              All Share
            </b-form-checkbox>
          </div>
          <template v-if="!newShortcutShared">
            <div class="col-3">
              <RoleDropdown
                :roles="roles"
                display-text="Share with roles"
                @selected-roles-updated="updateNewShortcutRoles"
              />
            </div>
            <div class="col-5">
              <b-input-group
                size="sm"
                prepend="Share with users">
                <b-form-input
                  v-model="newShortcutUsers"
                  placeholder="comma separated list of userIds"
                />
              </b-input-group>
            </div>
          </template>
        </div>
        <div class="form-group row">
          <div class="col offset-2">
            <b-button
              block
              size="sm"
              @click="createShortcut"
              variant="theme-tertiary">
              <template v-if="!createShortcutLoading">
                <span class="fa fa-plus-circle mr-1" />
                Create
              </template>
              <template v-else>
                <span class="fa fa-spinner fa-spin mr-1" />
                Creating
              </template>
            </b-button>
          </div>
        </div>
      </div>
    </div> <!-- /new shortcut form -->

  </form> <!-- / shortcut settings -->
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
      newShortcutShared: false,
      newShortcutName: '',
      newShortcutDescription: '',
      newShortcutValue: '',
      newShortcutType: 'string',
      newShortcutUsers: '',
      newShortcutRoles: [],
      shortcutsStart: 0,
      shortcutsSize: 50,
      shortcutsQuery: {
        desc: false,
        sortField: 'name',
        search: ''
      },
      createShortcutLoading: false,
      hasUsersES: this.$constants.MOLOCH_HASUSERSES
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
    /* toggles shared var on a shortcut and saves the shortcut */
    toggleShortcutShared (shortcut, index) {
      this.$set(shortcut, 'shared', !shortcut.shared);
      this.updateShortcut(shortcut, index);
    },
    /* opens up text area to edit shortcut value */
    toggleEditShortcut (shortcut) {
      const editingShortcut = !shortcut.editing;
      this.$set(shortcut, 'editing', editingShortcut);
      this.$set(shortcut, 'newValue', shortcut.value);
      this.$set(shortcut, 'newName', shortcut.name);
      this.$set(shortcut, 'newType', shortcut.type);
      this.$set(shortcut, 'newRoles', shortcut.roles);
      this.$set(shortcut, 'newUsers', shortcut.users);
      this.$set(shortcut, 'newDescription', shortcut.description);
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
      if (!this.newShortcutName) {
        this.$emit('display-message', { msg: 'Enter a unique shortcut name', type: 'danger' });
        return;
      }

      if (!this.newShortcutValue) {
        this.$emit('display-message', { msg: 'Enter a value for your new shortcut', type: 'danger' });
        return;
      }

      this.createShortcutLoading = true;

      const data = {
        name: this.newShortcutName,
        type: this.newShortcutType,
        value: this.newShortcutValue,
        users: this.newShortcutUsers,
        roles: this.newShortcutRoles,
        shared: this.newShortcutShared,
        description: this.newShortcutDescription
      };

      SettingsService.createShortcut(data).then((response) => {
        this.getShortcuts();
        // clear the inputs
        this.newShortcutName = '';
        this.newShortcutValue = '';
        this.newShortcutUsers = '';
        this.newShortcutRoles = '';
        this.newShortcutShared = false;
        this.newShortcutDescription = '';
        // display success message to user
        let msg = response.text;
        if (response.invalidUsers) {
          msg += ` Could not add these users: ${response.invalidUsers.join(',')}`;
        }
        this.$emit('display-message', { msg });
        this.createShortcutLoading = false;
      }).catch((error) => {
        this.$emit('display-message', { msg: error.text, type: 'danger' });
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
    /* updates a specified shortcut (only shared and value are editable) */
    updateShortcut (shortcut, index) {
      this.$set(shortcut, 'loading', true);

      const data = {
        shared: shortcut.shared,
        userId: shortcut.userId,
        users: shortcut.newUsers,
        roles: shortcut.newRoles,
        name: shortcut.newName || shortcut.name,
        type: shortcut.newType || shortcut.type,
        value: shortcut.newValue || shortcut.value,
        description: shortcut.newDescription || shortcut.description
      };

      SettingsService.updateShortcut(shortcut.id, data).then((response) => {
        response.shortcut.id = shortcut.id;
        response.shortcut.type = shortcut.newType || shortcut.type;
        this.$set(this.shortcuts.data, index, response.shortcut);
        // display success message to user
        // display success message to user
        let msg = response.text;
        if (response.invalidUsers) {
          msg += ` Could not add these users: ${response.invalidUsers.join(',')}`;
        }
        this.$emit('display-message', { msg });
        this.$set(shortcut, 'loading', false);
      }).catch((error) => {
        this.$emit('display-message', { msg: error.text, type: 'danger' });
        this.$set(shortcut, 'loading', false);
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
        // display success message to user
        this.$emit('display-message', { msg: response.text });
        this.$set(shortcut, 'loading', false);
      }).catch((error) => {
        // display error message to user
        this.$emit('display-message', { msg: error.text, type: 'danger' });
        this.$set(shortcut, 'loading', false);
      });
    },

    /* helpers ------------------------------------------------------------- */
    getShortcuts: function () {
      const queryParams = {
        length: this.shortcutsSize,
        start: this.shortcutsStart,
        desc: this.shortcutsQuery.desc,
        sort: this.shortcutsQuery.sortField
      };

      if (this.shortcutsQuery.search) { queryParams.searchTerm = this.shortcutsQuery.search; }
      if (this.userId) { queryParams.userId = this.userId; }

      SettingsService.getShortcuts(queryParams).then((response) => {
        this.loading = false;
        this.shortcuts = response;
        this.shortcutsListError = '';
      }).catch((error) => {
        this.loading = false;
        this.shortcutsListError = error.text || error;
      });
    }
  }
};
</script>

<style>
/* shortcuts form */
.settings-page .var-form {
  box-shadow: inset 0 1px 1px rgba(0, 0, 0, .05);
  background-color: var(--color-gray-lighter);
  border: 1px solid var(--color-gray-light);
  border-radius: 3px;
}
.settings-page .var-form input[type='checkbox'] {
  margin-top: 0.75rem;
}

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
