<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <id-tooltip
    :target="target"
    location="top"
  >
    TTL: {{ ttl }} ({{ readableDuration(ttl * 1000) }})
  </id-tooltip>
</template>

<script>
import IdTooltip from '@/utils/IdTooltip.vue';
import { readableDuration } from '@/utils/filters.js';

export default {
  name: 'TtlTooltip',
  components: { IdTooltip },
  props: {
    ttl: Number,
    target: String
  },
  methods: { readableDuration }
};
</script>
