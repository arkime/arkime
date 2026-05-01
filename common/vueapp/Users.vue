<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <!-- search/paging/download chrome -->
    <div class="d-flex align-items-center mt-3 mb-2">
      <div class="me-2 flex-grow-1">
        <v-text-field
          autofocus
          density="compact"
          variant="outlined"
          hide-details
          clearable
          prepend-inner-icon="fa-search"
          v-model="searchTerm"
          :placeholder="$t('users.searchPlaceholder')" />
      </div>
      <v-select
        class="users-per-page"
        density="compact"
        variant="outlined"
        hide-details
        item-title="text"
        item-value="value"
        :items="perPageOptions"
        :model-value="perPage"
        @update:model-value="perPageChange">
        <template #selection="{ item }">
          <span class="users-per-page-display">{{ item.raw.value }}</span>
        </template>
      </v-select>
      <v-pagination
        class="users-paging"
        density="compact"
        :total-visible="3"
        :length="totalPages"
        :model-value="currentPage"
        @update:model-value="currentPage = $event" />
      <span class="users-pagination-info me-2">
        <span v-if="recordsTotal">
          {{ $t('common.showingRange', { start: commaString(((currentPage - 1) * perPage) + 1), end: commaString(Math.min(currentPage * perPage, recordsTotal)), total: commaString(recordsTotal) }) }}
        </span>
        <span v-else>
          {{ $t('common.showingAll', { count: 0, total: 0 }) }}
        </span>
      </span>
      <button
        type="button"
        class="btn btn-sm btn-primary"
        @click="download"
        :title="$t('users.downloadCSVTip')">
        <span class="fa fa-download" />
      </button>
    </div> <!-- /chrome -->

    <!-- error -->
    <div
      v-if="error"
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
          <br>
          {{ $t('common.loading') }}
        </div>
      </slot>
    </template> <!-- /loading -->

    <!-- users table -->
    <div v-if="!error && !loading">
      <v-data-table
        density="compact"
        must-sort
        show-expand
        item-value="userId"
        :items="users"
        :headers="tableHeaders"
        :items-per-page="-1"
        hide-default-footer
        v-model:sort-by="sortBy"
        @update:sort-by="sortChanged"
        class="users-table-striped">
        <template #no-data>
          {{ searchTerm ? $t('users.noUsersOrRolesMatch') : $t('users.noUsersOrRoles') }}
        </template>

        <!-- header slots: each column gets a hover tooltip with the help text -->
        <template #[`header.userId`]="{ column }">
          <span class="header-with-tip">
            {{ column.title }}
            <v-tooltip
              activator="parent"
              location="top">{{ $t('users.userIdTip') }}</v-tooltip>
          </span>
        </template>
        <template #[`header.userName`]="{ column }">
          <span class="header-with-tip">
            {{ column.title }}
            <v-tooltip
              activator="parent"
              location="top">{{ $t('users.userNameTip') }}</v-tooltip>
          </span>
        </template>
        <template #[`header.enabled`]="{ column }">
          <span class="header-with-tip">
            {{ column.title }}
            <v-tooltip
              activator="parent"
              location="top">{{ $t('users.enabledTip') }}</v-tooltip>
          </span>
        </template>
        <template #[`header.webEnabled`]="{ column }">
          <span class="header-with-tip">
            {{ column.title }}
            <v-tooltip
              activator="parent"
              location="top">{{ $t('users.webEnabledTip') }}</v-tooltip>
          </span>
        </template>
        <template #[`header.headerAuthEnabled`]="{ column }">
          <span class="header-with-tip">
            {{ column.title }}
            <v-tooltip
              activator="parent"
              location="top">{{ $t('users.headerAuthEnabledTip') }}</v-tooltip>
          </span>
        </template>
        <template #[`header.roles`]>
          <span class="header-with-tip">
            {{ $t('users.roles') }}
            <span
              id="roles-help"
              class="fa fa-info-circle fa-lg cursor-help ms-2" />
            <v-tooltip activator="#roles-help">
              {{ $t('users.rolesTip') }}
            </v-tooltip>
          </span>
        </template>
        <template #[`header.lastUsed`]="{ column }">
          <span class="header-with-tip">
            {{ column.title }}
            <v-tooltip
              activator="parent"
              location="top">{{ $t('users.lastUsedTip') }}</v-tooltip>
          </span>
        </template>
        <template #[`header.action`]>
          <div class="pull-right">
            <button
              type="button"
              v-if="roles"
              class="btn btn-sm btn-success"
              :title="$t('users.createRoleTip')"
              @click="createMode = 'role'; showUserCreateModal = true">
              <span class="fa fa-plus-circle me-1" />
              {{ $t('common.role') }}
            </button>
            <button
              type="button"
              class="btn btn-sm btn-primary ms-2"
              :title="$t('users.createUserTip')"
              @click="createMode = 'user'; showUserCreateModal = true">
              <span class="fa fa-plus-circle me-1" />
              {{ $t('common.user') }}
            </button>
          </div>
        </template>

        <!-- expand-icon cell: keep auto-toggle but add restriction indicator class -->
        <template #[`item.data-table-expand`]="{ item, internalItem, toggleExpand, isExpanded }">
          <span :class="{'btn-indicator': hasRestrictions(item)}">
            <button
              type="button"
              class="btn btn-xs btn-link p-0"
              :title="hasRestrictions(item) ? $t('users.restrictedTip') : ''"
              @click="toggleExpand(internalItem)">
              <span
                class="fa"
                :class="isExpanded(internalItem) ? 'fa-chevron-up' : 'fa-chevron-down'" />
            </button>
          </span>
        </template>

        <!-- per-cell slots -->
        <template #[`item.userId`]="{ item }">
          <div class="mt-1">
            {{ item.userId }}
          </div>
        </template>
        <template #[`item.userName`]="{ item }">
          <input
            type="text"
            class="form-control form-control-sm"
            v-model="item.userName"
            @input="userHasChanged(item)">
        </template>
        <template #[`item.enabled`]="{ item }">
          <input
            type="checkbox"
            class="form-check-input mt-1"
            data-testid="checkbox"
            v-model="item.enabled"
            @change="userHasChanged(item)">
        </template>
        <template #[`item.webEnabled`]="{ item }">
          <input
            v-if="!item.userId.startsWith('role:')"
            type="checkbox"
            class="form-check-input mt-1"
            data-testid="checkbox"
            v-model="item.webEnabled"
            @change="userHasChanged(item)">
        </template>
        <template #[`item.headerAuthEnabled`]="{ item }">
          <input
            v-if="!item.userId.startsWith('role:')"
            type="checkbox"
            class="form-check-input mt-1"
            data-testid="checkbox"
            v-model="item.headerAuthEnabled"
            @change="userHasChanged(item)">
        </template>
        <template #[`item.roles`]="{ item }">
          <RoleDropdown
            v-if="roles && roles.length"
            :roles="isUser(item) ? roles : roleAssignableRoles"
            :id="item.userId"
            :selected-roles="item.roles"
            @selected-roles-updated="updateRoles"
            :truncate="4" />
        </template>
        <template #[`item.lastUsed`]="{ item }">
          <div class="mt-1">
            {{ item.lastUsed ? (tzDateStr(item.lastUsed, currentUser.settings.timezone || 'local', currentUser.settings.ms)) : $t('common.never') }}
          </div>
        </template>
        <template #[`item.action`]="{ item, index }">
          <div class="pull-right">
            <button
              type="button"
              class="btn btn-sm btn-primary ms-1"
              v-if="parentApp === 'Arkime' && isUser(item)"
              v-has-role="{user:currentUser,roles:'arkimeAdmin'}"
              @click="openSettings(item.userId)"
              :title="$t('users.settingsFor', {user: item.userId})">
              <span class="fa fa-gear" />
            </button>
            <button
              type="button"
              class="btn btn-sm btn-secondary ms-1"
              v-if="parentApp === 'Arkime'"
              @click="openHistory(item.userId)"
              :title="$t('users.historyFor', {user: item.userId})">
              <span class="fa fa-history" />
            </button>
            <transition name="buttons">
              <button
                type="button"
                class="btn btn-sm btn-warning ms-1"
                :title="$t('common.cancel')"
                v-if="confirmDelete[item.userId]"
                @click="toggleConfirmDeleteUser(item.userId)">
                <span class="fa fa-ban" />
              </button>
            </transition>
            <transition name="buttons">
              <button
                type="button"
                class="btn btn-sm btn-danger ms-1"
                :title="$t('common.areYouSure')"
                v-if="confirmDelete[item.userId]"
                @click="deleteUser(item, index)">
                <span class="fa fa-check" />
              </button>
            </transition>
            <transition name="buttons">
              <button
                type="button"
                class="btn btn-sm btn-danger ms-1"
                :title="$t('users.deleteUser', {user: item.userId})"
                v-if="!confirmDelete[item.userId]"
                @click="toggleConfirmDeleteUser(item.userId)">
                <span class="fa fa-trash-o" />
              </button>
            </transition>
          </div>
        </template>

        <!-- expanded row -->
        <template #expanded-row="{ item, columns }">
          <tr class="user-detail-row">
            <td :colspan="columns.length">
              <div class="m-2">
                <!-- user permission tri-state toggles -->
                <div
                  v-if="isUser(item)"
                  class="user-permissions mt-2 mb-2 d-flex flex-wrap gap-1">
                  <TriStateToggle
                    class="toggle-group rounded p-1"
                    :model-value="item.emailSearch"
                    :label="$t('users.disableEmailSearch')"
                    :negated="true"
                    @update:model-value="setRoleField(item, 'emailSearch', $event)" />
                  <TriStateToggle
                    class="toggle-group rounded p-1"
                    :model-value="item.removeEnabled"
                    :label="$t('users.disableDataRemoval')"
                    :negated="true"
                    @update:model-value="setRoleField(item, 'removeEnabled', $event)" />
                  <TriStateToggle
                    class="toggle-group rounded p-1"
                    :model-value="item.packetSearch"
                    :label="$t('users.disableHunting')"
                    :negated="true"
                    @update:model-value="setRoleField(item, 'packetSearch', $event)" />
                  <TriStateToggle
                    class="toggle-group rounded p-1"
                    :model-value="item.hideStats"
                    :label="$t('users.hideStatsPage')"
                    @update:model-value="setRoleField(item, 'hideStats', $event)" />
                  <TriStateToggle
                    class="toggle-group rounded p-1"
                    :model-value="item.hideFiles"
                    :label="$t('users.hideFilesPage')"
                    @update:model-value="setRoleField(item, 'hideFiles', $event)" />
                  <TriStateToggle
                    class="toggle-group rounded p-1"
                    :model-value="item.hidePcap"
                    :label="$t('users.hidePcap')"
                    @update:model-value="setRoleField(item, 'hidePcap', $event)" />
                  <TriStateToggle
                    class="toggle-group rounded p-1"
                    :model-value="item.disablePcapDownload"
                    :label="$t('users.disablePcapDownload')"
                    @update:model-value="setRoleField(item, 'disablePcapDownload', $event)" />
                </div>

                <div class="input-group input-group-sm mt-2">
                  <span
                    :id="`${item.userId}-expression`"
                    class="input-group-text">
                    {{ $t('users.forcedExpression') }}
                    <v-tooltip :activator="`[id='${item.userId}-expression']`">
                      {{ $t('users.forcedExpressionTip') }}
                    </v-tooltip>
                  </span>
                  <input
                    type="text"
                    class="form-control"
                    v-model="item.expression"
                    @input="userHasChanged(item)">
                </div>

                <div class="input-group input-group-sm mt-2 w-25">
                  <span
                    :id="`${item.userId}-timeLimit`"
                    class="input-group-text">
                    {{ $t('users.queryTimeLimit') }}
                    <v-tooltip :activator="`[id='${item.userId}-timeLimit']`">
                      {{ $t('users.queryTimeLimitTip') }}
                    </v-tooltip>
                  </span>
                  <select
                    class="form-control"
                    v-model="item.timeLimit"
                    @change="changeTimeLimit(item)">
                    <option value="1">
                      {{ $t('common.hourCount', { count: 1 }) }}
                    </option>
                    <option value="6">
                      {{ $t('common.hourCount', { count: 6 }) }}
                    </option>
                    <option value="24">
                      {{ $t('common.hourCount', { count: 24 }) }}
                    </option>
                    <option value="48">
                      {{ $t('common.hourCount', { count: 48 }) }}
                    </option>
                    <option value="72">
                      {{ $t('common.hourCount', { count: 72 }) }}
                    </option>
                    <option value="168">
                      {{ $t('common.weekCount', { count: 1 }) }}
                    </option>
                    <option value="336">
                      {{ $t('common.weekCount', { count: 2 }) }}
                    </option>
                    <option value="720">
                      {{ $t('common.monthCount', { count: 1 }) }}
                    </option>
                    <option value="1440">
                      {{ $t('common.monthCount', { count: 2 }) }}
                    </option>
                    <option value="4380">
                      {{ $t('common.monthCount', { count: 6 }) }}
                    </option>
                    <option value="8760">
                      {{ $t('common.yearCount', { count: 1 }) }}
                    </option>
                    <option value="undefined">
                      {{ $t('common.allCareful') }}
                    </option>
                  </select>
                </div>

                <!-- password change for users / role-permissions for roles -->
                <template v-if="parentApp === 'Cont3xt' || parentApp === 'Arkime'">
                  <form
                    class="row"
                    v-if="isUser(item)">
                    <div class="col-9 mt-4">
                      <div class="input-group input-group-sm mt-2">
                        <span class="input-group-text">{{ $t('users.newPassword') }}</span>
                        <input
                          type="password"
                          class="form-control"
                          v-model="newPassword"
                          autocomplete="new-password"
                          @keydown.enter="changePassword(item.userId)"
                          :placeholder="$t('users.newPasswordPlaceholder')">
                      </div>
                      <div class="input-group input-group-sm mt-2">
                        <span class="input-group-text">{{ $t('users.confirmPassword') }}</span>
                        <input
                          type="password"
                          class="form-control"
                          autocomplete="new-password"
                          v-model="confirmNewPassword"
                          @keydown.enter="changePassword(item.userId)"
                          :placeholder="$t('users.confirmPasswordPlaceholder')">
                      </div>
                      <button
                        type="button"
                        class="btn btn-sm btn-success mt-2"
                        @click="changePassword(item.userId)">
                        {{ $t('users.changePassword') }}
                      </button>
                    </div>
                  </form>
                  <div v-else>
                    <!-- role permission tri-state toggles -->
                    <div class="role-permissions mt-2 mb-2 d-flex flex-wrap gap-1">
                      <TriStateToggle
                        class="toggle-group rounded p-1"
                        :model-value="item.emailSearch"
                        :label="$t('users.disableEmailSearch')"
                        :negated="true"
                        @update:model-value="setRoleField(item, 'emailSearch', $event)" />
                      <TriStateToggle
                        class="toggle-group rounded p-1"
                        :model-value="item.removeEnabled"
                        :label="$t('users.disableDataRemoval')"
                        :negated="true"
                        @update:model-value="setRoleField(item, 'removeEnabled', $event)" />
                      <TriStateToggle
                        class="toggle-group rounded p-1"
                        :model-value="item.packetSearch"
                        :label="$t('users.disableHunting')"
                        :negated="true"
                        @update:model-value="setRoleField(item, 'packetSearch', $event)" />
                      <TriStateToggle
                        class="toggle-group rounded p-1"
                        :model-value="item.hideStats"
                        :label="$t('users.hideStatsPage')"
                        @update:model-value="setRoleField(item, 'hideStats', $event)" />
                      <TriStateToggle
                        class="toggle-group rounded p-1"
                        :model-value="item.hideFiles"
                        :label="$t('users.hideFilesPage')"
                        @update:model-value="setRoleField(item, 'hideFiles', $event)" />
                      <TriStateToggle
                        class="toggle-group rounded p-1"
                        :model-value="item.hidePcap"
                        :label="$t('users.hidePcap')"
                        @update:model-value="setRoleField(item, 'hidePcap', $event)" />
                      <TriStateToggle
                        class="toggle-group rounded p-1"
                        :model-value="item.disablePcapDownload"
                        :label="$t('users.disablePcapDownload')"
                        @update:model-value="setRoleField(item, 'disablePcapDownload', $event)" />
                    </div>
                    <UserDropdown
                      class="mt-2"
                      label="Role Assigners&nbsp;"
                      :selected-users="item.roleAssigners || []"
                      :role-id="item.userId"
                      @selected-users-updated="updateRoleAssigners" />
                  </div>
                </template>
              </div>
            </td>
          </tr>
        </template>
      </v-data-table>
    </div> <!-- /users table -->

    <!-- create user -->
    <UserCreate
      :show-modal="showUserCreateModal"
      :roles="createMode === 'user' ? roles : roleAssignableRoles"
      :create-mode="createMode"
      @user-created="userCreated"
      @close="showUserCreateModal = false" />

    <!-- messages (success/error) displayed at bottom of page -->
    <div
      v-if="msg"
      style="z-index: 2000;"
      :class="`alert-${msgType}`"
      class="alert position-fixed fixed-bottom m-0 rounded-0">
      {{ msg }}
      <button
        type="button"
        class="btn-close pull-right"
        @click="msg = ''" />
    </div> <!-- /messages -->
  </div>
</template>

<script>
import HasRole from './HasRole.vue';
import UserCreate from './UserCreate.vue';
import UserService from './UserService.js';
import RoleDropdown from './RoleDropdown.vue';
import UserDropdown from './UserDropdown.vue';
import TriStateToggle from './TriStateToggle.vue';
import { timezoneDateString, commaString } from './vueFilters.js';
import { resolveMessage } from './resolveI18nMessage';

let userChangeTimeout;

export default {
  name: 'UsersCommon',
  directives: { HasRole },
  components: {
    UserCreate,
    RoleDropdown,
    UserDropdown,
    TriStateToggle
  },
  emits: ['update-roles', 'update-current-user'],
  props: {
    roles: {
      type: Array,
      default: () => []
    },
    parentApp: {
      type: String,
      default: ''
    },
    currentUser: {
      type: Object,
      default: () => ({})
    },
    dark: { type: Boolean, default: false }
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
      sortBy: [{ key: 'userId', order: 'asc' }],
      createMode: 'user',
      // password
      newPassword: '',
      confirmNewPassword: '',
      confirmDelete: {},
      // create user
      showUserCreateModal: false
    };
  },
  computed: {
    roleAssignableRoles () {
      return this.roles.filter(({ value }) => value !== 'superAdmin' && value !== 'usersAdmin');
    },
    tableHeaders () {
      const $t = this.$t;
      const mk = (key, opts = {}) => ({
        title: $t('users.' + key),
        key,
        sortable: opts.sortable ?? true,
        ...opts
      });
      return [
        mk('userId'),
        mk('userName', { width: '250px' }),
        mk('enabled'),
        mk('webEnabled'),
        mk('headerAuthEnabled'),
        mk('roles', { sortable: false }),
        mk('lastUsed'),
        { title: '', key: 'action', sortable: false, width: '190px' }
      ];
    },
    perPageOptions () {
      return [
        { value: 50, text: this.$t('common.perPage', { count: 50 }) },
        { value: 100, text: this.$t('common.perPage', { count: 100 }) },
        { value: 200, text: this.$t('common.perPage', { count: 200 }) },
        { value: 500, text: this.$t('common.perPage', { count: 500 }) }
      ];
    },
    totalPages () {
      return Math.max(1, Math.ceil(this.recordsTotal / this.perPage));
    }
  },
  created () {
    this.loadUsers();
  },
  watch: {
    searchTerm () {
      this.loadUsers();
    },
    currentPage () {
      this.loadUsers();
    }
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    commaString,
    tzDateStr (date, tz, ms) {
      return timezoneDateString(date, tz, ms);
    },
    perPageChange (newVal) {
      this.perPage = newVal;
      this.loadUsers(false);
    },
    sortChanged () {
      this.loadUsers();
    },
    negativeToggle (user, field, existing) {
      user[field] = !user[field];
      if (existing) { this.userHasChanged(user); }
    },
    setRoleField (role, field, value) {
      if (value === undefined) {
        delete role[field];
      } else {
        role[field] = value;
      }
      this.userHasChanged(role);
    },
    changeTimeLimit (user) {
      if (user.timeLimit === 'undefined') {
        delete user.timeLimit;
      } else {
        user.timeLimit = parseInt(user.timeLimit);
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
      // remove _showDetails for user (added by BTable when user row is expanded)
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

      // For both users and roles: preserve the exact value (undefined = inherit, true/false = explicit)

      user.expanded = undefined; // don't care about expanded field (just for UI)
      user.lastUsed = undefined; // don't compare lastUsed, it might be different if the user is using the UI
      return user;
    },
    isUser (userOrRoleObj) {
      return !userOrRoleObj.userId.startsWith('role:');
    },
    hasRestrictions (item) {
      return !item.emailSearch || !item.removeEnabled || !item.packetSearch ||
        item.hideStats || item.hideFiles || item.hidePcap ||
        item.disablePcapDownload || item.timeLimit || item.expression;
    },
    userHasChanged (user) {
      this.changed[user.userId] = true;

      if (userChangeTimeout) { clearTimeout(userChangeTimeout); }
      // debounce the input so it only saves after 1s
      userChangeTimeout = setTimeout(() => {
        userChangeTimeout = null;
        this.updateUser(user);
      }, 1000);
    },
    updateUser (user) {
      UserService.updateUser(user).then((response) => {
        this.changed[user.userId] = false;
        this.showMessage({ variant: 'success', message: resolveMessage(response, this.$t) });

        const oldUser = this.dbUserList.find(u => u.userId === user.userId);
        const currentUserRoleAssignmentChanged =
            user.roleAssigners?.includes(this.currentUser.userId) !== oldUser.roleAssigners?.includes(this.currentUser.userId);

        // update the current user if they were changed or if their assignableRoles should be changed
        if (this.currentUser.userId === user.userId || currentUserRoleAssignmentChanged) {
          this.emitCurrentUserUpdate();
        }
      }).catch((error) => {
        this.showMessage({ variant: 'danger', message: resolveMessage(error, this.$t) });
      });
    },
    toggleConfirmDeleteUser (id) {
      this.confirmDelete[id] = !this.confirmDelete[id];
    },
    deleteUser (user, index) {
      UserService.deleteUser(user).then((response) => {
        this.users.splice(index, 1);
        this.showMessage({ variant: 'success', message: resolveMessage(response, this.$t) });
        if (user.roleAssigners?.includes(this.currentUser.userId)) {
          this.emitCurrentUserUpdate(); // update current user if one of their assignable roles is deleted
        }
      }).catch((error) => {
        this.showMessage({ variant: 'danger', message: resolveMessage(error, this.$t) });
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
    changePassword (userId) {
      this.msg = '';

      if (!this.newPassword) {
        this.showMessage({
          variant: 'danger',
          message: this.$t('users.newPasswordMsg')
        });
        return;
      }

      if (!this.confirmNewPassword) {
        this.showMessage({
          variant: 'danger',
          message: this.$t('users.confirmPasswordMsg')
        });
        return;
      }

      if (this.newPassword !== this.confirmNewPassword) {
        this.showMessage({
          variant: 'danger',
          message: this.$t('users.mismatchedPasswordMsg')
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
        this.showMessage({ variant: 'success', message: resolveMessage(response, this.$t) || this.$t('users.changedPasswordMsg') });
      }).catch((error) => {
        // display error message to user
        this.showMessage({ variant: 'danger', message: resolveMessage(error, this.$t) || error });
      });
    },
    userCreated (message, user) {
      this.reloadUsers();
      this.$emit('update-roles');
      if (user.roleAssigners?.includes(this.currentUser.userId)) {
        this.emitCurrentUserUpdate(); // update current user if they were made an assigner
      }
      this.showUserCreateModal = false;
      this.showMessage({ variant: 'success', message });
    },
    download () {
      const query = this.getUsersQuery();

      UserService.downloadCSV(query).then((response) => {
        // display success message to user
        this.showMessage({ variant: 'success', message: resolveMessage(response, this.$t) || this.$t('users.downloadCSVMsg') });
      }).catch((error) => {
        // display error message to user
        this.showMessage({ variant: 'danger', message: resolveMessage(error, this.$t) || error });
      });
    },
    /* helper functions ---------------------------------------------------- */
    emitCurrentUserUpdate () {
      this.$emit('update-current-user');
    },
    showMessage ({ variant, message }) {
      this.msg = message;
      this.msgType = variant;
      setTimeout(() => {
        this.msg = '';
        this.msgType = '';
      }, 10000);
    },
    getUsersQuery () {
      return {
        desc: this.sortBy[0]?.order === 'desc',
        length: this.perPage,
        filter: this.searchTerm,
        sortField: this.sortBy[0]?.key,
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
        this.error = resolveMessage(error, this.$t);
      });
    },
    reloadUsers () {
      const query = {
        desc: this.sortBy[0]?.order === 'desc',
        length: this.perPage,
        filter: this.searchTerm,
        sortField: this.sortBy[0]?.key,
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
        this.error = resolveMessage(error, this.$t);
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
  background: linear-gradient(135deg, var(--bs-primary) 1%, var(--bs-primary) 75%, var(--bs-primary) 75%, var(--bs-primary-border-subtle) 77%, var(--bs-primary-border-subtle) 100%);
}

/* make the roles dropdown text smaller */
.roles-dropdown > button, .users-dropdown > button {
  font-size: 0.8rem;
}

.small-table-font {
  font-size: 0.9rem;
}

.toggle-group {
  background-color: var(--color-white);
  color: var(--color-gray-dark);
}

.pagination-no-right-radius :deep(.page-item:last-child .page-link) {
  border-top-right-radius: 0;
  border-bottom-right-radius: 0;
}

/* ---- Pagination chrome polish: same look as Pagination.vue ---- */
.users-per-page {
  width: 76px;
  flex: 0 0 76px;
  font-size: 0.8rem;
}
.users-per-page :deep(.v-field) {
  --v-input-control-height: 32px;
  align-items: center;
}
.users-per-page :deep(.v-field__field) {
  align-items: center;
}
.users-per-page :deep(.v-field__input) {
  font-size: 0.8rem;
  padding-top: 0;
  padding-bottom: 0;
  padding-inline-start: 8px;
  padding-inline-end: 0;
  min-height: 32px;
  display: flex;
  align-items: center;
}
.users-per-page :deep(.v-field__append-inner) {
  padding-top: 0;
  padding-bottom: 0;
  padding-inline: 4px;
  align-items: center;
}
.users-per-page :deep(.v-field__append-inner .v-icon) {
  font-size: 16px;
  opacity: 0.6;
}
.users-per-page-display {
  white-space: nowrap;
  font-size: 0.8rem;
  line-height: 1;
}

.users-paging {
  padding-left: 0;
  padding-right: 0;
}
.users-paging :deep(.v-pagination__list) {
  margin: 0;
  padding: 0;
  gap: 0;
}
.users-paging :deep(.v-pagination__list > li:first-child) {
  margin-inline-start: 0;
}
.users-paging :deep(.v-pagination__list > li:last-child) {
  margin-inline-end: 0;
}
.users-paging :deep(.v-btn) {
  --v-btn-height: 24px;
  width: 24px;
  min-width: 24px;
  font-size: 0.75rem;
}

.users-pagination-info {
  display: inline-block;
  font-size: 0.8rem;
  color: var(--color-foreground);
  white-space: nowrap;
  margin-left: 4px;
}

/* ---- Users table: more vertical breathing room so inputs/buttons don't run into each other ---- */
.users-table-striped :deep(tbody tr:nth-of-type(odd) > td) {
  background-color: var(--color-gray-lighter) !important;
}
.users-table-striped :deep(tbody tr > td),
.users-table-striped :deep(thead tr > th) {
  padding-top: 6px !important;
  padding-bottom: 6px !important;
}
.users-table-striped :deep(.user-detail-row > td) {
  padding-top: 12px !important;
  padding-bottom: 12px !important;
}
.header-with-tip {
  cursor: help;
}
</style>
