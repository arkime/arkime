<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <!-- chrome is normal-flow inside a PageLayout shell, so collapsing is
       plain reflow — no measurement, spacer, or position juggling -->
  <v-expand-transition>
    <div v-show="showToolBars">
      <slot />
    </div>
  </v-expand-transition>
</template>

<script>
export default {
  computed: {
    // Boolean in the store will remember chosen toggle state for all pages
    showToolBars: function () {
      return this.$store.state.showToolBars;
    }
  }
};
</script>
