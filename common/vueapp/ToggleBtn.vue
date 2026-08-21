<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-btn
    variant="flat"
    size="small"
    density="comfortable"
    icon
    :color="opened ? 'warning' : 'primary'"
    :class="{ 'toggle-btn-collapsed': !opened }"
    class="toggle-btn"
    title="toggle"
    @click="toggle">
    <v-icon icon="mdi-close" />
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
  methods: {
    toggle () {
      this.$emit('toggle');
    }
  }
};
</script>

<style>
/* Spin the close icon 45° when collapsed so the same glyph reads as
   a `+` (collapsed) or `×` (opened). */
.toggle-btn .v-icon {
  transform-origin: 50% 50%;
  transition: transform 750ms;
}
.toggle-btn.toggle-btn-collapsed > .v-btn__content > .v-icon {
  transform: rotate(-45deg);
}
</style>
