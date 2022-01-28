<template>
  <div class="container-fluid">
    <UsersCommon
      :roles="userRoles"
      parent-app="Arkime"
      :current-user="user"
      @update-roles="getRoles"
      @update-current-user="updateCurrentUser">
      <template #loading>
        <MolochLoading />
      </template>
    </UsersCommon>
  </div>
</template>

<script>
import UserService from '@/components/users/UserService';
import MolochLoading from '@/components/utils/Loading';
import UsersCommon from '../../../../../common/vueapp/Users';

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
  computed: {
    user: {
      get () {
        return this.$store.state.user;
      },
      set (newValue) {
        this.$store.commit('setUser', newValue);
      }
    }
  },
  created () {
    if (this.tmpRolesSupport) {
      this.getRoles();
    }
  },
  methods: {
    getRoles () {
      UserService.getRoles().then((response) => {
        this.userRoles = response;
      });
    },
    updateCurrentUser () {
      UserService.getCurrent().then((response) => {
        this.user = response;
      });
    }
  }
};
</script>
