<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-flex flex-column flex-grow-1 overflow-auto pt-3 position-relative h-100">
    <!-- search -->
    <div class="d-flex justify-space-between mx-4">
      <div class="mr-2 flex-grow-1 ">
        <v-text-field
          clearable
          autofocus
          class="medium-input"
          v-model="searchTerm"
          v-debounce="loadUsers"
          @click:clear="loadUsers"
          prepend-inner-icon="mdi-magnify"
          placeholder="Begin typing to search for users by name, id, or role"
        />
      </div>
      <my-pagination
        :page-numbers-visible="4"
        v-model:per-page="perPage"
        v-model:current-page="currentPage"
        :total-items="recordsTotal"
        @per-page-change="perPageChange"
      />
      <div>
        <v-btn
          class="skinny-search-row-btn"
          @click="download"
          color="primary"
          v-tooltip="'Download CSV'"
          title="Download CSV">
          <span class="fa fa-download" />
        </v-btn>
      </div>
    </div> <!-- /search -->

    <!-- error -->
    <div v-if="error"
      class="info-area vertical-center text-monospace">
      <div class="text-error">
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
      <v-data-table
        v-model:expanded="expandedUserIds"
        show-expanded
        item-value="userId"
        hover
        class="table-striped"
        :loading="loading"
        :headers="headers"
        :items="users"
        v-model:sort-by="sortBy"
        :no-data-text="searchTerm ? 'No users or roles match your search' : 'No users or roles'"
        :items-per-page="-1"
        hide-default-footer
      >
        <!-- column headers -->
        <template #headers="{ columns }">
          <tr>
            <th
              :class="{'hover-reveal-sort-arrow cursor-pointer': header.sortable}"
              @click="updateSort(header.key, header.sortable)"
              v-for="header in columns"
              :key="header.key"
              :id="`users-header-${header.key}`"
              :style="header?.headerProps?.style"
            >
              <id-tooltip
                  v-if="header.help"
                  location="top"
                  :target="`users-header-${header.key}`"
              >{{ header.help }}</id-tooltip>
              {{ header.title }}
              <span
                v-if="header.key === 'roles'"
                class="fa fa-info-circle fa-lg cursor-help ml-2"
                v-tooltip="'These roles are applied across apps (Arkime, Parliament, WISE, Cont3xt)'"
              />
              <v-icon
                v-if="header.sortable"
                class="pull-right sort-arrow"
                :class="{'visible': (sortBy[0]?.key === header.key)}"
                :icon="(sortBy[0]?.order === 'desc' && sortBy[0]?.key === header.key) ? 'mdi-arrow-down' : 'mdi-arrow-up'"
                :color="(sortBy[0]?.key === header.key) ? '' : 'muted'"
              />
              <div class="pull-right"
                v-if="header.key === 'action'">
                <v-btn
                  size="small"
                  color="secondary"
                >
                  <span class="fa fa-plus-circle" />
                  <v-menu
                    activator="parent"
                    location="bottom"
                    :close-on-content-click="false"
                  >
                    <v-btn
                      style="width: 150px;"
                      v-if="roles"
                      class="mt-1"
                      size="small"
                      color="success"
                      title="Create a new role"
                      v-tooltip:left="'Create a new role'"
                      @click="createMode = 'role'; userCreateModalOpen = true;">
                      <span class="fa fa-plus-circle mr-1" />
                        Role
                    </v-btn>
                    <v-btn
                      style="width: 150px;"
                      class="mt-1"
                      size="small"
                      color="primary"
                      title="Create a new user"
                      v-tooltip:left="'Create a new user'"
                      @click="createMode = 'user'; userCreateModalOpen = true;">
                      <span class="fa fa-plus-circle mr-1" />
                        User
                    </v-btn>
                  </v-menu>
                </v-btn>
              </div>
            </th>
          </tr>
        </template> <!-- /column headers -->

        <!-- toggle column -->
        <template #item.toggle="{ item, internalItem, isExpanded, toggleExpand }">
          <span :class="{'btn-indicator':!item.emailSearch || !item.removeEnabled || !item.packetSearch || item.hideStats || item.hideFiles || item.hidePcap || item.disablePcapDownload || item.timeLimit || item.expression}">
            <ToggleBtn
            class="btn-toggle-user"
            @toggle="toggleExpand(internalItem)"
            :opened="isExpanded(internalItem)"
            :class="{expanded: isExpanded(internalItem)}"
            :id="`user-togglebtn-${item.userId}`"
            />
            <id-tooltip v-if="!item.emailSearch || !item.removeEnabled || !item.packetSearch || item.hideStats || item.hideFiles || item.hidePcap || item.disablePcapDownload || item.timeLimit || item.expression"
              :target="`user-togglebtn-${item.userId}`"
            >
              This user has additional restricted permissions
            </id-tooltip>
          </span>
        </template> <!-- /toggle column -->
        <!-- action column -->
        <template #item.action="{ item }">
          <div class="pull-right">
            <v-btn
              size="small"
              color="primary"
              @click="openSettings(item.userId)"
              v-has-role="{user:currentUser,roles:'arkimeAdmin'}"
              v-if="parentApp === 'Arkime' && isUser(item)"
              v-tooltip="`Arkime settings for ${item.userId}`">
              <span class="fa fa-gear" />
            </v-btn>
            <v-btn
              size="small"
              color="secondary"
              v-if="parentApp === 'Arkime'"
              @click="openHistory(item.userId)"
              v-tooltip="`History for ${item.userId}`">
              <span class="fa fa-history" />
            </v-btn>
            <!-- cancel confirm delete button -->
            <transition name="buttons">
              <v-btn
                size="small"
                class="square-btn-sm"
                color="warning"
                v-tooltip="'Cancel'"
                title="Cancel"
                v-if="confirmDelete[item.userId]"
                @click="toggleConfirmDeleteUser(item.userId)">
                <span class="fa fa-ban" />
              </v-btn>
            </transition> <!-- /cancel confirm delete button -->
            <!-- confirm delete button -->
            <transition name="buttons">
              <v-btn
                size="small"
                class="square-btn-sm"
                color="error"
                v-tooltip="'Are you sure?'"
                title="Are you sure?"
                v-if="confirmDelete[item.userId]"
                @click="deleteUser(item, data.index)">
                <span class="fa fa-check" />
              </v-btn>
            </transition> <!-- /confirm delete button -->
            <!-- delete button -->
            <transition name="buttons">
              <v-btn
                size="small"
                class="square-btn-sm"
                color="error"
                v-tooltip:start="`Delete ${item.userId}`"
                :title="`Delete ${item.userId}`"
                v-if="!confirmDelete[item.userId]"
                @click="toggleConfirmDeleteUser(item.userId)">
                <span class="fa fa-trash-o" />
              </v-btn>
            </transition> <!-- /delete button -->
          </div>
        </template> <!-- /action column -->
        <!-- user id column -->
        <template #item.userId="{ value }">
          <div class="mt-1">{{ value }}</div>
        </template> <!-- /user id column -->
        <!-- last used column -->
        <template #item.lastUsed="{ value }">
          <div class="mt-1">{{ value ? (tzDateStr(value, currentUser.settings.timezone || 'local', currentUser.settings.ms)) : 'Never' }}</div>
        </template> <!-- /last used column -->

        <template #item.userName="{ item }">
          <v-text-field
            style="min-width: 150px;"
            variant="filled"
            v-model="item.userName"
            @input="userHasChanged(item)"
          />
        </template>

        <template #item.enabled="{ item }">
          <v-checkbox
            class="mt-1"
            data-testid="checkbox"
            v-model="item.enabled"
            @input="userHasChanged(item)"
          />
        </template>

        <template #item.webEnabled="{ item }">
          <v-checkbox
            class="mt-1"
            data-testid="checkbox"
            v-model="item.webEnabled"
            v-if="!item.userId.startsWith('role:')"
            @input="userHasChanged(item)"
          />
        </template>

        <template #item.headerAuthEnabled="{ item }">
          <v-checkbox
            class="mt-1"
            data-testid="checkbox"
            v-model="item.headerAuthEnabled"
            v-if="!item.userId.startsWith('role:')"
            @input="userHasChanged(item)"
          />
        </template>

        <template #item.roles="{ item }">
            <RoleDropdown
              :roles="isUser(item) ? roles : roleAssignableRoles"
              :id="item.userId"
              :selected-roles="item.roles"
              @selected-roles-updated="updateRoles"
            />
        </template>

        <!-- detail row -->
        <template #expanded-row="{ columns, item }">
          <tr>
            <td :colspan="columns.length">
              <div class="row mb-2">
                <v-checkbox inline
                  data-testid="checkbox"
                  :model-value="!item.emailSearch"
                  v-if="isUser(item)"
                  @update:model-value="newVal => negativeToggle(newVal, item, 'emailSearch', true)"
                  label="Disable Arkime Email Search" />
                <v-checkbox inline
                  data-testid="checkbox"
                  :model-value="!item.removeEnabled"
                  v-if="isUser(item)"
                  @update:model-value="newVal => negativeToggle(newVal, item, 'removeEnabled', true)"
                  label="Disable Arkime Data Removal" />
                <v-checkbox inline
                  data-testid="checkbox"
                  :model-value="!item.packetSearch"
                  v-if="isUser(item)"
                  @update:model-value="newVal => negativeToggle(newVal, item, 'packetSearch', true)"
                  label="Disable Arkime Hunting" />
                <v-checkbox inline
                  data-testid="checkbox"
                  v-model="item.hideStats"
                  v-if="isUser(item)"
                  @update:model-value="userHasChanged(item)"
                  label="Hide Arkime Stats Page" />
                <v-checkbox inline
                  data-testid="checkbox"
                  v-model="item.hideFiles"
                  v-if="isUser(item)"
                  @update:model-value="userHasChanged(item)"
                  label="Hide Arkime Files Page" />
                <v-checkbox inline
                  data-testid="checkbox"
                  v-model="item.hidePcap"
                  v-if="isUser(item)"
                  @update:model-value="userHasChanged(item)"
                  label="Hide Arkime PCAP" />
                <v-checkbox inline
                  data-testid="checkbox"
                  v-model="item.disablePcapDownload"
                  v-if="isUser(item)"
                  @update:model-value="userHasChanged(item)"
                  label="Disable Arkime PCAP Download" />
              </div>
              <div class="d-flex flex-row mb-2">
                <v-text-field
                  class="mr-1 flex-grow-1"
                  label="Forced Expression"
                  v-tooltip="'An Arkime search expression that is silently added to all queries. Useful to limit what data can be accessed (e.g. which nodes or IPs)'"
                  v-model="item.expression"
                  @input="userHasChanged(item)"
                />
                <v-select
                  class="flex-shrink-1 flex-grow-0"
                  label="Query Time Limit"
                  v-tooltip="'Restrict the maximum time window of a query'"
                  :items="[
                    { value: 1, text: '1 hour' },
                    { value: 6, text: '6 hours' },
                    { value: 24, text: '24 hours' },
                    { value: 48, text: '48 hours' },
                    { value: 72, text: '72 hours' },
                    { value: 168, text: '1 week' },
                    { value: 336, text: '2 weeks' },
                    { value: 720, text: '1 month' },
                    { value: 1440, text: '2 months' },
                    { value: 4380, text: '6 months' },
                    { value: 8760, text: '1 year' },
                    // null, since v-select will falls-back to `text` if `value` is undefined
                    { value: null, text: 'All (careful)' }
                  ]"
                  item-title="text"
                  item-value="value"
                  :model-value="item.timeLimit"
                  @update:model-value="val => { item.timeLimit = val; changeTimeLimit(item); }"
                />
              </div>
              <!-- display change password if not a role and
                we're in cont3xt or arkime
                (assumes user is a usersAdmin since only usersAdmin can see this page) -->
              <template v-if="parentApp === 'Cont3xt' || parentApp === 'Arkime'">
                <form v-if="isUser(item)" style="display: contents"
                  class="d-flex flex-row mb-2">
                  <!-- new password -->
                  <v-text-field
                    class="mr-2"
                    label="New Password"
                    type="password"
                    v-model="newPassword"
                    autocomplete="new-password"
                    @keydown.enter="changePassword(item.userId)"
                    placeholder="Enter a new password"
                  />
                  <v-text-field
                    class="mr-2"
                    label="Confirm Password"
                    type="password"
                    v-model="confirmNewPassword"
                    autocomplete="new-password"
                    @keydown.enter="changePassword(item.userId)"
                    placeholder="Confirm the new password"
                  />
                  <v-btn
                    size="small"
                    color="success"
                    @click="changePassword(item.userId)">
                    Change Password
                  </v-btn>
                </form>
                <div v-else class="w-100">
                  <UserDropdown label="Role Assigners: "
                    :selected-users="item.roleAssigners || []"
                    :role-id="item.userId"
                    @selected-users-updated="updateRoleAssigners" />
                </div>
              </template>
            </td>
          </tr>
        </template><!-- /detail row -->
      </v-data-table>
    </div> <!-- /users table -->

    <!-- create user -->
    <UserCreate
      :roles="createMode === 'user' ? roles : roleAssignableRoles"
      :create-mode="createMode"
      v-model:modal-open="userCreateModalOpen"
      @user-created="userCreated"
    />

    <!-- messages -->
    <v-alert
      :model-value="!!msg"
      @click.close="clearMessage"
      class="position-fixed bottom-0 ma-0 rounded-0"
      style="z-index: 2000;"
      :color="msgType"
      closable>
      {{ msg }}
    </v-alert> <!-- messages -->
  </div>
</template>

<script>
import HasRole from './HasRole.vue';
import ToggleBtn from './ToggleBtn.vue';
import UserCreate from './UserCreate.vue';
import UserService from '@real_common/UserService';
import RoleDropdown from './RoleDropdown.vue';
import UserDropdown from './UserDropdown.vue';
import MyPagination from '@/utils/MyPagination.vue';
import IdTooltip from '@/utils/IdTooltip.vue';
import { timezoneDateString } from './vueFilters';

let userChangeTimeout;

export default {
  name: 'UsersCommon',
  directives: { HasRole },
  components: {
    ToggleBtn,
    UserCreate,
    RoleDropdown,
    UserDropdown,
    MyPagination,
    IdTooltip
  },
  props: {
    roles: Array,
    parentApp: String,
    currentUser: Object
  },
  emits: ['update-roles', 'update-current-user'],
  data () {
    return {
      error: '',
      msg: '',
      msgType: '',
      clearMessageTimeout: undefined,
      loading: true,
      searchTerm: '',
      users: undefined,
      dbUserList: undefined,
      changed: {},
      recordsTotal: 0,
      perPage: 100,
      currentPage: 1,
      sortBy: [{ key: 'userId', order: 'asc' }],
      createMode: 'user',
      headers: [
        { title: '', key: 'toggle', sortable: false },
        { title: 'ID', key: 'userId', sortable: true, required: true, help: 'The ID used for login (cannot be changed once created)', headerProps: { style: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' } },
        { title: 'Name', key: 'userName', sortable: true, type: 'text', required: true, help: 'Friendly/readable name', headerProps: { style: 'width:250px;white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' } },
        { title: 'Enabled', key: 'enabled', sortable: true, type: 'checkbox', help: 'Is the account currently enabled for anything?', headerProps: { style: 'min-width:120px;' } },
        { title: 'Web Interface', key: 'webEnabled', sortable: true, type: 'checkbox-notrole', help: 'Can access the web interface. When off only APIs can be used', headerProps: { style: 'min-width:80px;' } },
        { title: 'Web Auth Header', key: 'headerAuthEnabled', sortable: true, type: 'checkbox-notrole', help: 'Can login using the web auth header. This setting doesn\'t disable the password so it should be scrambled', headerProps: { style: 'min-width:100px;' } },
        { title: 'Roles', key: 'roles', sortable: false, type: 'select', help: 'Roles assigned', headerProps: { style: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' } },
        { title: 'Last Used', key: 'lastUsed', sortable: true, type: 'checkbox', help: 'The last time Arkime was used by this account', headerProps: { style: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' } },
        { title: '', key: 'action', sortable: false, headerProps: { style: 'width:190px;white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' } }
      ],
      newPassword: '',
      confirmNewPassword: '',
      confirmDelete: {},
      expandedUserIds: [],
      userCreateModalOpen: false
    };
  },
  computed: {
    roleAssignableRoles () {
      return this.roles.filter(({ value }) => value !== 'superAdmin' && value !== 'usersAdmin');
    },
    sortField () {
      return this.sortBy?.[0]?.key ?? 'userId';
    },
    sortIsDesc () {
      return this.sortBy?.[0]?.order === 'desc';
    }
  },
  created () {
    this.loadUsers();
  },
  watch: {
    sortBy (newVal, oldVal) {
      this.loadUsers();
    },
    currentPage () {
      this.loadUsers();
    }
  },
  methods: {
    updateSort (key, isSortable) {
      if (!isSortable) { return; }

      if (this.sortBy?.[0] == null || this.sortBy[0].key !== key) {
        this.sortBy = [{
          key,
          order: 'asc'
        }];
      } else {
        const { order } = this.sortBy?.[0];

        if (order === 'desc') {
          this.sortBy = []; // clear sortBy
        } else {
          this.sortBy[0].order = 'desc';
        }
      }
    },
    /* exposed page functions ---------------------------------------------- */
    tzDateStr (date, tz, ms) {
      return timezoneDateString(date, tz, ms);
    },
    perPageChange (newVal) {
      this.perPage = newVal;
      this.loadUsers(false);
    },
    negativeToggle (newVal, user, field, existing) {
      user[field] = !newVal;
      if (existing) { this.userHasChanged(user); }
    },
    changeTimeLimit (user) {
      if (user.timeLimit == null) {
        delete user.timeLimit;
      }

      this.userHasChanged(user);
    },
    updateRoles (roles, userId) {
      const user = this.users.find(u => u.userId === userId);
      user.roles = roles;
      this.userHasChanged(user);
    },
    updateRoleAssigners ({ newSelection }, roleId) {
      const role = this.users.find(u => u.userId === roleId);
      role.roleAssigners = newSelection;
      this.userHasChanged(role);
    },
    normalizeUser (unNormalizedUser) {
      const user = JSON.parse(JSON.stringify(unNormalizedUser));
      // remove _showDetails for user (added by b-table when user row is expanded)
      delete user._showDetails;

      // roles might be undefined, but compare to empty array since toggling on
      // any roles sets roles to an array, and removing that role = empty array
      user.roles ??= [];

      // sort, since order is not meaningful for roles
      user.roles.sort();

      // roleAssigners may be undefined, which should be functionally synonymous with an empty array
      user.roleAssigners ??= [];

      // sort, since order is not meaningful for roleAssigners
      user.roleAssigners.sort();

      // make sure these fields exist or the objects will be different
      // (undefined is the same as false for these fields)
      user.timeLimit ||= undefined;
      user.hidePcap ||= undefined;
      user.hideFiles ||= undefined;
      user.hideStats ||= undefined;
      user.disablePcapDownload ||= undefined;

      user.expanded = undefined; // don't care about expanded field (just for UI)
      user.lastUsed = undefined; // don't compare lastUsed, it might be different if the user is using the UI
      return user;
    },
    isUser (userOrRoleObj) {
      return !userOrRoleObj.userId.startsWith('role:');
    },
    userHasChanged (user) {
      this.changed[user.id] = true;

      if (userChangeTimeout) { clearTimeout(userChangeTimeout); }
      // debounce the input so it only saves after 600ms
      userChangeTimeout = setTimeout(() => {
        userChangeTimeout = null;
        this.updateUser(user);
      }, 600);
    },
    updateUser (user) {
      UserService.updateUser(user).then((response) => {
        this.changed[user.userId] = false;
        this.showMessage({ variant: 'success', message: response.text });

        const oldUser = this.dbUserList.find(u => u.userId === user.userId);
        const currentUserRoleAssignmentChanged =
            user.roleAssigners?.includes(this.currentUser.userId) !== oldUser.roleAssigners?.includes(this.currentUser.userId);

        // update the current user if they were changed or if their assignableRoles should be changed
        if (this.currentUser.userId === user.userId || currentUserRoleAssignmentChanged) {
          this.emitCurrentUserUpdate();
        }
      }).catch((error) => {
        this.showMessage({ variant: 'error', message: error.text });
      });
    },
    toggleConfirmDeleteUser (id) {
      this.confirmDelete[id] = !this.confirmDelete[id];
    },
    deleteUser (user, index) {
      UserService.deleteUser(user).then((response) => {
        this.users.splice(index, 1);
        this.showMessage({ variant: 'success', message: response.text });
        if (user.roleAssigners?.includes(this.currentUser.userId)) {
          this.emitCurrentUserUpdate(); // update current user if one of their assignable roles is deleted
        }
      }).catch((error) => {
        this.showMessage({ variant: 'error', message: error.text });
      });
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
    clearMessage () {
      this.msg = '';
      this.msgType = '';
      if (this.clearMessageTimeout) {
        clearTimeout(this.clearMessageTimeout);
        this.clearMessageTimeout = undefined;
      }
    },
    changePassword (userId) {
      this.clearMessage();

      if (!this.newPassword) {
        this.showMessage({
          variant: 'error',
          message: 'You must enter a new password'
        });
        return;
      }

      if (!this.confirmNewPassword) {
        this.showMessage({
          variant: 'error',
          message: 'You must confirm your new password'
        });
        return;
      }

      if (this.newPassword !== this.confirmNewPassword) {
        this.showMessage({
          variant: 'error',
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
        this.showMessage({ variant: 'error', message: error.text || error });
      });
    },
    userCreated (message, user) {
      this.reloadUsers();
      this.$emit('update-roles');
      if (user.roleAssigners?.includes(this.currentUser.userId)) {
        this.emitCurrentUserUpdate(); // update current user if they were made an assigner
      }
      this.userCreateModalOpen = false;
      this.showMessage({ variant: 'success', message });
    },
    download () {
      const query = this.getUsersQuery();

      UserService.downloadCSV(query).then((response) => {
        // display success message to user
        this.showMessage({ variant: 'success', message: response.text || 'Downloaded!' });
      }).catch((error) => {
        // display error message to user
        this.showMessage({ variant: 'error', message: error.text || error });
      });
    },
    /* helper functions ---------------------------------------------------- */
    emitCurrentUserUpdate () {
      this.$emit('update-current-user');
    },
    showMessage ({ variant, message }) {
      if (this.clearMessageTimeout != null) {
        this.clearMessage();
      }

      this.msg = message;
      this.msgType = variant;

      this.clearMessageTimeout = setTimeout(() => {
        this.clearMessage();
      }, 10000);
    },
    getUsersQuery () {
      return {
        desc: this.sortIsDesc,
        length: this.perPage,
        filter: this.searchTerm,
        sortField: this.sortField,
        start: (this.currentPage - 1) * this.perPage
      };
    },
    loadUsers () {
      const query = this.getUsersQuery();

      UserService.searchUsers(query).then((response) => {
        this.error = '';
        this.loading = false;
        this.recordsTotal = response.recordsTotal;
        this.users = JSON.parse(JSON.stringify(response.data));
        // don't modify original list - used for comparing
        this.dbUserList = JSON.parse(JSON.stringify(response.data));
      }).catch((error) => {
        this.loading = false;
        this.showMessage({ variant: 'error', message: error.text ?? 'Failed to load users' });
      });
    },
    reloadUsers () {
      const query = {
        desc: this.sortIsDesc,
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
        this.dbUserList = response.data;
      }).catch((error) => {
        this.loading = false;
        this.showMessage({ variant: 'error', message: error.text ?? 'Failed to reload users' });
      });
    }
  }
};
</script>

<style scoped>
/* center cell content vertically */
.btn-toggle-user {
  margin-top: 2px;
  margin-bottom: 2px;
}

/* indication that a user has additional permissions set */
.btn-indicator .btn-toggle-user:not(.expanded) {
  background: linear-gradient(135deg, rgb(var(--v-theme-primary)) 1%, rgb(var(--v-theme-primary)) 75%, rgb(var(--v-theme-primary)) 75%, rgb(var(--v-theme-dark)) 77%, rgb(var(--v-theme-dark)) 100%);
}

/* make the roles dropdown text smaller */
.roles-dropdown > button, .users-dropdown > button {
  font-size: 0.8rem;
}

.small-table-font {
  font-size: 0.9rem;
}

/* show sort arrows when parent <td> is hovered */
.hover-reveal-sort-arrow .sort-arrow {
  visibility: hidden;
  position: absolute;
}
.hover-reveal-sort-arrow:hover .sort-arrow {
  visibility: visible;
}
</style>
