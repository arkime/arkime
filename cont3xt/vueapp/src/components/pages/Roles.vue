<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <RolesCommon
      :currentUser="getUser"
      :cont3xt-dark-theme="getDarkThemeEnabled"
      @update-current-user="updateCurrentUser" />
</template>

<script>
import { mapGetters } from 'vuex';
import RolesCommon from '@common/Roles.vue';
import UserService from '@/components/services/UserService';

export default {
  name: 'Roles',
  components: {
    RolesCommon
  },
  computed: {
    ...mapGetters(['getUser', 'getDarkThemeEnabled'])
  },
  methods: {
    updateCurrentUser () {
      // NOTE: don't need to do anything with the data (the store does it)
      UserService.getUser();
    }
  }
};
</script>

<style scoped>

</style>
