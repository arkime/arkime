<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div
    ref="roleDropdown"
    class="d-inline-block">
    <v-menu
      :close-on-content-click="false"
      location="bottom"
      @update:model-value="onMenuToggle">
      <template #activator="{ props: activatorProps }">
        <button
          v-bind="activatorProps"
          type="button"
          class="btn btn-sm btn-outline-secondary roles-dropdown no-wrap"
          :id="activatorId"
          :disabled="disabled">
          {{ displayText || getRolesStr(localSelectedRoles) }}
          <span class="fa fa-caret-down ms-1" />
          <v-tooltip
            v-if="tooltip"
            :activator="`#${activatorId}`"
            location="top">
            {{ tooltip }}
          </v-tooltip>
        </button>
      </template>

      <v-list
        density="compact"
        class="roles-dropdown-menu">
        <!-- roles search -->
        <div class="px-2 py-1">
          <div class="input-group input-group-sm">
            <input
              ref="searchInput"
              type="text"
              class="form-control"
              :value="searchTerm"
              @input="searchRolesLocal($event.target.value)"
              :placeholder="$t('users.rolesSearchPlaceholder')">
            <button
              type="button"
              class="btn btn-outline-secondary"
              :disabled="!searchTerm"
              @click="clearSearchTerm">
              <span class="fa fa-close" />
            </button>
          </div>
        </div> <!-- /roles search -->
        <v-divider />

        <!-- role checkboxes -->
        <div
          v-if="filteredRoles && filteredRoles.length"
          class="px-2 py-1 roles-dropdown-options">
          <div
            v-for="role in filteredRoles"
            :key="role.value"
            class="form-check">
            <input
              :id="`roledd-${activatorId}-${role.value}`"
              type="checkbox"
              class="form-check-input"
              :checked="localSelectedRoles.includes(role.value)"
              @change="toggleRole(role.value, $event.target.checked)">
            <label
              :for="`roledd-${activatorId}-${role.value}`"
              class="form-check-label">
              {{ role.text }}
              <span
                v-if="role.userDefined"
                :title="$t('users.userDefinedRoleMsg')"
                class="fa fa-user cursor-help ms-2" />
            </label>
          </div>
          <!-- previously deleted roles still selected on the user -->
          <template
            v-for="role in localSelectedRoles"
            :key="role">
            <div
              v-if="!roles.find(r => r.value === role)"
              class="form-check">
              <input
                :id="`roledd-${activatorId}-${role}`"
                type="checkbox"
                class="form-check-input"
                :checked="true"
                @change="toggleRole(role, $event.target.checked)">
              <label
                :for="`roledd-${activatorId}-${role}`"
                class="form-check-label">
                {{ role }}
                <span
                  class="fa fa-times-circle cursor-help ms-2"
                  :title="$t('users.missingRoleMsg')" />
              </label>
            </div>
          </template>
        </div> <!-- /role checkboxes -->

        <v-list-item
          v-else-if="filteredRoles && !filteredRoles.length && searchTerm"
          disabled>
          {{ $t('users.noRolesMatchSearch') }}
        </v-list-item>
      </v-list>
    </v-menu>
  </div>
</template>

<script>
import { searchRoles } from './vueFilters.js';

export default {
  name: 'RoleDropdown',
  emits: ['selected-roles-updated'],
  props: {
    id: {
      type: String,
      default: ''
    },
    tooltip: {
      type: String,
      default: ''
    },
    displayText: {
      type: String,
      default: ''
    },
    selectedRoles: {
      type: Array,
      default: () => []
    },
    truncate: { type: Number, default: 0 },
    roles: { type: Array, required: true },
    disabled: { type: Boolean, default: false }
  },
  data () {
    return {
      searchTerm: '',
      filteredRoles: this.roles,
      localSelectedRoles: this.selectedRoles || [],
      activatorId: `roledd-${this.id || Math.random().toString(36).slice(2, 10)}`
    };
  },
  watch: {
    roles (newRoles) {
      this.filteredRoles = newRoles;
    },
    selectedRoles (newValue) { // localSelectedRoles must be changed whenever selectedRoles is (this syncs during sorting)
      this.localSelectedRoles = newValue || [];
    }
  },
  methods: {
    onMenuToggle (opened) {
      if (opened) {
        // focus the search input once the menu mounts
        this.$nextTick(() => {
          this.$refs.searchInput?.focus();
        });
      }
    },
    toggleRole (value, checked) {
      let newSelection;
      if (checked) {
        newSelection = [...this.localSelectedRoles, value];
      } else {
        newSelection = this.localSelectedRoles.filter(r => r !== value);
      }
      this.localSelectedRoles = newSelection;
      this.$emit('selected-roles-updated', newSelection, this.id);
    },
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

      let allRoles = userDefinedRoles.concat(roles);

      // Truncate list so button doesn't get too long if truncate is specified
      if (this.truncate && allRoles.length > this.truncate) {
        allRoles = allRoles.splice(0, this.truncate);
        allRoles.push('...');
      }

      return allRoles.join(', ');
    },
    searchRolesLocal (newVal) {
      this.searchTerm = newVal || '';
      this.filteredRoles = searchRoles(this.roles, this.searchTerm);
    },
    clearSearchTerm () {
      this.searchTerm = '';
      this.searchRolesLocal('');
      this.$refs.searchInput?.focus();
    }
  }
};
</script>

<style>
/* The menu is teleported to body via v-menu, so styles must be unscoped to
   reach the rendered list. */
.roles-dropdown-menu {
  min-width: 240px;
  max-height: 360px;
  overflow-y: auto;
  font-size: 0.85rem;
}
.roles-dropdown-menu .form-check {
  padding-left: 1.6rem;
  margin-bottom: 2px;
}
.roles-dropdown-menu .form-check-label {
  font-size: 0.85rem;
  cursor: pointer;
}
</style>
