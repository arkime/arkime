<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <RolesCommon
    :current-user="getUser"
    @update-current-user="updateCurrentUser" />
</template>

<script setup>
import { computed } from 'vue';
import { useStore } from 'vuex';

import RolesCommon from '@common/Roles.vue';
import UserService from '@/components/user.service.js';

const store = useStore();
// the /roles route guard (router.js requireUser) already fetches the
// current user before this component mounts
const getUser = computed(() => store.state.user);

function updateCurrentUser () {
  // NOTE: don't need to do anything with the data (the store does it)
  UserService.getUser();
}
</script>
