<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-dialog
    :model-value="showModal"
    @update:model-value="(val) => { if (!val) $emit('close'); }"
    max-width="1140">
    <v-card density="compact">
      <v-card-title>
        {{ createMode === 'user' ? $t('users.createNewUser') : $t('users.createNewRole') }}
      </v-card-title>
      <v-card-text>
        <!-- create form -->
        <form>
          <div class="row">
            <div class="col-md-6 mt-2">
              <div class="input-group input-group-sm">
                <span class="input-group-text">
                  {{ $t('users.userId') }}<sup>*</sup>
                </span>
                <input
                  type="text"
                  class="form-control"
                  autofocus
                  autocomplete="userid"
                  :placeholder="$t('users.userIdPlaceholder')"
                  v-model.lazy="newUser.userId">
              </div>
            </div>
            <div class="col-md-6 mt-2">
              <div class="input-group input-group-sm">
                <span class="input-group-text">
                  {{ $t('users.userName') }}<sup>*</sup>
                </span>
                <input
                  type="text"
                  class="form-control"
                  autocomplete="username"
                  :placeholder="$t('users.userNamePlaceholder')"
                  v-model.lazy="newUser.userName">
              </div>
            </div>
          </div>

          <div class="input-group input-group-sm mt-2">
            <span
              id="create-user-expression"
              class="input-group-text cursor-help">
              {{ $t('users.forcedExpression') }}
              <v-tooltip activator="#create-user-expression">
                {{ $t('users.forcedExpressionTip') }}
              </v-tooltip>
            </span>
            <input
              type="text"
              class="form-control"
              autocomplete="expression"
              :placeholder="$t('users.forcedExpressionPlaceholder')"
              v-model.lazy="newUser.expression">
          </div>

          <div class="row">
            <div class="col-md-6">
              <div class="input-group input-group-sm mt-2">
                <span
                  id="create-user-time-limit"
                  class="input-group-text cursor-help">
                  {{ $t('users.queryTimeLimit') }}
                  <v-tooltip activator="#create-user-time-limit">
                    {{ $t('users.queryTimeLimitTip') }}
                  </v-tooltip>
                </span>
                <select
                  class="form-control"
                  v-model.lazy="newUser.timeLimit">
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
            </div>
            <div class="col-md-6 mt-2 d-inline-flex align-items-center">
              <template v-if="roles">
                <RoleDropdown
                  :roles="roles"
                  :display-text="$t('users.roles')"
                  @selected-roles-updated="updateNewUserRoles" />
                <span
                  id="create-user-roles"
                  class="fa fa-info-circle fa-lg cursor-help ms-2" />
                <v-tooltip activator="#create-user-roles">
                  {{ $t('users.rolesTip') }}
                </v-tooltip>
              </template>
              <template v-if="createMode === 'role'">
                <UserDropdown
                  class="ms-3"
                  :display-text="$t('users.roleAssigners')"
                  :selected-users="newUser.roleAssigners"
                  @selected-users-updated="updateNewRoleAssigners">
                  {{ $t('users.roleAssigners') }}
                </UserDropdown>
                <span
                  id="create-user-role-assigners"
                  class="fa fa-info-circle fa-lg cursor-help ms-2" />
                <v-tooltip activator="#create-user-role-assigners">
                  {{ $t('users.roleAssignersTip') }}
                </v-tooltip>
              </template>
            </div>
          </div>

          <div
            v-if="createMode === 'user'"
            class="input-group input-group-sm mt-2 mb-2">
            <span class="input-group-text">
              {{ $t('users.password') }}<sup>*</sup>
            </span>
            <input
              type="password"
              class="form-control"
              :class="{'is-invalid': validatePassword === false}"
              :placeholder="$t('users.passwordPlaceholder')"
              autocomplete="new-password"
              v-model.lazy="newUser.password">
          </div>

          <div class="d-inline-flex align-items-center gap-3 mt-2">
            <label class="form-check form-check-inline mb-0">
              <input
                type="checkbox"
                class="form-check-input"
                v-model="newUser.enabled">
              <span class="form-check-label">{{ $t('users.enabled') }}</span>
            </label>
            <label
              v-if="createMode === 'user'"
              class="form-check form-check-inline mb-0">
              <input
                type="checkbox"
                class="form-check-input"
                v-model="newUser.webEnabled">
              <span class="form-check-label">{{ $t('users.webEnabled') }}</span>
            </label>
            <label
              v-if="createMode === 'user'"
              class="form-check form-check-inline mb-0">
              <input
                type="checkbox"
                class="form-check-input"
                v-model="newUser.headerAuthEnabled">
              <span class="form-check-label">{{ $t('users.headerAuthEnabled') }}</span>
            </label>
          </div>

          <!-- User permission tri-state toggles -->
          <div
            v-if="createMode === 'user'"
            class="user-permissions mt-2 mb-2 d-flex flex-wrap gap-1">
            <TriStateToggle
              class="toggle-group rounded p-1"
              :model-value="newUser.emailSearch"
              :label="$t('users.disableEmailSearch')"
              :negated="true"
              @update:model-value="setRoleField('emailSearch', $event)" />
            <TriStateToggle
              class="toggle-group rounded p-1"
              :model-value="newUser.removeEnabled"
              :label="$t('users.disableDataRemoval')"
              :negated="true"
              @update:model-value="setRoleField('removeEnabled', $event)" />
            <TriStateToggle
              class="toggle-group rounded p-1"
              :model-value="newUser.packetSearch"
              :label="$t('users.disableHunting')"
              :negated="true"
              @update:model-value="setRoleField('packetSearch', $event)" />
            <TriStateToggle
              class="toggle-group rounded p-1"
              :model-value="newUser.hideStats"
              :label="$t('users.hideStatsPage')"
              @update:model-value="setRoleField('hideStats', $event)" />
            <TriStateToggle
              class="toggle-group rounded p-1"
              :model-value="newUser.hideFiles"
              :label="$t('users.hideFilesPage')"
              @update:model-value="setRoleField('hideFiles', $event)" />
            <TriStateToggle
              class="toggle-group rounded p-1"
              :model-value="newUser.hidePcap"
              :label="$t('users.hidePcap')"
              @update:model-value="setRoleField('hidePcap', $event)" />
            <TriStateToggle
              class="toggle-group rounded p-1"
              :model-value="newUser.disablePcapDownload"
              :label="$t('users.disablePcapDownload')"
              @update:model-value="setRoleField('disablePcapDownload', $event)" />
          </div>

          <!-- Role permission tri-state toggles -->
          <div
            v-if="createMode === 'role'"
            class="role-permissions mt-2 mb-2 d-flex flex-wrap gap-1">
            <TriStateToggle
              class="toggle-group rounded p-1"
              :model-value="newUser.emailSearch"
              :label="$t('users.disableEmailSearch')"
              :negated="true"
              @update:model-value="setRoleField('emailSearch', $event)" />
            <TriStateToggle
              class="toggle-group rounded p-1"
              :model-value="newUser.removeEnabled"
              :label="$t('users.disableDataRemoval')"
              :negated="true"
              @update:model-value="setRoleField('removeEnabled', $event)" />
            <TriStateToggle
              class="toggle-group rounded p-1"
              :model-value="newUser.packetSearch"
              :label="$t('users.disableHunting')"
              :negated="true"
              @update:model-value="setRoleField('packetSearch', $event)" />
            <TriStateToggle
              class="toggle-group rounded p-1"
              :model-value="newUser.hideStats"
              :label="$t('users.hideStatsPage')"
              @update:model-value="setRoleField('hideStats', $event)" />
            <TriStateToggle
              class="toggle-group rounded p-1"
              :model-value="newUser.hideFiles"
              :label="$t('users.hideFilesPage')"
              @update:model-value="setRoleField('hideFiles', $event)" />
            <TriStateToggle
              class="toggle-group rounded p-1"
              :model-value="newUser.hidePcap"
              :label="$t('users.hidePcap')"
              @update:model-value="setRoleField('hidePcap', $event)" />
            <TriStateToggle
              class="toggle-group rounded p-1"
              :model-value="newUser.disablePcapDownload"
              :label="$t('users.disablePcapDownload')"
              @update:model-value="setRoleField('disablePcapDownload', $event)" />
          </div>
        </form> <!-- /create form -->
        <!-- create form error -->
        <div
          v-if="createError"
          class="alert alert-danger mt-2 mb-0">
          {{ createError }}
        </div> <!-- /create form error -->
      </v-card-text>
      <v-card-actions>
        <div class="w-100 d-flex justify-content-between">
          <button
            type="button"
            class="btn btn-danger"
            :title="$t('common.cancel')"
            @click="$emit('close')">
            <span class="fa fa-times" />
            {{ $t('common.cancel') }}
          </button>
          <button
            type="button"
            class="btn btn-primary"
            @click="createUser(createMode === 'role')">
            <span class="fa fa-plus-circle me-1" />
            {{ $t('common.create') }}
          </button>
        </div>
      </v-card-actions>
    </v-card>
  </v-dialog>
</template>

<script>
import UserService from './UserService';
import RoleDropdown from './RoleDropdown.vue';
import UserDropdown from './UserDropdown.vue';
import TriStateToggle from './TriStateToggle.vue';
import { resolveMessage } from './resolveI18nMessage';

const defaultNewUser = {
  userId: '',
  userName: '',
  password: '',
  enabled: true,
  webEnabled: true,
  packetSearch: true,
  emailSearch: false,
  removeEnabled: false,
  roleAssigners: []
};

const defaultNewRole = {
  userId: '',
  userName: '',
  enabled: true,
  roleAssigners: []
};

export default {
  name: 'CreateUser',
  emits: ['close', 'user-created'],
  components: {
    RoleDropdown,
    UserDropdown,
    TriStateToggle
  },
  props: {
    roles: {
      type: Array,
      required: true
    },
    createMode: {
      type: String,
      default: 'user'
    },
    showModal: {
      type: Boolean,
      default: false
    }
  },
  data () {
    return {
      createError: '',
      newUser: { ...defaultNewUser },
      validatePassword: undefined
    };
  },
  watch: {
    showModal (newVal) {
      if (newVal) {
        // Reset to appropriate defaults when modal opens
        this.newUser = this.createMode === 'role'
          ? { ...defaultNewRole }
          : { ...defaultNewUser };
        this.createError = '';
        this.validatePassword = undefined;
      }
    }
  },
  methods: {
    negativeToggle (newVal, user, field) {
      user[field] = !newVal;
    },
    setRoleField (field, value) {
      if (value === undefined) {
        delete this.newUser[field];
      } else {
        this.newUser[field] = value;
      }
    },
    updateNewUserRoles (roles) {
      this.newUser.roles = roles;
    },
    updateNewRoleAssigners ({ newSelection }) {
      this.newUser.roleAssigners = newSelection;
    },
    createUser (createRole) {
      this.createError = '';
      this.validatePassword = undefined;

      if (!this.newUser.userId) {
        this.createError = this.$t('users.userIdEmptyError');
        return;
      }

      if (!this.newUser.userName) {
        this.createError = this.$t('users.userNameEmptyError');
        return;
      }

      if (!createRole && !this.newUser.password) {
        this.validatePassword = false;
        this.createError = this.$t('users.passwordEmptyError');
        return;
      }

      const user = JSON.parse(JSON.stringify(this.newUser));
      if (createRole) {
        if (!user.userId.startsWith('role:')) {
          user.userId = `role:${user.userId}`;
        }
      }

      UserService.createUser(user).then((response) => {
        this.newUser = createRole ? { ...defaultNewRole } : { ...defaultNewUser };
        this.$emit('user-created', resolveMessage(response, this.$t), user);
      }).catch((error) => {
        this.createError = resolveMessage(error, this.$t);
      });
    }
  }
};
</script>

<style scoped>
.toggle-group {
  background-color: var(--color-white);
  color: var(--color-gray-dark);
}
</style>
