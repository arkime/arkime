<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <b-button
    :size="size"
    v-if="logoutUrl"
    class="ms-2"
    @click="logout"
    id="logout-button"
    variant="outline-warning">
    <span class="fa fa-sign-out fa-fw" />
    <BTooltip
      target="logout-button"
      placement="bottom">
      {{ $t('common.logout') }}
    </BTooltip>
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
      if (this.$constants.LOGOUT_URL_METHOD === 'GET') {
        window.location = this.$constants.LOGOUT_URL;
      } else {
        fetch(this.$constants.LOGOUT_URL, {
          method: 'POST',
          credentials: 'include'
        }).finally(() => {
          location.reload();
        });
      }
    }
  }
};
</script>
