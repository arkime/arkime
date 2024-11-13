<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-btn
    size="small"
    color="secondary"
    @shown="setFocus"
    :disabled="disabled"
    class="roles-dropdown no-wrap text-none"
  >
    <v-tooltip v-if="!!tooltip" activator="parent">
      {{ tooltip }}
    </v-tooltip>
    {{ displayText || getRolesStr(localSelectedRoles) }} <v-icon icon="mdi-menu-down"/>
    <v-menu
      activator="parent"
      location="bottom left"
      :close-on-content-click="false"
    >
      <v-card class="px-1 py-1 overflow-hidden">
        <div class="d-flex flex-column">
          <!-- roles search -->
          <div class="w-100">
            <v-text-field
              v-focus="focus"
              @input="searchRoles"
              @click:clear="searchRoles"
              v-model="searchTerm"
              placeholder="Search for roles..."
              clearable
            />
          </div> <!-- /roles search -->
          <div class="roles-dropdown-checkboxes overflow-auto" v-if="filteredRoles && filteredRoles.length">
            <!-- role checkboxes -->
            <v-checkbox
              v-for="role in filteredRoles"
              :key="role.value"
              v-model="localSelectedRoles"
              :value="role.value"
              @change="updateRoles">
              <template #label>
                {{ role.text }}
                <v-icon
                  v-if="role.userDefined"
                  title="User defined role"
                  v-tooltip="'User defined role'"
                  class="cursor-help ml-2"
                  icon="mdi-account"
                />
              </template>
            </v-checkbox>
            <!-- previously deleted roles -->
            <template v-for="role in localSelectedRoles" :key="role">
              <v-checkbox
                v-if="!roles.find(r => r.value === role)"
                v-model="localSelectedRoles"
                :value="role"
                @change="updateRoles">
                <template #label>
                  {{ role }}
                  <v-icon
                    icon="mdi-alert-rhombus"
                    class="cursor-help ml-2"
                    v-tooltip="'This role no longer exists'"
                  />
                </template>
              </v-checkbox>
            </template><!-- /previously deleted roles -->
            <!-- /role checkboxes -->
          </div>
          <div class="text-disabled mx-2 my-2"
            v-if="filteredRoles && !filteredRoles.length && searchTerm">
            No roles match your search
          </div>
        </div>
      </v-card>
    </v-menu>
  </v-btn>
</template>

<script>
import Focus from './Focus.vue';
import { searchRoles } from './vueFilters';

export default {
  name: 'RoleDropdown',
  directives: { Focus },
  props: {
    id: { type: String },
    tooltip: { type: String },
    displayText: { type: String },
    selectedRoles: { type: Array },
    roles: { type: Array, required: true },
    disabled: { type: Boolean, default: false }
  },
  emits: ['selected-roles-updated'],
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
    updateRoles () {
      this.$emit('selected-roles-updated', this.localSelectedRoles, this.id);
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
      const beforeLen = roles.length;
      roles = roles.slice(0, 2); // just first 2
      if (beforeLen !== roles.length) {
        roles.push(`(+${beforeLen - roles.length})`);
      }

      const allRoles = userDefinedRoles.concat(roles);

      if (allRoles.length === 0) {
        allRoles.push('None');
      }

      return allRoles.join(', ');
    },
    searchRoles () {
      this.filteredRoles = searchRoles(this.roles, this.searchTerm);
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
.roles-dropdown-checkboxes {
  max-height: 300px;
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
