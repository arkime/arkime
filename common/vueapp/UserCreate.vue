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
          <v-row dense>
            <v-col
              cols="12"
              md="6">
              <v-text-field
                density="compact"
                variant="outlined"
                hide-details
                autofocus
                autocomplete="userid"
                :label="`${$t('users.userId')} *`"
                :placeholder="$t('users.userIdPlaceholder')"
                v-model.lazy="newUser.userId" />
            </v-col>
            <v-col
              cols="12"
              md="6">
              <v-text-field
                density="compact"
                variant="outlined"
                hide-details
                autocomplete="username"
                :label="`${$t('users.userName')} *`"
                :placeholder="$t('users.userNamePlaceholder')"
                v-model.lazy="newUser.userName" />
            </v-col>
          </v-row>

          <v-text-field
            density="compact"
            variant="outlined"
            hide-details
            class="mt-2"
            autocomplete="expression"
            :label="$t('users.forcedExpression')"
            :placeholder="$t('users.forcedExpressionPlaceholder')"
            v-model.lazy="newUser.expression">
            <template #append-inner>
              <v-icon
                icon="mdi-information"
                class="cursor-help"
                id="create-user-expression" />
              <v-tooltip activator="#create-user-expression">
                {{ $t('users.forcedExpressionTip') }}
              </v-tooltip>
            </template>
          </v-text-field>

          <v-row
            dense
            class="mt-2">
            <v-col
              cols="12"
              md="6">
              <v-select
                density="compact"
                variant="outlined"
                hide-details
                item-title="text"
                item-value="value"
                :items="timeLimitOptions"
                :label="$t('users.queryTimeLimit')"
                v-model.lazy="newUser.timeLimit">
                <template #append-inner>
                  <v-icon
                    icon="mdi-information"
                    class="cursor-help"
                    id="create-user-time-limit" />
                  <v-tooltip activator="#create-user-time-limit">
                    {{ $t('users.queryTimeLimitTip') }}
                  </v-tooltip>
                </template>
              </v-select>
            </v-col>
            <v-col
              cols="12"
              md="6"
              class="d-inline-flex align-center">
              <template v-if="roles">
                <RoleDropdown
                  :roles="roles"
                  :display-text="$t('users.roles')"
                  @selected-roles-updated="updateNewUserRoles" />
                <v-icon
                  icon="mdi-information"
                  size="small"
                  class="cursor-help ms-2"
                  id="create-user-roles" />
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
                <v-icon
                  icon="mdi-information"
                  size="small"
                  class="cursor-help ms-2"
                  id="create-user-role-assigners" />
                <v-tooltip activator="#create-user-role-assigners">
                  {{ $t('users.roleAssignersTip') }}
                </v-tooltip>
              </template>
            </v-col>
          </v-row>

          <v-text-field
            v-if="createMode === 'user'"
            density="compact"
            variant="outlined"
            hide-details
            class="mt-2"
            type="password"
            :error="validatePassword === false"
            :label="`${$t('users.password')} *`"
            :placeholder="$t('users.passwordPlaceholder')"
            autocomplete="new-password"
            v-model.lazy="newUser.password" />

          <div class="d-inline-flex align-center gap-3 mt-2">
            <v-checkbox
              density="compact"
              hide-details
              :label="$t('users.enabled')"
              v-model="newUser.enabled" />
            <v-checkbox
              v-if="createMode === 'user'"
              density="compact"
              hide-details
              :label="$t('users.webEnabled')"
              v-model="newUser.webEnabled" />
            <v-checkbox
              v-if="createMode === 'user'"
              density="compact"
              hide-details
              :label="$t('users.headerAuthEnabled')"
              v-model="newUser.headerAuthEnabled" />
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
        <v-alert
          v-if="createError"
          type="error"
          variant="tonal"
          density="compact"
          class="mt-2 mb-0">
          {{ createError }}
        </v-alert> <!-- /create form error -->
      </v-card-text>
      <v-card-actions>
        <div class="w-100 d-flex justify-space-between">
          <v-btn
            size="large"
            color="error"
            variant="flat"
            :title="$t('common.cancel')"
            @click="$emit('close')">
            <v-icon
              start
              icon="mdi-close" />
            {{ $t('common.cancel') }}
          </v-btn>
          <v-btn
            size="large"
            color="primary"
            variant="flat"
            @click="createUser(createMode === 'role')">
            <v-icon
              start
              icon="mdi-plus-circle" />
            {{ $t('common.create') }}
          </v-btn>
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
  computed: {
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
    }
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
  background-color: rgb(var(--v-theme-white));
  color: rgb(var(--v-theme-neutral-dark));
}
</style>
