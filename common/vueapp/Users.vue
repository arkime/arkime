<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <!-- search/paging/download chrome -->
    <div class="d-flex align-center mt-2 mb-2 ga-2">
      <div class="arkime-input-group arkime-input-group--fluid align-self-center ms-1">
        <span class="arkime-input-label arkime-input-label-fw">
          <v-icon icon="mdi-magnify" />
        </span>
        <input
          type="text"
          class="arkime-input-control"
          v-focus="true"
          v-model="searchTerm"
          :placeholder="$t('users.searchPlaceholder')">
        <v-btn
          v-if="searchTerm"
          variant="text"
          size="x-small"
          density="comfortable"
          icon
          class="arkime-input-append-btn"
          :aria-label="$t('common.clear')"
          @click="searchTerm = ''">
          <v-icon icon="mdi-close" />
        </v-btn>
      </div>
      <ArkimePaging
        class="align-self-center"
        :records-filtered="recordsFiltered"
        :records-total="recordsTotal"
        :length-default="100"
        @change-paging="onPagingChange" />
      <v-btn
        size="small"
        color="secondary"
        variant="outlined"
        class="align-self-center me-1"
        @click="download"
        :title="$t('users.downloadCSVTip')">
        <v-icon icon="mdi:mdi-download" />
      </v-btn>
    </div> <!-- /chrome -->

    <!-- error -->
    <div
      v-if="error"
      class="info-area vertical-center text-monospace">
      <div class="text-danger">
        <span class="mdi mdi-alert mdi-24px" />
        {{ error }}
      </div>
    </div> <!-- /error -->

    <!-- loading -->
    <template v-if="loading">
      <slot name="loading">
        <div class="text-center mt-5">
          <span class="mdi mdi-loading mdi-spin mdi-24px" />
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

        <!-- Default Vuetify headers render title + sort icon when sortable.
             Tooltips bind by id selector via each column's headerProps so
             we don't have to override the header slot. -->

        <!-- +Role / +User in the right-most header column. -->
        <template #[`header.action`]>
          <div class="float-right">
            <v-btn
              v-if="roles"
              size="x-small"
              color="success"
              variant="flat"
              :title="$t('users.createRoleTip')"
              @click="createMode = 'role'; showUserCreateModal = true">
              <v-icon
                start
                icon="mdi:mdi-plus-circle" />
              {{ $t('common.role') }}
            </v-btn>
            <v-btn
              size="x-small"
              color="primary"
              variant="flat"
              class="ms-2"
              :title="$t('users.createUserTip')"
              @click="createMode = 'user'; showUserCreateModal = true">
              <v-icon
                start
                icon="mdi:mdi-plus-circle" />
              {{ $t('common.user') }}
            </v-btn>
          </div>
        </template>

        <!-- expand-icon cell: keep auto-toggle but add restriction indicator class -->
        <template #[`item.data-table-expand`]="{ item, internalItem, toggleExpand, isExpanded }">
          <span :class="{'btn-indicator': hasRestrictions(item)}">
            <ToggleBtn
              :opened="isExpanded(internalItem)"
              :title="hasRestrictions(item) ? $t('users.restrictedTip') : ''"
              @toggle="toggleExpand(internalItem)" />
          </span>
        </template>

        <!-- per-cell slots -->
        <template #[`item.userId`]="{ item }">
          {{ item.userId }}
        </template>
        <template #[`item.userName`]="{ item }">
          <v-text-field
            density="compact"
            variant="outlined"
            hide-details
            v-model="item.userName"
            @update:model-value="userHasChanged(item)" />
        </template>
        <template #[`item.enabled`]="{ item }">
          <input
            type="checkbox"
            class="arkime-check-input"
            data-testid="checkbox"
            v-model="item.enabled"
            @change="userHasChanged(item)">
        </template>
        <template #[`item.webEnabled`]="{ item }">
          <input
            v-if="!item.userId.startsWith('role:')"
            type="checkbox"
            class="arkime-check-input"
            data-testid="checkbox"
            v-model="item.webEnabled"
            @change="userHasChanged(item)">
        </template>
        <template #[`item.headerAuthEnabled`]="{ item }">
          <input
            v-if="!item.userId.startsWith('role:')"
            type="checkbox"
            class="arkime-check-input"
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
          <div class="float-right">
            <v-btn
              v-if="parentApp === 'Arkime' && isUser(item)"
              v-has-role="{user:currentUser,roles:'arkimeAdmin'}"
              color="primary"
              variant="flat"
              class="ms-1"
              icon="mdi:mdi-cog"
              @click="openSettings(item.userId)"
              :title="$t('users.settingsFor', {user: item.userId})" />
            <v-btn
              v-if="parentApp === 'Arkime'"
              color="secondary"
              variant="flat"
              class="ms-1"
              icon="mdi:mdi-history"
              @click="openHistory(item.userId)"
              :title="$t('users.historyFor', {user: item.userId})" />
            <transition name="buttons">
              <v-btn
                v-if="confirmDelete[item.userId]"
                color="warning"
                variant="flat"
                class="ms-1"
                icon="mdi:mdi-cancel"
                :title="$t('common.cancel')"
                @click="toggleConfirmDeleteUser(item.userId)" />
            </transition>
            <transition name="buttons">
              <v-btn
                v-if="confirmDelete[item.userId]"
                color="error"
                variant="flat"
                class="ms-1"
                icon="mdi:mdi-check"
                :title="$t('common.areYouSure')"
                @click="deleteUser(item, index)" />
            </transition>
            <transition name="buttons">
              <v-btn
                v-if="!confirmDelete[item.userId]"
                color="error"
                variant="flat"
                class="ms-1"
                icon="mdi:mdi-delete"
                :title="$t('users.deleteUser', {user: item.userId})"
                @click="toggleConfirmDeleteUser(item.userId)" />
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

                <v-text-field
                  class="mt-2"
                  :label="$t('users.forcedExpression')"
                  v-model="item.expression"
                  @update:model-value="userHasChanged(item)">
                  <template #append-inner>
                    <span
                      :id="`${item.userId}-expression`"
                      class="mdi mdi-information cursor-help" />
                    <v-tooltip :activator="`[id='${item.userId}-expression']`">
                      {{ $t('users.forcedExpressionTip') }}
                    </v-tooltip>
                  </template>
                </v-text-field>

                <v-select
                  class="mt-1 w-25"
                  item-title="text"
                  item-value="value"
                  :items="timeLimitOptions"
                  :label="$t('users.queryTimeLimit')"
                  v-model="item.timeLimit"
                  @update:model-value="changeTimeLimit(item)">
                  <template #append-inner>
                    <span
                      :id="`${item.userId}-timeLimit`"
                      class="mdi mdi-information cursor-help" />
                    <v-tooltip :activator="`[id='${item.userId}-timeLimit']`">
                      {{ $t('users.queryTimeLimitTip') }}
                    </v-tooltip>
                  </template>
                </v-select>

                <!-- password change for users / role-permissions for roles -->
                <template v-if="parentApp === 'Cont3xt' || parentApp === 'Arkime'">
                  <form
                    class="row"
                    v-if="isUser(item)">
                    <div class="col-9 mt-2">
                      <v-text-field
                        class="mt-1"
                        type="password"
                        :label="$t('users.newPassword')"
                        v-model="newPassword"
                        autocomplete="new-password"
                        @keydown.enter="changePassword(item.userId)"
                        :placeholder="$t('users.newPasswordPlaceholder')" />
                      <v-text-field
                        class="mt-1"
                        type="password"
                        :label="$t('users.confirmPassword')"
                        autocomplete="new-password"
                        v-model="confirmNewPassword"
                        @keydown.enter="changePassword(item.userId)"
                        :placeholder="$t('users.confirmPasswordPlaceholder')" />
                      <v-btn
                        size="large"
                        color="success"
                        variant="flat"
                        class="mt-2"
                        @click="changePassword(item.userId)">
                        {{ $t('users.changePassword') }}
                      </v-btn>
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

      <!-- Hover tooltips for header help text. Default Vuetify headers
           render title + sort icon; we bind tooltips by id selector via
           each column's headerProps so we don't have to override the
           header slot (which would replace the default sort UI). -->
      <v-tooltip
        activator="#users-header-userId"
        location="top">
        {{ $t('users.userIdTip') }}
      </v-tooltip>
      <v-tooltip
        activator="#users-header-userName"
        location="top">
        {{ $t('users.userNameTip') }}
      </v-tooltip>
      <v-tooltip
        activator="#users-header-enabled"
        location="top">
        {{ $t('users.enabledTip') }}
      </v-tooltip>
      <v-tooltip
        activator="#users-header-webEnabled"
        location="top">
        {{ $t('users.webEnabledTip') }}
      </v-tooltip>
      <v-tooltip
        activator="#users-header-headerAuthEnabled"
        location="top">
        {{ $t('users.headerAuthEnabledTip') }}
      </v-tooltip>
      <v-tooltip
        activator="#users-header-roles"
        location="top">
        {{ $t('users.rolesTip') }}
      </v-tooltip>
      <v-tooltip
        activator="#users-header-lastUsed"
        location="top">
        {{ $t('users.lastUsedTip') }}
      </v-tooltip>
    </div> <!-- /users table -->

    <!-- create user -->
    <UserCreate
      :show-modal="showUserCreateModal"
      :roles="createMode === 'user' ? roles : roleAssignableRoles"
      :create-mode="createMode"
      @user-created="userCreated"
      @close="showUserCreateModal = false" />

    <!-- messages (success/error) displayed at bottom of page -->
    <v-snackbar
      :model-value="!!msg"
      @update:model-value="(val) => { if (!val) msg = ''; }"
      :color="snackbarColor"
      location="bottom"
      timeout="-1"
      variant="flat">
      {{ msg }}
      <template #actions>
        <v-btn
          variant="text"
          icon="$close"
          @click="msg = ''" />
      </template>
    </v-snackbar> <!-- /messages -->
  </div>
</template>

<script>
import Focus from './Focus.vue';
import HasRole from './HasRole.vue';
import UserCreate from './UserCreate.vue';
import UserService from './UserService.js';
import RoleDropdown from './RoleDropdown.vue';
import UserDropdown from './UserDropdown.vue';
import TriStateToggle from './TriStateToggle.vue';
import ToggleBtn from './ToggleBtn.vue';
import ArkimePaging from './Pagination.vue';
import { timezoneDateString, commaString } from './vueFilters.js';
import { resolveMessage } from './resolveI18nMessage';

let userChangeTimeout;

export default {
  name: 'UsersCommon',
  directives: { HasRole, Focus },
  components: {
    UserCreate,
    RoleDropdown,
    UserDropdown,
    TriStateToggle,
    ToggleBtn,
    ArkimePaging
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
      recordsFiltered: 0,
      // managed by the ArkimePaging component (length defaults to 100 via
      // its length-default prop; start updates on page changes).
      paging: { start: 0, length: 100 },
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
        headerProps: { id: `users-header-${key}` },
        ...opts
      });
      return [
        // Explicit expand-toggle column at position 0; v-data-table's
        // show-expand auto-injects it at the END otherwise.
        { title: '', key: 'data-table-expand', sortable: false, width: '36px' },
        mk('userId'),
        mk('userName', { width: '250px' }),
        mk('enabled'),
        mk('webEnabled'),
        mk('headerAuthEnabled'),
        mk('roles', { sortable: false }),
        mk('lastUsed'),
        { title: '', key: 'action', sortable: false, width: '230px', align: 'end' }
      ];
    },
    timeLimitOptions () {
      return [
        { value: '1', text: this.$t('common.hourCount', { count: 1 }) },
        { value: '6', text: this.$t('common.hourCount', { count: 6 }) },
        { value: '24', text: this.$t('common.hourCount', { count: 24 }) },
        { value: '48', text: this.$t('common.hourCount', { count: 48 }) },
        { value: '72', text: this.$t('common.hourCount', { count: 72 }) },
        { value: '168', text: this.$t('common.weekCount', { count: 1 }) },
        { value: '336', text: this.$t('common.weekCount', { count: 2 }) },
        { value: '720', text: this.$t('common.monthCount', { count: 1 }) },
        { value: '1440', text: this.$t('common.monthCount', { count: 2 }) },
        { value: '4380', text: this.$t('common.monthCount', { count: 6 }) },
        { value: '8760', text: this.$t('common.yearCount', { count: 1 }) },
        { value: 'undefined', text: this.$t('common.allCareful') }
      ];
    },
    /* Map Bootstrap alert variants to Vuetify color tokens. msgType is set
       via showMessage() callers using strings like 'danger'/'success' that
       date back to the BVN/bootstrap era. */
    snackbarColor () {
      const map = { danger: 'error', success: 'success', warning: 'warning', info: 'info' };
      return map[this.msgType] || 'info';
    }
  },
  created () {
    this.loadUsers();
  },
  watch: {
    searchTerm () {
      this.loadUsers();
    }
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    commaString,
    tzDateStr (date, tz, ms) {
      return timezoneDateString(date, tz, ms);
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
        length: this.paging.length,
        filter: this.searchTerm,
        sortField: this.sortBy[0]?.key,
        start: this.paging.start
      };
    },
    onPagingChange (params) {
      this.paging = { start: params.start, length: params.length };
      this.loadUsers();
    },
    loadUsers () {
      const query = this.getUsersQuery();

      UserService.searchUsers(query).then((response) => {
        this.error = '';
        this.loading = false;
        this.recordsTotal = response.recordsTotal;
        this.recordsFiltered = response.recordsFiltered;
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
        this.recordsFiltered = response.recordsFiltered;
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
  background: linear-gradient(135deg, rgb(var(--v-theme-primary)) 1%, rgb(var(--v-theme-primary)) 75%, rgb(var(--v-theme-primary)) 75%, rgb(var(--v-theme-primary-lighter)) 77%, rgb(var(--v-theme-primary-lighter)) 100%);
}

/* shrink the RoleDropdown / UserDropdown trigger text -- when a user
   has many selected roles the comma-joined list overflows the cell at
   the default size="large" v-btn font (~14px). */
.roles-dropdown.v-btn,
.users-dropdown.v-btn,
.notifier-trigger.v-btn {
  font-size: 0.75rem !important;
  line-height: 1.2 !important;
}
.roles-dropdown.v-btn .v-btn__content,
.users-dropdown.v-btn .v-btn__content {
  white-space: normal;
  text-align: start;
}

.small-table-font {
  font-size: 0.9rem;
}

.toggle-group {
  background-color: rgb(var(--v-theme-white));
  color: rgb(var(--v-theme-neutral-dark));
}

.pagination-no-right-radius :deep(.page-item:last-child .page-link) {
  border-top-right-radius: 0;
  border-bottom-right-radius: 0;
}

/* ---- Users table: tight rows so the list fits more on screen.
   Padding is below Vuetify's compact default (8px); font shrunk to
   match the rest of the analyst-grade dense chrome. ---- */
.users-table-striped :deep(tbody tr:nth-of-type(odd) > td) {
  background-color: rgb(var(--v-theme-neutral-lighter)) !important;
}
.users-table-striped :deep(tbody tr > td),
.users-table-striped :deep(thead tr > th) {
  padding-top: 1px !important;
  padding-bottom: 1px !important;
  padding-left: 6px !important;
  padding-right: 6px !important;
  height: auto !important;
  font-size: 0.8rem !important;
  vertical-align: middle;
}
.users-table-striped :deep(thead tr > th) {
  font-size: 0.75rem !important;
  text-transform: uppercase;
  letter-spacing: 0.03em;
  text-align: left;
}
.users-table-striped :deep(tbody tr > td .v-input),
.users-table-striped :deep(tbody tr > td .arkime-input-group) {
  margin-top: 0 !important;
  margin-bottom: 0 !important;
}
/* per-row icon-only v-btns: keep the button square chrome but shrink
   so the rows aren't blown out by the action column. */
.users-table-striped :deep(tbody tr > td .v-btn--icon) {
  width: 28px !important;
  height: 28px !important;
}
.users-table-striped :deep(.user-detail-row > td) {
  padding-top: 6px !important;
  padding-bottom: 6px !important;
}
</style>
