<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <input
      v-focus="focus"
      data-testid="focus-test"
    />
  </div>
</template>

<script>
export default {
  name: 'FocusComponent',
  props: {
    focus: {
      type: Boolean,
      defaut: false
    }
  }
};
</script>
