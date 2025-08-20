<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="container-fluid overflow-auto">
    <UsersCommon
      :dark="getTheme === 'dark'"
      v-if="getUser"
      :roles="getRoles"
      parent-app="Parliament"
      :current-user="getUser"
      @update-roles="updateRoles"
      @update-current-user="updateCurrentUser">
      <template #loading>
        <ArkimeLoading />
      </template>
    </UsersCommon>
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import UsersCommon from '@common/Users.vue';
import UserService from '@/components/user.service.js';

export default {
  name: 'Users',
  components: { UsersCommon },
  computed: {
    ...mapGetters(['getUser', 'getRoles', 'getTheme'])
  },
  created () {
    UserService.getUser();
    UserService.getRoles();
  },
  methods: {
    updateRoles () {
      // NOTE: don't need to do anything with the data (the store does it)
      UserService.getRoles();
    },
    updateCurrentUser () {
      // NOTE: don't need to do anything with the data (the store does it)
      UserService.getUser();
    }
  }
};
</script>
