<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <!-- alert -->
  <v-alert
    v-if="message"
    :type="vuetifyType"
    variant="tonal"
    density="compact"
    closable
    @click:close="done(null)">
    {{ message || 'undefined message' }}
  </v-alert> <!-- /alert -->
</template>

<script>
let timeout;

export default {
  name: 'ArkimeToast',
  props: {
    message: {
      type: String,
      default: ''
    },
    done: {
      type: Function,
      default: () => {}
    },
    duration: {
      type: Number,
      default: 5000
    },
    type: {
      type: String,
      default: 'info'
    }
  },
  watch: {
    message: function (newVal, oldVal) {
      if (newVal !== oldVal) {
        this.dismissAfter();
      }
    }
  },
  computed: {
    vuetifyType: function () {
      // Vuetify v-alert accepts: success | info | warning | error.
      // Map Bootstrap-flavored "danger" → Vuetify "error".
      return this.type === 'danger' ? 'error' : this.type;
    }
  },
  created: function () {
    if (this.message) {
      this.dismissAfter();
    }
  },
  methods: {
    /* helper functions ------------------------------------------ */
    dismissAfter: function () {
      if (timeout) { clearTimeout(timeout); }

      timeout = setTimeout(() => {
        this.done();
      }, this.duration);
    }
  },
  beforeUnmount () {
    if (timeout) { clearTimeout(timeout); }
  }
};
</script>
