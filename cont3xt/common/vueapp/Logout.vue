<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-btn
    :size="size"
    v-if="logoutUrl"
    class="ml-2"
    @click="logout"
    v-tooltip="'Logout'"
    title="Logout"
    variant="outlined"
    color="warning">
    <v-icon icon="mdi-logout mdi-fw" />
  </v-btn>
</template>

<script>
export default {
  name: 'Logout',
  props: {
    size: {
      type: String
    },
    basePath: {
      type: String,
      default: '/'
    }
  },
  data () {
    return {
      logoutUrl: this.$constants.LOGOUT_URL
    };
  },
  methods: {
    logout () {
      fetch(this.$constants.LOGOUT_URL, {
        method: 'POST',
        credentials: 'include'
      }).finally(() => {
        location.reload();
      });
    }
  }
};
</script>
