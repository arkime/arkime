<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div
    ref="roleDropdown"
    class="d-inline-block">
    <BTooltip
      v-if="tooltip"
      :target="$refs.roleDropdown"
      placement="top">
      {{ tooltip }}
    </BTooltip>
    <b-dropdown
      size="sm"
      auto-close="outside"
      teleport-disabled
      @shown="setFocus"
      :disabled="disabled"
      class="roles-dropdown no-wrap"
      :text="displayText || getRolesStr(localSelectedRoles)">
      <!-- roles search -->
      <b-dropdown-header class="w-100 sticky-top">
        <b-input-group size="sm">
          <b-form-input
            v-focus="focus"
            :model-value="searchTerm"
            @update:model-value="searchRolesLocal"
            :placeholder="$t('users.rolesSearchPlaceholder')" />
          <template #append>
            <b-button
              :disabled="!searchTerm"
              @click="clearSearchTerm"
              variant="outline-secondary">
              <span class="fa fa-close" />
            </b-button>
          </template>
        </b-input-group>
        <b-dropdown-divider />
      </b-dropdown-header> <!-- /roles search -->
      <b-dropdown-form v-if="filteredRoles && filteredRoles.length">
        <!-- role checkboxes -->
        <b-form-checkbox-group
          stacked
          :model-value="localSelectedRoles"
          @update:model-value="updateRoles">
          <b-form-checkbox
            :key="role.value"
            :value="role.value"
            v-for="role in filteredRoles">
            {{ role.text }}
            <span
              v-if="role.userDefined"
              :title="$t('users.userDefinedRoleMsg')"
              class="fa fa-user cursor-help ms-2" />
          </b-form-checkbox>
          <template v-for="role in localSelectedRoles">
            <!-- previously deleted roles -->
            <b-form-checkbox
              :key="role"
              :value="role"
              v-if="!roles.find(r => r.value === role)">
              {{ role }}
              <span
                class="fa fa-times-circle cursor-help ms-2"
                :title="$t('users.missingRoleMsg')" />
            </b-form-checkbox>
          </template>
        </b-form-checkbox-group> <!-- /role checkboxes -->
      </b-dropdown-form>
      <b-dropdown-item
        disabled
        v-if="filteredRoles && !filteredRoles.length && searchTerm">
        {{ $t('users.noRolesMatchSearch') }}
      </b-dropdown-item>
    </b-dropdown>
  </div>
</template>

<script>
import Focus from './Focus.vue';
import { searchRoles } from './vueFilters.js';

export default {
  name: 'RoleDropdown',
  directives: { Focus },
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
      focus: false,
      searchTerm: '',
      filteredRoles: this.roles,
      localSelectedRoles: this.selectedRoles || []
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
    updateRoles (newVal) {
      this.localSelectedRoles = newVal || [];
      this.$emit('selected-roles-updated', newVal, this.id);
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
      this.searchTerm = newVal;
      this.filteredRoles = searchRoles(this.roles, this.searchTerm);
    },
    clearSearchTerm () {
      this.searchTerm = '';
      this.searchRolesLocal();
      this.setFocus();
    },
    setFocus () {
      this.focus = true;
      setTimeout(() => {
        this.focus = false;
      }, 100);
    }
  }
};
</script>

<style>
.roles-dropdown > ul.dropdown-menu li > form > div {
  color: var(--color-foreground, black) !important;
}
/* hides elements scrolling behind sticky search bar */
.roles-dropdown .sticky-top {
  top: -8px;
}
.roles-dropdown .dropdown-header {
  padding: 0rem 0.5rem;
  background-color: var(--color-background);
}
.roles-dropdown .dropdown-header > li {
  padding-top: 10px;
  background-color: var(--color-background);
}
.roles-dropdown .dropdown-divider {
  margin-top: 0px;
}

.roles-dropdown .dropdown-item,
.roles-dropdown .custom-control {
  padding-left: 0.5rem;
}
</style>
