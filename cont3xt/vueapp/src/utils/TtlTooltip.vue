<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <b-tooltip noninteractive :target="target"
             placement="top" boundary="viewport">
    TTL: {{ ttl }} ({{ ttl * 1000 | readableDuration }})
  </b-tooltip>
</template>

<script>
export default {
  name: 'TtlTooltip',
  props: {
    ttl: Number,
    target: String
  }
};
</script>

<style scoped>

</style>
