<template>
  <v-menu
    :activator="idSelector(target)"
    open-on-hover
    :close-on-content-click="false"
  >
    <v-sheet>
      <slot />
    </v-sheet>
  </v-menu>
</template>

<script>
import { idSelector } from '@/vue3-helpers';

export default {
  name: 'IdTooltip',
  props: {
    target: {
      type: String,
      default: ''
    } // id of target
  },
  methods: {
    idSelector
  }
};
</script>
