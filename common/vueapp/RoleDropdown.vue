<template>
  <b-dropdown
    size="sm"
    class="roles-dropdown"
    :text="displayText || getRolesStr(localSelectedRoles)">
    <b-dropdown-header>
      <b-form-input
        @input="searchRoles"
        v-model.lazy="roleSearch"
        placeholder="Search for roles..."
        class="form-control-sm dropdown-typeahead"
      />
    </b-dropdown-header>
    <b-dropdown-divider />
    <b-dropdown-form class="d-flex flex-column">
      <b-form-checkbox-group
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
      </b-form-checkbox-group>
    </b-dropdown-form>
    <b-dropdown-item disabled
      v-if="filteredRoles && !filteredRoles.length && roleSearch">
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
      roleSearch: '',
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
      if (!this.roleSearch) {
        this.filteredRoles = this.roles;
        return;
      }

      this.filteredRoles = this.roles.filter((role) => {
        return role.text.toLowerCase().includes(this.roleSearch.toLowerCase());
      });
    }
  }
};
</script>

<style>
.roles-dropdown .dropdown-header {
  padding: 0.25rem 0.5rem 0;
}
.roles-dropdown .dropdown-item,
.roles-dropdown .custom-control {
  padding-left: 0.5rem;
}
</style>
