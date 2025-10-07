<template>
  <v-tooltip :activator="(targetId) ? `#${targetId}` : 'parent'">
    <span v-html="tooltipHtml" />
  </v-tooltip>
</template>

<script setup>
import { computed } from 'vue';

const props = defineProps({
  html: { // string or object of shape ( title: string )
    type: [String, Object],
    required: true
  },
  targetId: {
    type: String,
    required: false,
    default: ''
  }
});

const tooltipHtml = computed(() => (typeof props.html === 'string') ? props.html : props.html?.title);
</script>
