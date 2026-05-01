<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-btn
    :size="vuetifySize"
    v-if="logoutUrl"
    class="ms-2"
    variant="outlined"
    color="warning"
    @click="logout">
    <span class="fa fa-sign-out fa-fw" />
    <v-tooltip
      activator="parent"
      location="bottom">
      {{ $t('common.logout') }}
    </v-tooltip>
  </v-btn>
</template>

<script>
// Map Bootstrap-style size names ('sm', 'md', 'lg') to Vuetify size names
// so callers passing legacy BVN-era sizes still work during the migration.
const SIZE_MAP = { sm: 'small', md: 'default', lg: 'large' };

export default {
  name: 'Logout',
  props: {
    size: {
      type: String,
      default: 'default'
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
  computed: {
    vuetifySize () {
      return SIZE_MAP[this.size] || this.size;
    }
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
