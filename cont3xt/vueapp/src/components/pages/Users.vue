<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="container-fluid overflow-auto">
    <UsersCommon
      v-if="getUser"
      :roles="getRoles"
      parent-app="Cont3xt"
      :current-user="getUser"
      @update-roles="updateRoles"
      @update-current-user="updateCurrentUser">
    </UsersCommon>
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import UsersCommon from '../../../../../common/vueapp/Users';
import UserService from '@/components/services/UserService';

export default {
  name: 'Users',
  components: { UsersCommon },
  computed: {
    ...mapGetters(['getUser', 'getRoles'])
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
