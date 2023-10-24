<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span class="px-1 ml-1 rounded bg-danger tag tag-pad-bottom">
    {{ value }}
  </span>
</template>

<script>
export default {
  name: 'IndicatorTag',
  props: {
    value: {
      type: String,
      required: true
    }
  }
};
</script>

<style scoped>
.tag-pad-bottom {
  padding-bottom: 2px;
}
</style>
