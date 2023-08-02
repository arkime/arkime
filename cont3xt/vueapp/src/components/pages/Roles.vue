<template>
  <div class="overflow-auto">
    <RolesCommon
        :currentUser="getUser"
        :cont3xt-dark-theme="getDarkThemeEnabled"
        @update-current-user="updateCurrentUser" />
  </div>
</template>

<script>
import { mapGetters } from 'vuex';
import RolesCommon from '../../../../../common/vueapp/Roles';
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
