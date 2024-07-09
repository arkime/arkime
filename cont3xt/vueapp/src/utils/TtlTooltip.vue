<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-tooltip :target="target" :activator="`#${target}`" location="top">
    TTL: {{ ttl }} ({{ readableDuration(ttl * 1000) }})
  </v-tooltip>
</template>

<script>
import { readableDuration } from '@/utils/filters.js';
export default {
  name: 'TtlTooltip',
  props: {
    ttl: Number,
    target: String
  },
  methods: { readableDuration }
};
</script>

<style scoped>

</style>
