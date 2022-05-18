<template>
  <b-dropdown
    size="sm"
    class="roles-dropdown"
    :text="displayText || getRolesStr(localSelectedRoles)">
    <b-dropdown-form>
      <b-form-checkbox-group
        v-model="localSelectedRoles">
        <b-form-checkbox
          :key="role.value"
          :value="role.value"
          v-for="role in roles"
          @change="updateRoles">
          {{ role.text }}
          <span
            v-if="role.userDefined"
            class="fa fa-user cursor-help ml-2"
            v-b-tooltip.hover="'User defined role'"
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
      localSelectedRoles: this.selectedRoles || []
    };
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
    }
  }
};
</script>