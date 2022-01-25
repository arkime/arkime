<template>
  <div class="container-fluid">
    <UsersCommon
      :roles="userRoles"
      parent-app="Arkime">
      <template #loading>
        <MolochLoading />
      </template>
    </UsersCommon>
  </div>
</template>

<script>
import UserService from '@/components/users/UserService';
import MolochLoading from '@/components/utils/Loading';
import UsersCommon from '../../../../../common/Users';

export default {
  name: 'Users',
  components: {
    UsersCommon,
    MolochLoading
  },
  data () {
    return {
      userRoles: [],
      tmpRolesSupport: this.$constants.MOLOCH_TMP_ROLES_SUPPORT
    };
  },
  created () {
    if (this.tmpRolesSupport) { // TODO update roles when creating roles
      UserService.getRoles().then((response) => {
        this.userRoles = response;
      });
    }
  }
};
</script>
