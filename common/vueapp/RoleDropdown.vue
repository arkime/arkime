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
        <v-btn
          v-bind="activatorProps"
          :size="size"
          variant="outlined"
          color="secondary"
          class="roles-dropdown text-none"
          :id="activatorId"
          :disabled="disabled">
          {{ displayText || getRolesStr(localSelectedRoles) }}
          <v-icon
            end
            icon="mdi:mdi-menu-down" />
          <v-tooltip
            v-if="tooltip"
            :activator="`#${activatorId}`"
            location="top">
            {{ tooltip }}
          </v-tooltip>
        </v-btn>
      </template>

      <v-list
        density="compact"
        class="roles-dropdown-menu">
        <!-- roles search -->
        <div class="px-2 py-1">
          <v-text-field
            ref="searchInput"
            density="compact"
            variant="outlined"
            hide-details
            clearable
            prepend-inner-icon="fa:fa-search"
            :model-value="searchTerm"
            @update:model-value="searchRolesLocal"
            :placeholder="$t('users.rolesSearchPlaceholder')" />
        </div> <!-- /roles search -->
        <v-divider />

        <!-- role checkboxes -->
        <div
          v-if="filteredRoles && filteredRoles.length"
          class="px-2 py-1 roles-dropdown-options">
          <div
            v-for="role in filteredRoles"
            :key="role.value"
            class="dropdown-check">
            <input
              :id="`roledd-${activatorId}-${role.value}`"
              type="checkbox"
              class="dropdown-check-input"
              :checked="localSelectedRoles.includes(role.value)"
              @change="toggleRole(role.value, $event.target.checked)">
            <label
              :for="`roledd-${activatorId}-${role.value}`"
              class="dropdown-check-label">
              {{ role.text }}
              <v-icon
                icon="mdi-account"
                class="cursor-help ms-2"
                v-if="role.userDefined"
                :title="$t('users.userDefinedRoleMsg')" />
            </label>
          </div>
          <!-- previously deleted roles still selected on the user -->
          <template
            v-for="role in localSelectedRoles"
            :key="role">
            <div
              v-if="!roles.find(r => r.value === role)"
              class="dropdown-check">
              <input
                :id="`roledd-${activatorId}-${role}`"
                type="checkbox"
                class="dropdown-check-input"
                :checked="true"
                @change="toggleRole(role, $event.target.checked)">
              <label
                :for="`roledd-${activatorId}-${role}`"
                class="dropdown-check-label">
                {{ role }}
                <v-icon
                  icon="mdi-close-circle"
                  class="cursor-help ms-2"
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
    disabled: { type: Boolean, default: false },
    // pass-through v-btn size on the trigger; defaults to the
    // app-wide v-btn density set in viewer/main.js.
    size: { type: String, default: 'default' }
  },
  data () {
    return {
      searchTerm: '',
      filteredRoles: this.roles,
      localSelectedRoles: this.selectedRoles || [],
      // `id` may contain characters illegal in a CSS "#id" selector (Vuetify
      // resolves the activator via document.querySelector). An unescaped
      // colon etc. throws "not a valid selector" and wedges navigation -- see
      // the matching note in UserDropdown.vue. Sanitize to a CSS-safe id.
      activatorId: `roledd-${(this.id || Math.random().toString(36).slice(2, 10)).replace(/[^a-zA-Z0-9_-]/g, '-')}`
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
    }
  }
};
</script>

<style>
/* Force the activator button to match Vuetify's v-input--density-compact
   height (40px). Several forms place a Share-with-Roles button next to a
   compact v-text-field; with size="large" the button was 4px taller and
   read as misaligned. Bypassing Vuetify's size-based height keeps the
   pair flush regardless of the size prop. */
.roles-dropdown.v-btn {
  height: 30px;
  min-height: 30px;
}

/* The menu is teleported to body via v-menu, so styles must be unscoped to
   reach the rendered list. */
.roles-dropdown-menu {
  min-width: 240px;
  max-height: 360px;
  overflow-y: auto;
  font-size: 0.85rem;
}
.roles-dropdown-menu .dropdown-check {
  position: relative;
  padding-left: 1.6rem;
  margin-bottom: 2px;
}
.roles-dropdown-menu .dropdown-check-input {
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
.roles-dropdown-menu .dropdown-check-input:checked {
  background-color: rgb(var(--v-theme-primary)) !important;
  border-color: rgb(var(--v-theme-primary)) !important;
  background-image: url("data:image/svg+xml,%3csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 20 20'%3e%3cpath fill='none' stroke='%23fff' stroke-linecap='round' stroke-linejoin='round' stroke-width='3' d='M6 10l3 3 6-6'/%3e%3c/svg%3e") !important;
  background-size: 14px 14px !important;
  background-position: center !important;
  background-repeat: no-repeat !important;
}
.roles-dropdown-menu .dropdown-check-label {
  font-size: 0.85rem;
  cursor: pointer;
}
</style>
