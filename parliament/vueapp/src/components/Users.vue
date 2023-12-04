<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="container-fluid overflow-auto">
    <UsersCommon
      v-if="getUser"
      :roles="getRoles"
      parent-app="Parliament"
      :current-user="getUser"
      @update-roles="updateRoles"
      @update-current-user="updateCurrentUser">
    </UsersCommon>
    <div v-else>
      <!-- error that we can't fetch the user -->
      <div class="alert alert-danger" role="alert">
        <h4 class="alert-heading">Error</h4>
        <p>There was an error fetching the user.</p>
      </div>
    </div>
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import UsersCommon from '../../../../common/vueapp/Users';
import UserService from '@/components/user.service';

export default {
  name: 'Users',
  components: { UsersCommon },
  computed: {
    ...mapGetters(['getUser', 'getRoles'])
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
