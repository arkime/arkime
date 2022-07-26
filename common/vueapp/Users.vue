<template>
  <div>

    <!-- search -->
    <div class="d-flex justify-content-between mt-3">
      <div class="mr-2 flex-grow-1 ">
        <b-input-group size="sm">
          <template #prepend>
            <b-input-group-text>
              <span class="fa fa-search fa-fw" />
            </b-input-group-text>
          </template>
          <b-form-input
            autofocus
            type="text"
            debounce="400"
            v-model="searchTerm"
            placeholder="Begin typing to search for users by name, id, or roles"
          />
          <template #append>
            <b-button
              :disabled="!searchTerm"
              @click="searchTerm = ''"
              variant="outline-secondary"
              v-b-tooltip.hover="'Clear search'">
              <span class="fa fa-close" />
            </b-button>
          </template>
        </b-input-group>
      </div>
      <div class="mr-2">
        <b-form-select
          size="sm"
          v-model="perPage"
          @change="perPageChange"
          :options="[
            { value: 10, text: '10 per page'},
            { value: 20, text: '20 per page'},
            { value: 50, text: '50 per page'},
            { value: 100, text: '100 per page'},
            { value: 200, text: '200 per page'}
          ]"
        />
      </div>
      <div>
        <b-pagination
          size="sm"
          :per-page="perPage"
          v-model="currentPage"
          :total-rows="recordsTotal"
        />
      </div>
    </div> <!-- /search -->

    <!-- error -->
    <div v-if="error"
      class="info-area vertical-center text-monospace">
      <div class="text-danger">
        <span class="fa fa-2x fa-warning" />
        {{ error }}
      </div>
    </div> <!-- /error -->

    <!-- loading -->
    <template v-if="loading">
      <slot name="loading">
        <div class="text-center mt-5">
          <span class="fa fa-2x fa-spin fa-spinner" />
          <br />
          Loading...
        </div>
      </slot>
    </template> <!-- /loading -->

    <!-- users table -->
    <div v-if="!error">
      <b-table
        small
        hover
        striped
        foot-clone
        show-empty
        sort-icon-left
        no-local-sorting
        :items="users"
        :fields="fields"
        :sort-desc.sync="desc"
        class="small-table-font"
        :sort-by.sync="sortField"
        @sort-changed="sortChanged"
        :empty-text="searchTerm ? 'No users or roles match your search' : 'No users or roles'">

        <!-- column headers -->
        <template v-slot:head()="data">
          <span v-b-tooltip.hover="data.field.help">
            {{ data.label }}
            <span
              v-if="data.field.key === 'roles'"
              class="fa fa-info-circle fa-lg cursor-help ml-2"
              v-b-tooltip.hover="'These roles are applied across apps (Arkime, Parliament, WISE, Cont3xt)'"
            />
            <div class="pull-right"
              v-if="data.field.key === 'action'">
              <b-button
                size="sm"
                v-if="roles"
                variant="success"
                title="Create a new role"
                v-b-modal.create-user-modal
                @click="createMode = 'role'">
                <span class="fa fa-plus-circle mr-1" />
                Role
              </b-button>
              <b-button
                size="sm"
                variant="primary"
                title="Create a new user"
                v-b-modal.create-user-modal
                @click="createMode = 'user'">
                <span class="fa fa-plus-circle mr-1" />
                User
              </b-button>
            </div>
          </span>
        </template> <!-- /column headers -->

        <!-- toggle column -->
         <template #cell(toggle)="data">
          <span :class="{'btn-indicator':!data.item.emailSearch || !data.item.removeEnabled || !data.item.packetSearch || data.item.hideStats || data.item.hideFiles || data.item.hidePcap || data.item.disablePcapDownload || data.item.timeLimit || data.item.expression}">
            <ToggleBtn
              class="btn-toggle-user"
              @toggle="data.toggleDetails"
              :opened="data.detailsShowing"
              :class="{expanded: data.detailsShowing}"
              v-b-tooltip.hover.noninteractive="!data.item.emailSearch || !data.item.removeEnabled || !data.item.packetSearch || data.item.hideStats || data.item.hideFiles || data.item.hidePcap || data.item.disablePcapDownload || data.item.timeLimit || data.item.expression ? 'This user has additional restricted permissions' : ''"
            />
          </span>
        </template> <!-- /toggle column -->
        <!-- action column -->
        <template #cell(action)="data">
          <div class="pull-right">
            <b-button v-if="changed[data.item.userId]"
              size="sm"
              variant="success"
              v-b-tooltip.hover
              @click="updateUser(data)"
              :title="`Save the updated settings for ${data.item.userId}`">
              <span class="fa fa-save" />
            </b-button>
            <b-button v-if="changed[data.item.userId]"
              size="sm"
              variant="warning"
              @click="cancelEdits(data.item.userId)"
              v-b-tooltip.hover="`Cancel changed settings for ${data.item.userId}`">
              <span class="fa fa-ban" />
            </b-button>
            <b-button
              size="sm"
              variant="primary"
              @click="openSettings(data.item.userId)"
              v-has-role="{user:currentUser,roles:'arkimeAdmin'}"
              v-if="parentApp === 'Arkime' && !data.item.userId.startsWith('role:')"
              v-b-tooltip.hover="`Arkime settings for ${data.item.userId}`">
              <span class="fa fa-gear" />
            </b-button>
            <b-button
              size="sm"
              variant="secondary"
              v-if="parentApp === 'Arkime'"
              @click="openHistory(data.item.userId)"
              v-b-tooltip.hover="`History for ${data.item.userId}`">
              <span class="fa fa-history" />
            </b-button>
            <b-button
              size="sm"
              variant="danger"
              v-b-tooltip.hover
              :title="`Delete ${data.item.userId}`"
              @click="deleteUser(data.item, data.index)">
              <span class="fa fa-trash-o" />
            </b-button>
          </div>
        </template> <!-- /action column -->
        <!-- user id column -->
        <template #cell(userId)="data">
          {{ data.value }}
        </template> <!-- /user id column -->
        <!-- last used column -->
        <template #cell(lastUsed)="data">
          {{ data.value ? (tzDateStr(data.value, currentUser.settings.timezone || 'local', currentUser.settings.ms)) : 'Never' }}
        </template> <!-- /last used column -->
        <!-- all other columns -->
        <template #cell()="data">
          <b-form-input
            size="sm"
            v-model="data.item[data.field.key]"
            v-if="data.field.type === 'text'"
            @input="userHasChanged(data.item.userId)"
          />
          <b-form-checkbox
            data-testid="checkbox"
            v-model="data.item[data.field.key]"
            v-else-if="data.field.type === 'checkbox'"
            @input="userHasChanged(data.item.userId)"
          />
          <template v-else-if="data.field.type === 'select' && roles && roles.length">
            <RoleDropdown
              :roles="roles"
              :id="data.item.userId"
              :selected-roles="data.item.roles"
              @selected-roles-updated="updateRoles"
            />
          </template>
        </template> <!-- all other columns -->

        <!-- detail row -->
        <template #row-details="data">
          <div class="m-2">
            <b-form-checkbox inline
              data-testid="checkbox"
              :checked="!data.item.emailSearch"
              @input="newVal => negativeToggle(newVal, data.item, 'emailSearch', true)">
              Disable Arkime Email Search
            </b-form-checkbox>
            <b-form-checkbox inline
              data-testid="checkbox"
              :checked="!data.item.removeEnabled"
              @input="newVal => negativeToggle(newVal, data.item, 'removeEnabled', true)">
              Disable Arkime Data Removal
            </b-form-checkbox>
            <b-form-checkbox inline
              data-testid="checkbox"
              :checked="!data.item.packetSearch"
              @input="newVal => negativeToggle(newVal, data.item, 'packetSearch', true)">
              Disable Arkime Hunting
            </b-form-checkbox>
            <b-form-checkbox inline
              data-testid="checkbox"
              v-model="data.item.hideStats"
              @input="userHasChanged(data.item.userId)">
              Hide Arkime Stats Page
            </b-form-checkbox>
            <b-form-checkbox inline
              data-testid="checkbox"
              v-model="data.item.hideFiles"
              @input="userHasChanged(data.item.userId)">
              Hide Arkime Files Page
            </b-form-checkbox>
            <b-form-checkbox inline
              data-testid="checkbox"
              v-model="data.item.hidePcap"
              @input="userHasChanged(data.item.userId)">
              Hide Arkime PCAP
            </b-form-checkbox>
            <b-form-checkbox inline
              data-testid="checkbox"
              v-model="data.item.disablePcapDownload"
              @input="userHasChanged(data.item.userId)">
              Disable Arkime PCAP Download
            </b-form-checkbox>
            <b-input-group
              size="sm"
              class="mt-2">
              <template #prepend>
                <b-input-group-text
                  v-b-tooltip.hover="'An Arkime search expression that is silently added to all queries. Useful to limit what data can be accessed (e.g. which nodes or IPs)'">
                  Forced Expression
                </b-input-group-text>
              </template>
              <b-form-input
                v-model="data.item.expression"
                @input="userHasChanged(data.item.userId)"
              />
            </b-input-group>
            <b-input-group
              size="sm"
              class="mt-2 w-25">
              <template #prepend>
                <b-input-group-text
                  v-b-tooltip.hover="'Restrict the maximum time window of a query'">
                  Query Time Limit
                </b-input-group-text>
              </template>
              <!-- NOTE: can't use b-form-select because it doesn't allow for undefined v-models -->
              <select
                class="form-control"
                v-model="data.item.timeLimit"
                @change="changeTimeLimit(data.item)">
                <option value="1">1 hour</option>
                <option value="6">6 hours</option>
                <option value="24">24 hours</option>
                <option value="48">48 hours</option>
                <option value="72">72 hours</option>
                <option value="168">1 week</option>
                <option value="336">2 weeks</option>
                <option value="720">1 month</option>
                <option value="1440">2 months</option>
                <option value="4380">6 months</option>
                <option value="8760">1 year</option>
                <option value=undefined>All (careful)</option>
              </select>
            </b-input-group>

            <!-- display change password if not a role and
                 we're in cont3xt or arkime
                 (assumes user is a usersAdmin since only usersAdmin can see this page) -->
            <template v-if="parentApp === 'Cont3xt' || parentApp === 'Arkime'">
              <form class="row" v-if="!data.item.userId.startsWith('role:')">
                <div class="col-9 mt-4">
                  <!-- new password -->
                  <b-input-group
                      size="sm"
                      class="mt-2"
                      prepend="New Password">
                    <b-form-input
                        type="password"
                        v-model="newPassword"
                        autocomplete="new-password"
                        @keydown.enter="changePassword"
                        placeholder="Enter a new password"
                    />
                  </b-input-group>
                  <!-- confirm new password -->
                  <b-input-group
                      size="sm"
                      class="mt-2"
                      prepend="Confirm Password">
                    <b-form-input
                        type="password"
                        autocomplete="new-password"
                        v-model="confirmNewPassword"
                        @keydown.enter="changePassword"
                        placeholder="Confirm the new password"
                    />
                  </b-input-group>
                  <!-- change password button -->
                  <b-button
                      size="sm"
                      class="mt-2"
                      variant="success"
                      @click="changePassword(data.item.userId)">
                    Change Password
                  </b-button>
                </div>
              </form>
              <span v-else>
                <UserDropdown class="mt-2" label="Role Assigners: "
                              :selected-users="data.item.roleAssigners || []"
                              :role-id="data.item.userId"
                              @selected-users-updated="updateRoleAssigners" />
              </span>
            </template>
          </div>
        </template> <!-- /detail row -->
      </b-table>
    </div> <!-- /users table -->

    <!-- create user -->
    <UserCreate
      :roles="roles"
      :create-mode="createMode"
      @user-created="userCreated"
    />

    <!-- messages -->
    <b-alert
      :show="!!msg"
      class="position-fixed fixed-bottom m-0 rounded-0"
      style="z-index: 2000;"
      :variant="msgType"
      dismissible>
      {{ msg }}
    </b-alert> <!-- messages -->
  </div>
</template>

<script>
import HasRole from './HasRole';
import ToggleBtn from './ToggleBtn';
import UserCreate from './UserCreate';
import UserService from './UserService';
import RoleDropdown from './RoleDropdown';
import UserDropdown from './UserDropdown';
import { timezoneDateString } from './vueFilters';

export default {
  name: 'UsersCommon',
  directives: { HasRole },
  components: {
    ToggleBtn,
    UserCreate,
    RoleDropdown,
    UserDropdown
  },
  props: {
    roles: Array,
    parentApp: String,
    currentUser: Object
  },
  data () {
    return {
      error: '',
      msg: '',
      msgType: '',
      loading: true,
      searchTerm: '',
      users: undefined,
      dbUserList: undefined,
      changed: {},
      recordsTotal: 0,
      perPage: 100,
      currentPage: 1,
      sortField: 'userId',
      desc: false,
      createMode: 'user',
      fields: [
        { label: '', key: 'toggle', sortable: false },
        { label: 'ID', key: 'userId', sortable: true, required: true, help: 'The ID used for login (cannot be changed once created)', thStyle: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
        { label: 'Name', key: 'userName', sortable: true, type: 'text', required: true, help: 'Friendly/readable name', thStyle: 'width:250px;white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
        { name: 'Enabled', key: 'enabled', sortable: true, type: 'checkbox', help: 'Is the account currently enabled for anything?', thStyle: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
        { name: 'Web Interface', key: 'webEnabled', sortable: true, type: 'checkbox', help: 'Can access the web interface. When off only APIs can be used', thStyle: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
        { name: 'Web Auth Header', key: 'headerAuthEnabled', sortable: true, type: 'checkbox', help: 'Can login using the web auth header. This setting doesn\'t disable the password so it should be scrambled', thStyle: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
        { name: 'Roles', key: 'roles', sortable: false, type: 'select', help: 'Roles assigned', thStyle: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
        { name: 'Last Used', key: 'lastUsed', sortable: true, type: 'checkbox', help: 'The last time Arkime was used by this account', thStyle: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
        { label: '', key: 'action', sortable: false, thStyle: 'width:190px;white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' }
      ],
      // password
      newPassword: '',
      confirmNewPassword: ''
    };
  },
  created () {
    this.loadUsers();
  },
  watch: {
    searchTerm () {
      this.loadUsers();
    },
    currentPage (newPage) {
      this.loadUsers();
    }
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    getRolesStr (userRoles) {
      let userDefinedRoles = [];
      let roles = [];
      for (let role of userRoles) {
        if (role.startsWith('role:')) {
          role = role.slice(5);
          userDefinedRoles.push(role);
          continue;
        }
        roles.push(role);
      }

      userDefinedRoles = userDefinedRoles.sort();
      roles = roles.sort();

      const allRoles = userDefinedRoles.concat(roles);
      return allRoles.join(', ');
    },
    tzDateStr (date, tz, ms) {
      return timezoneDateString(date, tz, ms);
    },
    perPageChange (newVal) {
      this.perPage = newVal;
      this.loadUsers(false);
    },
    sortChanged (ctx) {
      this.sortField = ctx.sortBy;
      this.desc = ctx.sortDesc;
      this.loadUsers();
    },
    negativeToggle (newVal, user, field, existing) {
      this.$set(user, field, !newVal);
      if (existing) { this.userHasChanged(user.userId); }
    },
    changeTimeLimit (user) {
      if (user.timeLimit === 'undefined') {
        delete user.timeLimit;
      } else {
        user.timeLimit = parseInt(user.timeLimit);
      }

      this.userHasChanged(user.userId);
    },
    updateRoles (roles, userId) {
      const user = this.users.find(u => u.userId === userId);
      this.$set(user, 'roles', roles);
      this.userHasChanged(userId);
    },
    updateRoleAssigners ({ newSelection }, roleId) {
      const role = this.users.find(u => u.userId === roleId);
      console.log(role, newSelection, roleId);
      this.$set(role, 'roleAssigners', newSelection);
      this.userHasChanged(roleId);
    },
    userHasChanged (userId) {
      const newUser = JSON.parse(JSON.stringify(this.users.find(u => u.userId === userId)));
      const oldUser = JSON.parse(JSON.stringify(this.dbUserList.find(u => u.userId === userId)));

      // remove _showDetails for user (added by b-table when user row is expanded)
      delete newUser._showDetails;
      delete oldUser._showDetails;

      // roles might be undefined, but compare to emtpy array since toggling on
      // any roles sets roles to an array, and removing that role = empty array
      if (oldUser.roles === undefined) { oldUser.roles = []; }
      if (newUser.roles === undefined) { newUser.roles = []; }

      // make sure these fields exist or the objects will be different
      // (undefined is the same as false for these fields)
      oldUser.timeLimit = oldUser.timeLimit ? oldUser.timeLimit : undefined;
      newUser.timeLimit = newUser.timeLimit ? newUser.timeLimit : undefined;
      oldUser.hidePcap = oldUser.hidePcap ? oldUser.hidePcap : undefined;
      newUser.hidePcap = newUser.hidePcap ? newUser.hidePcap : undefined;
      oldUser.hideFiles = oldUser.hideFiles ? oldUser.hideFiles : undefined;
      newUser.hideFiles = newUser.hideFiles ? newUser.hideFiles : undefined;
      oldUser.hideStats = oldUser.hideStats ? oldUser.hideStats : undefined;
      newUser.hideStats = newUser.hideStats ? newUser.hideStats : undefined;
      oldUser.disablePcapDownload = oldUser.disablePcapDownload ? oldUser.disablePcapDownload : undefined;
      newUser.disablePcapDownload = newUser.disablePcapDownload ? newUser.disablePcapDownload : undefined;

      oldUser.expanded = undefined; // don't care about expanded field (just for UI)
      newUser.expanded = undefined;
      oldUser.lastUsed = undefined; // don't compare lastused, it might be different if the user is using the UI
      newUser.lastUsed = undefined;

      const hasChanged = JSON.stringify(newUser) !== JSON.stringify(oldUser);
      this.$set(this.changed, userId, hasChanged);
      return hasChanged;
    },
    updateUser (row) {
      if (row.detailsShowing) { row.toggleDetails(); }

      const user = row.item;
      UserService.updateUser(user).then((response) => {
        this.$set(this.changed, user.userId, false);
        this.showMessage({ variant: 'success', message: response.text });
        this.reloadUsers();

        // update the current user if they were changed
        if (this.currentUser.userId === user.userId) {
          this.$emit('update-current-user');
        }
      }).catch((error) => {
        this.showMessage({ variant: 'danger', message: error.text });
      });
    },
    deleteUser (user, index) {
      UserService.deleteUser(user).then((response) => {
        this.users.splice(index, 1);
        this.showMessage({ variant: 'success', message: response.text });
      }).catch((error) => {
        this.showMessage({ variant: 'danger', message: error.text });
      });
    },
    cancelEdits (userId) {
      this.$set(this.changed, userId, false);
      const canceledUser = this.users.find(u => u.userId === userId);
      const oldUser = this.dbUserList.find(u => u.userId === userId);
      Object.assign(canceledUser, oldUser);
    },
    openSettings (userId) {
      this.$router.push({
        path: '/settings',
        query: {
          ...this.$route.query,
          userId
        }
      });
    },
    openHistory (userId) {
      this.$router.push({
        path: '/history',
        query: {
          ...this.$route.query,
          userId
        }
      });
    },
    changePassword (userId) {
      this.msg = '';

      if (!this.newPassword) {
        this.showMessage({
          variant: 'danger',
          message: 'You must enter a new password'
        });
        return;
      }

      if (!this.confirmNewPassword) {
        this.showMessage({
          variant: 'danger',
          message: 'You must confirm your new password'
        });
        return;
      }

      if (this.newPassword !== this.confirmNewPassword) {
        this.showMessage({
          variant: 'danger',
          message: "Your passwords don't match"
        });
        return;
      }

      const data = {
        newPassword: this.newPassword
      };

      UserService.changePassword(data, userId).then((response) => {
        this.newPassword = null;
        this.confirmNewPassword = null;
        // display success message to user
        this.showMessage({ variant: 'success', message: response.text || 'Updated password!' });
      }).catch((error) => {
        // display error message to user
        this.showMessage({ variant: 'danger', message: error.text || error });
      });
    },
    userCreated (message) {
      this.reloadUsers();
      this.$emit('update-roles');
      this.$bvModal.hide('create-user-modal');
      this.showMessage({ variant: 'success', message });
    },
    /* helper functions ---------------------------------------------------- */
    showMessage ({ variant, message }) {
      this.msg = message;
      this.msgType = variant;
      setTimeout(() => {
        this.msg = '';
        this.msgType = '';
      }, 10000);
    },
    loadUsers () {
      const query = {
        desc: this.desc,
        length: this.perPage,
        filter: this.searchTerm,
        sortField: this.sortField,
        start: (this.currentPage - 1) * this.perPage
      };

      UserService.searchUsers(query).then((response) => {
        this.error = '';
        this.loading = false;
        this.recordsTotal = response.recordsTotal;
        this.users = JSON.parse(JSON.stringify(response.data));
        // don't modify original list - used for comparing
        this.dbUserList = JSON.parse(JSON.stringify(response.data));
      }).catch((error) => {
        this.loading = false;
        this.error = error.text;
      });
    },
    reloadUsers () {
      const query = {
        desc: this.desc,
        length: this.perPage,
        filter: this.searchTerm,
        sortField: this.sortField,
        start: (this.currentPage - 1) * this.perPage
      };

      UserService.searchUsers(query).then((response) => {
        this.error = '';
        this.loading = false;
        const userData = JSON.parse(JSON.stringify(response.data));
        this.recordsTotal = response.recordsTotal;
        // don't modify original list - used for comparing
        this.dbUserList = response.data;

        // Dont update users that have edits. Update dbUserList first to compare against
        // This will keep returned db sorting order regardless if sorted fields are shown on edited fields
        this.users = userData.map(u => {
          const matchedUser = this.users.find(item => item.userId === u.userId);
          // If user already exists and is still being edited, keep user obj
          return (matchedUser && this.userHasChanged(u.userId)) ? matchedUser : u;
        });
      }).catch((error) => {
        this.loading = false;
        this.error = error.text;
      });
    }
  }
};
</script>

<style scoped>
/* center cell content vertically */
.btn-toggle-user {
  margin-top: 2px;
}

/* indication that a user has additional permissions set */
.btn-indicator .btn-toggle-user:not(.expanded) {
  background: linear-gradient(135deg, var(--primary) 1%, var(--primary) 75%, var(--primary) 75%, var(--dark) 77%, var(--dark) 100%);
}

/* make the roles dropdown text smaller */
.roles-dropdown > button, .users-dropdown > button {
  font-size: 0.8rem;
}

.small-table-font {
  font-size: 0.9rem;
}
</style>
