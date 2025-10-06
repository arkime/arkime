<template>
  <v-tooltip :activator="idSelector(target)">
    <slot />
  </v-tooltip>
</template>

<script>
import { idSelector } from '@/vue3-helpers';

export default {
  name: 'IdTooltip',
  props: {
    target: {
      type: String,
      default: ''
    }
  },
  methods: {
    idSelector
  }
};
</script>
