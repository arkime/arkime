<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-btn
    size="small"
    title="toggle"
    @click="toggle"
    :class="btnClass"
    class="square-btn-sm btn-toggle"
  >
    <v-icon icon="mdi-close-thick" />
  </v-btn>
</template>

<script>
export default {
  name: 'ToggleBtn',
  emits: ['toggle'],
  props: {
    opened: {
      type: Boolean,
      default: false
    }
  },
  computed: {
    btnClass () {
      return {
        'btn-warning': this.opened,
        'collapsed btn-primary': !this.opened
      };
    }
  },
  methods: {
    toggle () {
      this.$emit('toggle');
    }
  }
};
</script>

<style scoped>
/* override btn-sm style to better fit in table rows */
.btn-sm {
  padding: 3px 8px;
  font-size: 12px;
}

/* use bootstrap theme styles for button */
.btn-toggle {
  border-color: rgb(var(--v-theme-primary));
  background-color: rgb(var(--v-theme-primary));
}
.btn-toggle:hover {
  border-color: rgb(var(--v-theme-primary));
  background-color: rgb(var(--v-theme-primary));
}
.btn-toggle:not(.collapsed) {
  border-color: rgb(var(--v-theme-warning));
  background-color: rgb(var(--v-theme-warning));
}
.btn-toggle:not(.collapsed):hover {
  border-color: rgb(var(--v-theme-warning));
  background-color: rgb(var(--v-theme-warning));
}

.btn-toggle {
  border-width: 1px;
  color: rgb(var(--v-theme-on-primary))
}

/* transition for font awesome icon
 * only works for rotating icons 45 degrees  */
.btn-toggle .mdi-close-thick {
  -webkit-transform-origin: 50% 50%;
          transform-origin: 50% 50%;

  -webkit-transition: all 750ms;
          transition: all 750ms;
}
.btn-toggle.collapsed .mdi-close-thick {
  -webkit-transform: rotate(-45deg);
      -ms-transform: rotate(-45deg);
          transform: rotate(-45deg);
}
</style>
