<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-inline-flex align-items-center">
    <label
      v-if="label"
      :for="activatorId"
      class="mb-0 me-1">{{ label }}</label>

    <v-menu
      :close-on-content-click="false"
      location="bottom"
      @update:model-value="onMenuToggle">
      <template #activator="{ props: activatorProps }">
        <button
          v-bind="activatorProps"
          type="button"
          class="btn btn-sm btn-outline-secondary users-dropdown no-wrap"
          data-testid="user-dropdown"
          :id="activatorId">
          <slot
            :count="localSelectedUsers.length"
            :filter="searchTerm"
            :unknown="loading || error">
            {{ getUsersStr() }}
          </slot>
          <span class="fa fa-caret-down ms-1" />
          <v-tooltip
            v-if="selectedTooltip"
            :activator="`#${activatorId}`">
            {{ selectedTooltip ? getUsersStr() : '' }}
          </v-tooltip>
        </button>
      </template>

      <v-list
        density="compact"
        class="users-dropdown-menu">
        <!-- search bar -->
        <div class="px-2 py-1">
          <div class="input-group input-group-sm">
            <input
              ref="searchInput"
              type="text"
              class="form-control"
              :value="searchTerm"
              @input="searchTerm = $event.target.value"
              :placeholder="$t('users.searchUserPlaceholder')">
            <button
              type="button"
              class="btn btn-outline-secondary"
              :disabled="!searchTerm"
              @click="clearSearchTerm">
              <span class="fa fa-close" />
            </button>
          </div>
        </div>
        <v-divider />

        <!-- loading -->
        <div
          v-if="loading"
          class="mt-3 text-center">
          <span class="fa fa-circle-o-notch fa-spin fa-2x" />
          <p>{{ $t('common.loading') }}</p>
        </div> <!-- /loading -->

        <!-- error -->
        <div
          v-else-if="error"
          class="mt-3 alert alert-warning">
          <span class="fa fa-exclamation-triangle" />&nbsp;
          {{ error }}
        </div> <!-- /error -->

        <!-- user checkboxes -->
        <div
          v-else-if="users && users.length"
          class="px-2 py-1 users-dropdown-options">
          <div
            v-for="user in users"
            :key="user.userId"
            class="form-check">
            <input
              :id="`userdd-${activatorId}-${user.userId}`"
              type="checkbox"
              class="form-check-input"
              :checked="localSelectedUsers.includes(user.userId)"
              @change="toggleUser(user.userId, $event.target.checked)">
            <label
              :for="`userdd-${activatorId}-${user.userId}`"
              class="form-check-label">
              {{ user.userName }} ({{ user.userId }})
            </label>
          </div>
        </div> <!-- /user checkboxes -->

        <v-list-item
          v-else-if="users && !users.length && searchTerm"
          disabled>
          {{ $t('users.noUsersMatch') }}
        </v-list-item>
      </v-list>
    </v-menu>
  </div>
</template>

<script>
import UserService from './UserService';
import { resolveMessage } from './resolveI18nMessage';

let searchTimeout;

export default {
  name: 'UserDropdown',
  emits: ['selected-users-updated'],
  props: {
    roleId: {
      type: String,
      required: false, // during creation, a role will not have an ID
      default: ''
    },
    selectedUsers: {
      type: Array,
      required: false, // can use initializeSelectionWithRole instead
      default: () => []
    },
    requestRoleStatus: { type: Boolean },
    initializeSelectionWithRole: { type: Boolean },
    selectedTooltip: { type: Boolean },
    label: {
      type: String,
      required: false,
      default: ''
    }
  },
  data () {
    return {
      error: '',
      loading: true,
      searchTerm: '',
      users: undefined,
      localSelectedUsers: this.selectedUsers || [],
      activatorId: `userdd-${this.roleId || Math.random().toString(36).slice(2, 10)}`
    };
  },
  watch: {
    searchTerm () {
      // debounce search input (was BVN's `debounce="400"` on b-form-input)
      if (searchTimeout) clearTimeout(searchTimeout);
      searchTimeout = setTimeout(() => {
        searchTimeout = null;
        this.loadUsers();
      }, 400);
    }
  },
  methods: {
    onMenuToggle (opened) {
      if (opened) {
        this.$nextTick(() => {
          this.$refs.searchInput?.focus();
        });
      }
    },
    getUsersStr () {
      const userArr = [...this.localSelectedUsers];
      userArr.sort();
      return userArr.join(', ');
    },
    loadUsers () {
      const query = {
        filter: this.searchTerm
      };
      if (this.requestRoleStatus && this.roleId != null) {
        query.roleId = this.roleId;
      }

      UserService.searchUsersMin(query).then((response) => {
        this.error = '';
        this.loading = false;
        this.users = JSON.parse(JSON.stringify(response.data));
        if (this.initializeSelectionWithRole) {
          this.localSelectedUsers = this.users.filter(u => u.hasRole).map(u => u.userId);
        }
      }).catch((error) => {
        this.loading = false;
        this.error = resolveMessage(error, this.$t);
      });
    },
    toggleUser (userId, checked) {
      let newSelection;
      if (checked) {
        newSelection = [...this.localSelectedUsers, userId];
      } else {
        newSelection = this.localSelectedUsers.filter(u => u !== userId);
      }
      this.localSelectedUsers = newSelection;
      const change = {
        newSelection,
        changedUser: { userId, newState: checked }
      };
      this.$emit('selected-users-updated', change, this.roleId);
    },
    clearSearchTerm () {
      this.searchTerm = '';
      this.$refs.searchInput?.focus();
    }
  },
  mounted () {
    this.loadUsers();
  }
};
</script>

<style>
/* The menu is teleported to body via v-menu, so styles must be unscoped to
   reach the rendered list. */
.users-dropdown-menu {
  min-width: 280px;
  max-height: 360px;
  overflow-y: auto;
  font-size: 0.85rem;
}
.users-dropdown-menu .form-check {
  padding-left: 1.6rem;
  margin-bottom: 2px;
}
.users-dropdown-menu .form-check-label {
  font-size: 0.85rem;
  cursor: pointer;
}
</style>
