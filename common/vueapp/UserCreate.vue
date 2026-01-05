<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <b-modal
    size="xl"
    id="create-user-modal"
    @hidden="$emit('close')"
    :model-value="showModal"
    :title="createMode === 'user' ? $t('users.createNewUser') : $t('users.createNewRole')">
    <!-- create form -->
    <b-form>
      <div class="row">
        <b-input-group
          size="sm"
          class="col-md-6 mt-2">
          <template #prepend>
            <b-input-group-text>
              {{ $t('users.userId') }}
              <sup>*</sup>
            </b-input-group-text>
          </template>
          <b-form-input
            autofocus
            autocomplete="userid"
            :placeholder="$t('users.userIdPlaceholder')"
            v-model.lazy="newUser.userId"
            :state="newUser.userId.length > 0" />
        </b-input-group>
        <b-input-group
          size="sm"
          class="col-md-6 mt-2">
          <template #prepend>
            <b-input-group-text>
              {{ $t('users.userName') }}
              <sup>*</sup>
            </b-input-group-text>
          </template>
          <b-form-input
            autocomplete="username"
            :placeholder="$t('users.userNamePlaceholder')"
            v-model.lazy="newUser.userName"
            :state="newUser.userName.length > 0" />
        </b-input-group>
      </div>
      <b-input-group
        size="sm"
        class="mt-2">
        <template #prepend>
          <b-input-group-text
            id="create-user-expression"
            class="cursor-help">
            {{ $t('users.forcedExpression') }}
            <BTooltip target="create-user-expression">
              {{ $t('users.forcedExpressionTip') }}
            </BTooltip>
          </b-input-group-text>
        </template>
        <b-form-input
          autocomplete="expression"
          :placeholder="$t('users.forcedExpressionPlaceholder')"
          v-model.lazy="newUser.expression" />
      </b-input-group>
      <div class="row">
        <div class="col-md-6">
          <b-input-group
            size="sm"
            class="mt-2">
            <template #prepend>
              <b-input-group-text
                id="create-user-time-limit"
                class="cursor-help">
                {{ $t('users.queryTimeLimit') }}
                <BTooltip target="create-user-time-limit">
                  {{ $t('users.queryTimeLimitTip') }}
                </BTooltip>
              </b-input-group-text>
            </template>
            <!-- NOTE: can't use b-form-select because it doesn't allow for undefined v-models -->
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
          </b-input-group>
        </div>
        <div class="col-md-6 mt-2 d-inline-flex align-items-center">
          <template v-if="roles">
            <RoleDropdown
              :roles="roles"
              :display-text="$t('users.roles')"
              @selected-roles-updated="updateNewUserRoles" />
            <span
              id="create-user-roles"
              class="fa fa-info-circle fa-lg cursor-help ms-2">
              <BTooltip target="create-user-roles">
                {{ $t('users.rolesTip') }}
              </BTooltip>
            </span>
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
              class="fa fa-info-circle fa-lg cursor-help ms-2">
              <BTooltip target="create-user-role-assigners">
                {{ $t('users.roleAssignersTip') }}
              </BTooltip>
            </span>
          </template>
        </div>
      </div>
      <b-input-group
        size="sm"
        class="mb-2 mt-2"
        v-if="createMode === 'user'">
        <template #prepend>
          <b-input-group-text>
            {{ $t('users.password') }}<sup>*</sup>
          </b-input-group-text>
        </template>
        <b-form-input
          type="password"
          :state="validatePassword"
          :placeholder="$t('users.passwordPlaceholder')"
          autocomplete="new-password"
          v-model.lazy="newUser.password" />
      </b-input-group>
      <b-form-checkbox
        inline
        v-model="newUser.enabled">
        {{ $t('users.enabled') }}
      </b-form-checkbox>
      <b-form-checkbox
        inline
        v-if="createMode === 'user'"
        v-model="newUser.webEnabled">
        {{ $t('users.webEnabled') }}
      </b-form-checkbox>
      <b-form-checkbox
        inline
        v-if="createMode === 'user'"
        v-model="newUser.headerAuthEnabled">
        {{ $t('users.headerAuthEnabled') }}
      </b-form-checkbox>

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
    </b-form> <!-- /create form -->
    <!-- create form error -->
    <div
      v-if="createError"
      class="alert alert-danger mt-2 mb-0">
      {{ createError }}
    </div> <!-- /create form error -->
    <!-- modal footer -->
    <template #footer>
      <div class="w-100 d-flex justify-content-between">
        <b-button
          :title="$t('common.cancel')"
          variant="danger"
          @click="$emit('close')">
          <span class="fa fa-times" />
          {{ $t('common.cancel') }}
        </b-button>
        <div>
          <BButton
            variant="primary"
            @click="createUser(createMode === 'role')">
            <span class="fa fa-plus-circle me-1" />
            {{ $t('common.create') }}
          </BButton>
        </div>
      </div>
    </template> <!-- /modal footer -->
  </b-modal>
</template>

<script>
import UserService from './UserService';
import RoleDropdown from './RoleDropdown.vue';
import UserDropdown from './UserDropdown.vue';
import TriStateToggle from './TriStateToggle.vue';

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
        this.$emit('user-created', response.text, user);
      }).catch((error) => {
        this.createError = error.text;
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
