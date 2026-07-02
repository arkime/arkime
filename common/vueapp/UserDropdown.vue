<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-inline-flex align-center">
    <label
      v-if="label"
      :for="activatorId"
      class="mb-0 me-1">{{ label }}</label>

    <v-menu
      :close-on-content-click="false"
      location="bottom"
      @update:model-value="onMenuToggle">
      <template #activator="{ props: activatorProps }">
        <v-btn
          v-bind="activatorProps"
          size="large"
          variant="outlined"
          color="secondary"
          class="users-dropdown text-none"
          data-testid="user-dropdown"
          :id="activatorId">
          <slot
            :count="localSelectedUsers.length"
            :filter="searchTerm"
            :unknown="loading || error">
            {{ getUsersStr() }}
          </slot>
          <v-icon
            end
            icon="mdi:mdi-menu-down" />
          <v-tooltip
            v-if="selectedTooltip"
            :activator="`#${activatorId}`">
            {{ selectedTooltip ? getUsersStr() : '' }}
          </v-tooltip>
        </v-btn>
      </template>

      <v-list
        density="compact"
        class="users-dropdown-menu">
        <!-- search bar -->
        <div class="px-2 py-1">
          <v-text-field
            ref="searchInput"
            density="compact"
            variant="outlined"
            hide-details
            clearable
            prepend-inner-icon="fa:fa-search"
            :model-value="searchTerm"
            @update:model-value="(val) => { searchTerm = val || ''; }"
            :placeholder="$t('users.searchUserPlaceholder')" />
        </div>
        <v-divider />

        <!-- loading -->
        <div
          v-if="loading"
          class="mt-3 text-center">
          <v-icon
            icon="mdi-loading"
            size="large"
            class="mdi-spin" />
          <p>{{ $t('common.loading') }}</p>
        </div> <!-- /loading -->

        <!-- error -->
        <v-alert
          v-else-if="error"
          type="warning"
          variant="tonal"
          density="compact"
          class="mt-3 mx-2">
          {{ error }}
        </v-alert> <!-- /error -->

        <!-- user checkboxes -->
        <div
          v-else-if="users && users.length"
          class="px-2 py-1 users-dropdown-options">
          <div
            v-for="user in users"
            :key="user.userId"
            class="dropdown-check">
            <input
              :id="`userdd-${activatorId}-${user.userId}`"
              type="checkbox"
              class="dropdown-check-input"
              :checked="localSelectedUsers.includes(user.userId)"
              @change="toggleUser(user.userId, $event.target.checked)">
            <label
              :for="`userdd-${activatorId}-${user.userId}`"
              class="dropdown-check-label">
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
      // roleId can contain a colon (e.g. "role:role1"). A colon is legal in
      // an HTML id but NOT in a CSS "#id" selector, and Vuetify resolves the
      // tooltip/menu activator via document.querySelector(`#${activatorId}`).
      // An unescaped colon throws "not a valid selector", which aborts the
      // render and wedges SPA navigation. Sanitize to a CSS-safe id.
      activatorId: `userdd-${(this.roleId || Math.random().toString(36).slice(2, 10)).replace(/[^a-zA-Z0-9_-]/g, '-')}`
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
.users-dropdown-menu .dropdown-check {
  position: relative;
  padding-left: 1.6rem;
  margin-bottom: 2px;
}
.users-dropdown-menu .dropdown-check-input {
  appearance: none;
  -webkit-appearance: none;
  position: absolute;
  left: 0;
  top: 2px;
  width: 14px;
  height: 14px;
  border: 1px solid rgb(var(--v-theme-neutral));
  border-radius: 3px;
  background-color: rgb(var(--v-theme-background)) !important;
  cursor: pointer;
}
/* !important required to beat overrides.css's global `input { background:
   rgb(var(--v-theme-input-bg)) !important }` rule -- otherwise the primary fill
   doesn't paint and the white check glyph is invisible on the white bg. */
.users-dropdown-menu .dropdown-check-input:checked {
  background-color: rgb(var(--v-theme-primary)) !important;
  border-color: rgb(var(--v-theme-primary)) !important;
  background-image: url("data:image/svg+xml,%3csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 20 20'%3e%3cpath fill='none' stroke='%23fff' stroke-linecap='round' stroke-linejoin='round' stroke-width='3' d='M6 10l3 3 6-6'/%3e%3c/svg%3e") !important;
  background-size: 14px 14px !important;
  background-position: center !important;
  background-repeat: no-repeat !important;
}
.users-dropdown-menu .dropdown-check-label {
  font-size: 0.85rem;
  cursor: pointer;
}
</style>
