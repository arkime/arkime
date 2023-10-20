<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="container-fluid">
    <UsersCommon
      :roles="userRoles"
      parent-app="Arkime"
      :current-user="user"
      @update-roles="getRoles"
      @update-current-user="updateCurrentUser">
      <template #loading>
        <ArkimeLoading />
      </template>
    </UsersCommon>
  </div>
</template>

<script>
import UserService from './UserService';
import ArkimeLoading from '../utils/Loading';
import UsersCommon from '../../../../../common/vueapp/Users';

export default {
  name: 'Users',
  components: {
    UsersCommon,
    ArkimeLoading
  },
  computed: {
    user: {
      get () {
        return this.$store.state.user;
      },
      set (newValue) {
        this.$store.commit('setUser', newValue);
      }
    },
    userRoles: {
      get () {
        return this.$store.state.roles;
      },
      set (newValue) {
        this.$store.commit('setRoles', newValue);
      }
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
