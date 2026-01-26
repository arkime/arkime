<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <!-- search -->
    <div class="d-flex justify-content-between mt-3">
      <div class="me-2 flex-grow-1 ">
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
            :placeholder="$t('users.searchPlaceholder')" />
          <template #append>
            <b-button
              :disabled="!searchTerm"
              @click="searchTerm = ''"
              variant="outline-secondary">
              <span class="fa fa-close" />
            </b-button>
          </template>
        </b-input-group>
      </div>
      <div class="me-2">
        <b-form-select
          size="sm"
          v-model="perPage"
          @update:model-value="perPageChange"
          :options="[
            { value: 50, text: $t('common.perPage', {count: 50})},
            { value: 100, text: $t('common.perPage', {count: 100})},
            { value: 200, text: $t('common.perPage', {count: 200})},
            { value: 500, text: $t('common.perPage', {count: 500})}
          ]" />
      </div>
      <div>
        <b-pagination
          size="sm"
          :per-page="perPage"
          v-model="currentPage"
          :total-rows="recordsTotal" />
      </div>
      <div>
        <b-button
          size="sm"
          class="ms-2"
          @click="download"
          variant="primary"
          :title="$t('users.downloadCSVTip')">
          <span class="fa fa-download" />
        </b-button>
      </div>
    </div> <!-- /search -->

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
    <div v-if="!error">
      <BTable
        :dark="dark"
        small
        striped
        show-empty
        no-local-sorting
        :items="users"
        :fields="fields"
        @sorted="sortChanged"
        class="small-table-font"
        :empty-text="searchTerm ? $t('users.noUsersOrRolesMatch') : $t('users.noUsersOrRoles')">
        <!-- column headers -->
        <template #head()="data">
          <span :title="data.field.help">
            {{ data.label }}
            <span
              id="roles-help"
              v-if="data.field.key === 'roles'"
              class="fa fa-info-circle fa-lg cursor-help ms-2">
              <BTooltip target="roles-help">
                {{ $t('users.rolesTip') }}
              </BTooltip>
            </span>
            <div
              class="pull-right"
              v-if="data.field.key === 'action'">
              <b-button
                size="sm"
                v-if="roles"
                variant="success"
                :title="$t('users.createRoleTip')"
                @click="createMode = 'role'; showUserCreateModal = true">
                <span class="fa fa-plus-circle me-1" />
                {{ $t('common.role') }}
              </b-button>
              <b-button
                size="sm"
                class="ms-2"
                variant="primary"
                :title="$t('users.createUserTip')"
                @click="createMode = 'user'; showUserCreateModal = true">
                <span class="fa fa-plus-circle me-1" />
                {{ $t('common.user') }}
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
              :title="!data.item.emailSearch || !data.item.removeEnabled || !data.item.packetSearch || data.item.hideStats || data.item.hideFiles || data.item.hidePcap || data.item.disablePcapDownload || data.item.timeLimit || data.item.expression ? $t('users.restrictedTip') : ''" />
          </span>
        </template> <!-- /toggle column -->
        <!-- action column -->
        <template #cell(action)="data">
          <div class="pull-right">
            <b-button
              size="sm"
              class="ms-1"
              variant="primary"
              @click="openSettings(data.item.userId)"
              :title="$t('users.settingsFor', {user: data.item.userId})"
              v-has-role="{user:currentUser,roles:'arkimeAdmin'}"
              v-if="parentApp === 'Arkime' && isUser(data.item)">
              <span class="fa fa-gear" />
            </b-button>
            <b-button
              size="sm"
              class="ms-1"
              variant="secondary"
              v-if="parentApp === 'Arkime'"
              @click="openHistory(data.item.userId)"
              :title="$t('users.historyFor', {user: data.item.userId})">
              <span class="fa fa-history" />
            </b-button>
            <!-- cancel confirm delete button -->
            <transition name="buttons">
              <b-button
                size="sm"
                class="ms-1"
                :title="$t('common.cancel')"
                variant="warning"
                v-if="confirmDelete[data.item.userId]"
                @click="toggleConfirmDeleteUser(data.item.userId)">
                <span class="fa fa-ban" />
              </b-button>
            </transition> <!-- /cancel confirm delete button -->
            <!-- confirm delete button -->
            <transition name="buttons">
              <b-button
                size="sm"
                class="ms-1"
                variant="danger"
                :title="$t('common.areYouSure')"
                v-if="confirmDelete[data.item.userId]"
                @click="deleteUser(data.item, data.index)">
                <span class="fa fa-check" />
              </b-button>
            </transition> <!-- /confirm delete button -->
            <!-- delete button -->
            <transition name="buttons">
              <b-button
                size="sm"
                class="ms-1"
                variant="danger"
                :title="$t('users.deleteUser', {user: data.item.userId})"
                v-if="!confirmDelete[data.item.userId]"
                @click="toggleConfirmDeleteUser(data.item.userId)">
                <span class="fa fa-trash-o" />
              </b-button>
            </transition> <!-- /delete button -->
          </div>
        </template> <!-- /action column -->
        <!-- user id column -->
        <template #cell(userId)="data">
          <div class="mt-1">
            {{ data.value }}
          </div>
        </template> <!-- /user id column -->
        <!-- last used column -->
        <template #cell(lastUsed)="data">
          <div class="mt-1">
            {{ data.value ? (tzDateStr(data.value, currentUser.settings.timezone || 'local', currentUser.settings.ms)) : $t('common.never') }}
          </div>
        </template> <!-- /last used column -->
        <!-- roles column -->
        <template #cell(roles)="data">
          <RoleDropdown
            v-if="data.field.type === 'select' && roles && roles.length"
            :roles="isUser(data.item) ? roles : roleAssignableRoles"
            :id="data.item.userId"
            :selected-roles="data.item.roles"
            @selected-roles-updated="updateRoles"
            :truncate="4" />
        </template> <!-- /roles column -->
        <!-- all other columns -->
        <template #cell()="data">
          <b-form-input
            size="sm"
            v-model="data.item[data.field.key]"
            v-if="data.field.type === 'text'"
            @input="userHasChanged(data.item)" />
          <b-form-checkbox
            class="mt-1"
            data-testid="checkbox"
            v-model="data.item[data.field.key]"
            v-else-if="data.field.type === 'checkbox'"
            @input="userHasChanged(data.item)" />
          <b-form-checkbox
            class="mt-1"
            data-testid="checkbox"
            v-model="data.item[data.field.key]"
            v-else-if="data.field.type === 'checkbox-notrole' && !data.item.userId.startsWith('role:')"
            @input="userHasChanged(data.item)" />
        </template> <!-- all other columns -->

        <!-- detail row -->
        <template #row-details="data">
          <div class="m-2">
            <!-- User permission tri-state toggles -->
            <div
              v-if="isUser(data.item)"
              class="user-permissions mt-2 mb-2 d-flex flex-wrap gap-1">
              <TriStateToggle
                class="toggle-group rounded p-1"
                :model-value="data.item.emailSearch"
                :label="$t('users.disableEmailSearch')"
                :negated="true"
                @update:model-value="setRoleField(data.item, 'emailSearch', $event)" />
              <TriStateToggle
                class="toggle-group rounded p-1"
                :model-value="data.item.removeEnabled"
                :label="$t('users.disableDataRemoval')"
                :negated="true"
                @update:model-value="setRoleField(data.item, 'removeEnabled', $event)" />
              <TriStateToggle
                class="toggle-group rounded p-1"
                :model-value="data.item.packetSearch"
                :label="$t('users.disableHunting')"
                :negated="true"
                @update:model-value="setRoleField(data.item, 'packetSearch', $event)" />
              <TriStateToggle
                class="toggle-group rounded p-1"
                :model-value="data.item.hideStats"
                :label="$t('users.hideStatsPage')"
                @update:model-value="setRoleField(data.item, 'hideStats', $event)" />
              <TriStateToggle
                class="toggle-group rounded p-1"
                :model-value="data.item.hideFiles"
                :label="$t('users.hideFilesPage')"
                @update:model-value="setRoleField(data.item, 'hideFiles', $event)" />
              <TriStateToggle
                class="toggle-group rounded p-1"
                :model-value="data.item.hidePcap"
                :label="$t('users.hidePcap')"
                @update:model-value="setRoleField(data.item, 'hidePcap', $event)" />
              <TriStateToggle
                class="toggle-group rounded p-1"
                :model-value="data.item.disablePcapDownload"
                :label="$t('users.disablePcapDownload')"
                @update:model-value="setRoleField(data.item, 'disablePcapDownload', $event)" />
            </div>
            <b-input-group
              size="sm"
              class="mt-2">
              <template #prepend>
                <b-input-group-text :id="data.id + '-expression'">
                  {{ $t('users.forcedExpression') }}
                  <BTooltip :target="data.id + '-expression'">
                    {{ $t('users.forcedExpressionTip') }}
                  </BTooltip>
                </b-input-group-text>
              </template>
              <b-form-input
                v-model="data.item.expression"
                @input="userHasChanged(data.item)" />
            </b-input-group>
            <b-input-group
              size="sm"
              class="mt-2 w-25">
              <template #prepend>
                <b-input-group-text :id="data.id + '-timeLimit'">
                  {{ $t('users.queryTimeLimit') }}
                  <BTooltip :target="data.id + '-timeLimit'">
                    {{ $t('users.queryTimeLimitTip') }}
                  </BTooltip>
                </b-input-group-text>
              </template>
              <!-- NOTE: can't use b-form-select because it doesn't allow for undefined v-models -->
              <select
                class="form-control"
                v-model="data.item.timeLimit"
                @change="changeTimeLimit(data.item)">
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
            </b-input-group>

            <!-- display change password if not a role and
                 we're in cont3xt or arkime
                 (assumes user is a usersAdmin since only usersAdmin can see this page) -->
            <template v-if="parentApp === 'Cont3xt' || parentApp === 'Arkime'">
              <form
                class="row"
                v-if="isUser(data.item)">
                <div class="col-9 mt-4">
                  <!-- new password -->
                  <b-input-group
                    size="sm"
                    class="mt-2"
                    :prepend="$t('users.newPassword')">
                    <b-form-input
                      type="password"
                      v-model="newPassword"
                      autocomplete="new-password"
                      @keydown.enter="changePassword"
                      :placeholder="$t('users.newPasswordPlaceholder')" />
                  </b-input-group>
                  <!-- confirm new password -->
                  <b-input-group
                    size="sm"
                    class="mt-2"
                    :prepend="$t('users.confirmPassword')">
                    <b-form-input
                      type="password"
                      autocomplete="new-password"
                      v-model="confirmNewPassword"
                      @keydown.enter="changePassword"
                      :placeholder="$t('users.confirmPasswordPlaceholder')" />
                  </b-input-group>
                  <!-- change password button -->
                  <b-button
                    size="sm"
                    class="mt-2"
                    variant="success"
                    @click="changePassword(data.item.userId)">
                    {{ $t('users.changePassword') }}
                  </b-button>
                </div>
              </form>
              <div v-else>
                <!-- Role permission tri-state toggles -->
                <div class="role-permissions mt-2 mb-2 d-flex flex-wrap gap-1">
                  <TriStateToggle
                    class="toggle-group rounded p-1"
                    :model-value="data.item.emailSearch"
                    :label="$t('users.disableEmailSearch')"
                    :negated="true"
                    @update:model-value="setRoleField(data.item, 'emailSearch', $event)" />
                  <TriStateToggle
                    class="toggle-group rounded p-1"
                    :model-value="data.item.removeEnabled"
                    :label="$t('users.disableDataRemoval')"
                    :negated="true"
                    @update:model-value="setRoleField(data.item, 'removeEnabled', $event)" />
                  <TriStateToggle
                    class="toggle-group rounded p-1"
                    :model-value="data.item.packetSearch"
                    :label="$t('users.disableHunting')"
                    :negated="true"
                    @update:model-value="setRoleField(data.item, 'packetSearch', $event)" />
                  <TriStateToggle
                    class="toggle-group rounded p-1"
                    :model-value="data.item.hideStats"
                    :label="$t('users.hideStatsPage')"
                    @update:model-value="setRoleField(data.item, 'hideStats', $event)" />
                  <TriStateToggle
                    class="toggle-group rounded p-1"
                    :model-value="data.item.hideFiles"
                    :label="$t('users.hideFilesPage')"
                    @update:model-value="setRoleField(data.item, 'hideFiles', $event)" />
                  <TriStateToggle
                    class="toggle-group rounded p-1"
                    :model-value="data.item.hidePcap"
                    :label="$t('users.hidePcap')"
                    @update:model-value="setRoleField(data.item, 'hidePcap', $event)" />
                  <TriStateToggle
                    class="toggle-group rounded p-1"
                    :model-value="data.item.disablePcapDownload"
                    :label="$t('users.disablePcapDownload')"
                    @update:model-value="setRoleField(data.item, 'disablePcapDownload', $event)" />
                </div>
                <UserDropdown
                  class="mt-2"
                  label="Role Assigners&nbsp;"
                  :selected-users="data.item.roleAssigners || []"
                  :role-id="data.item.userId"
                  @selected-users-updated="updateRoleAssigners" />
              </div>
            </template>
          </div>
        </template> <!-- /detail row -->
      </BTable>
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
import ToggleBtn from './ToggleBtn.vue';
import UserCreate from './UserCreate.vue';
import UserService from './UserService.js';
import RoleDropdown from './RoleDropdown.vue';
import UserDropdown from './UserDropdown.vue';
import TriStateToggle from './TriStateToggle.vue';
import { timezoneDateString } from './vueFilters.js';

let userChangeTimeout;

export default {
  name: 'UsersCommon',
  directives: { HasRole },
  components: {
    ToggleBtn,
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
      sortField: 'userId',
      desc: false,
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
    fields () {
      const $t = this.$t;
      function mkRow (row) {
        const key = 'users.' + row.key;
        row.label = $t(key);
        row.help = $t(key + 'Tip');
        return row;
      }
      return [
        { label: '', key: 'toggle', sortable: false },
        mkRow({ key: 'userId', sortable: true, required: true }),
        mkRow({ key: 'userName', sortable: true, type: 'text', required: true, thStyle: 'width:250px;' }),
        mkRow({ key: 'enabled', sortable: true, type: 'checkbox' }),
        mkRow({ key: 'webEnabled', sortable: true, type: 'checkbox-notrole' }),
        mkRow({ key: 'headerAuthEnabled', sortable: true, type: 'checkbox-notrole' }),
        mkRow({ key: 'roles', sortable: false, type: 'select' }),
        mkRow({ key: 'lastUsed', sortable: true, type: 'checkbox' }),
        { label: '', key: 'action', sortable: false, thStyle: 'width:190px;' }
      ];
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
    tzDateStr (date, tz, ms) {
      return timezoneDateString(date, tz, ms);
    },
    perPageChange (newVal) {
      this.perPage = newVal;
      this.loadUsers(false);
    },
    sortChanged (newSort) {
      this.sortField = newSort.key;
      this.desc = newSort.order === 'desc';
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
        this.showMessage({ variant: 'success', message: response.text });

        const oldUser = this.dbUserList.find(u => u.userId === user.userId);
        const currentUserRoleAssignmentChanged =
            user.roleAssigners?.includes(this.currentUser.userId) !== oldUser.roleAssigners?.includes(this.currentUser.userId);

        // update the current user if they were changed or if their assignableRoles should be changed
        if (this.currentUser.userId === user.userId || currentUserRoleAssignmentChanged) {
          this.emitCurrentUserUpdate();
        }
      }).catch((error) => {
        this.showMessage({ variant: 'danger', message: error.text });
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
        this.showMessage({ variant: 'danger', message: error.text });
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
        this.showMessage({ variant: 'success', message: response.text || this.$t('users.changedPasswordMsg') });
      }).catch((error) => {
        // display error message to user
        this.showMessage({ variant: 'danger', message: error.text || error });
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
        this.showMessage({ variant: 'success', message: response.text || this.$t('users.downloadCSVMsg') });
      }).catch((error) => {
        // display error message to user
        this.showMessage({ variant: 'danger', message: error.text || error });
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
        desc: this.desc,
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
        this.recordsTotal = response.recordsTotal;
        this.users = JSON.parse(JSON.stringify(response.data));
        // don't modify original list - used for comparing
        this.dbUserList = response.data;
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
</style>
