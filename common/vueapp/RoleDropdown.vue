<template>
  <b-dropdown
    size="sm"
    class="roles-dropdown"
    :text="displayText || getRolesStr(localSelectedRoles)">
    <!-- roles search -->
    <b-dropdown-header class="w-100 sticky-top">
      <b-form-input
        size="sm"
        @input="searchRoles"
        v-model="searchTerm"
        placeholder="Search for roles..."
      />
      <b-dropdown-divider />
    </b-dropdown-header> <!-- /roles search -->
    <b-dropdown-form v-if="filteredRoles && filteredRoles.length">
      <!-- role checkboxes -->
      <b-form-checkbox-group
        class="d-flex flex-column"
        v-model="localSelectedRoles">
        <b-form-checkbox
          :key="role.value"
          :value="role.value"
          v-for="role in filteredRoles"
          @change="updateRoles">
          {{ role.text }}
          <span
            v-b-tooltip.hover
            v-if="role.userDefined"
            title="User defined role"
            class="fa fa-user cursor-help ml-2"
          />
        </b-form-checkbox>
        <template v-for="role in localSelectedRoles">
          <b-form-checkbox
            :key="role"
            :value="role"
            @change="updateRoles"
            v-if="!roles.find(r => r.value === role)">
            {{ role }}
            <span
              class="fa fa-times-circle cursor-help ml-2"
              v-b-tooltip.hover="'This role no longer exists'"
            />
          </b-form-checkbox>
        </template>
      </b-form-checkbox-group> <!-- /role checkboxes -->
    </b-dropdown-form>
    <b-dropdown-item disabled
      v-if="filteredRoles && !filteredRoles.length && searchTerm">
      No roles match your search
    </b-dropdown-item>
  </b-dropdown>
</template>

<script>
export default {
  name: 'RoleDropdown',
  props: {
    id: { type: String },
    displayText: { type: String },
    selectedRoles: { type: Array },
    roles: { type: Array, required: true }
  },
  data () {
    return {
      searchTerm: '',
      filteredRoles: this.roles,
      localSelectedRoles: this.selectedRoles || []
    };
  },
  watch: {
    selectedRoles (newValue) { // localSelectedRoles must be changed whenever selectedRoles is (this syncs during sorting)
      this.localSelectedRoles = newValue || [];
    }
  },
  methods: {
    updateRoles (newVal) {
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

      const allRoles = userDefinedRoles.concat(roles);
      return allRoles.join(', ');
    },
    searchRoles () {
      this.filteredRoles = this.$options.filters.searchRoles(this.roles, this.searchTerm);
    }
  }
};
</script>

<style>
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
