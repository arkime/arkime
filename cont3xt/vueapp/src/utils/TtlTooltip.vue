<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <b-tooltip noninteractive :target="target"
             placement="top" boundary="viewport">
    TTL: {{ ttl }} ({{ readableDuration(ttl * 1000) }})
  </b-tooltip>
</template>

<script>
import { readableDuration } from '@/utils/filters.js';
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
