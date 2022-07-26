<template>
  <div class="container-fluid">
    <h1>Roles</h1>
    <b-overlay
        rounded="sm"
        blur="0.2rem"
        opacity="0.9"
        :show="loading"
        variant="transparent">

      <!-- loading overlay template -->
      <template #overlay>
        <div class="text-center margin-for-nav-and-progress">
          <span class="fa fa-circle-o-notch fa-spin fa-2x" />
          <p>Loading roles...</p>
        </div>
      </template> <!-- /loading overlay template -->

      <template v-if="!loading">
        <span v-for="role in getUser.assignableRoles" :key="role">
          <UserDropdown :display-text="`Who Has ${role}`" :role-id="role" :selected-users="[]" @selected-users-updated="updateUsers"
            :initialize-selection-with-role="true" />
        </span>
      </template>
    </b-overlay>
  </div>
</template>

<script>
import { mapGetters } from 'vuex';
import UserDropdown from '../../../../../common/vueapp/UserDropdown';
import UserService from '../../../../../common/vueapp/UserService';

export default {
  name: 'Roles',
  components: {
    UserDropdown
  },
  computed: {
    ...mapGetters(['getUser']),
    loading () {
      return this.getUser?.assignableRoles == null;
    }
  },
  methods: {
    updateUsers ({ changedUser }, roleId) {
      UserService.updateUserRole({
        userId: changedUser.userId,
        roleId,
        newRoleState: changedUser.newState
      });
    }
  }
};
</script>

<style scoped>

</style>
