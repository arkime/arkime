<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <RolesCommon
      :currentUser="user"
      @update-current-user="updateCurrentUser">
    <template #loading>
      <ArkimeLoading />
    </template>
  </RolesCommon>
</template>

<script>
import UserService from '@/components/users/UserService';
import ArkimeLoading from '@/components/utils/Loading';
import RolesCommon from '../../../../../common/vueapp/Roles';

export default {
  name: 'Roles',
  components: {
    RolesCommon,
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
    }
  },

  methods: {
    updateCurrentUser () {
      UserService.getCurrent().then((response) => {
        this.user = response;
      });
    }
  }
};
</script>

<style scoped>

</style>
