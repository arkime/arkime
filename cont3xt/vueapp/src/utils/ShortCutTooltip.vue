<template>
  <v-tooltip
    v-if="getShiftKeyHold"
    :model-value="getShiftKeyHold"
    :location="location"
    :target="targetId"
    :activator="`#${targetId}`"
    >
    <strong class="help-shortcut text-warning">
      <slot>Insert Shortcut Text Here</slot>
    </strong>
  </v-tooltip>
</template>

<script setup>
import { defineProps } from 'vue';
import { useStore } from 'vuex';
import { useGetters } from '@/vue3-helpers';

const store = useStore();
const { getShiftKeyHold } = useGetters(store);

defineProps({
  targetId: { // id of element tooltip should attach to
    type: String,
    required: true
  },
  location: { // location of the tooltip
    type: String,
    default: 'bottom'
  }
});
</script>
