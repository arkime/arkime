<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <b-button
    :size="size"
    v-if="logoutUrl"
    class="ml-2"
    title="Logout"
    @click="logout"
    v-b-tooltip.hover
    variant="outline-warning">
    <span class="fa fa-sign-out fa-fw"></span>
  </b-button>
</template>

<script>
export default {
  name: 'Logout',
  props: {
    size: {
      type: String,
      default: 'md'
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
