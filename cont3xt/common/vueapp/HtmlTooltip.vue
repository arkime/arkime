<template>
  <v-tooltip :activator="(targetId) ? `#${targetId}` : 'parent'">
    <span v-html="tooltipHtml"/>
  </v-tooltip>
</template>

<script setup>
import { defineProps, computed } from 'vue';

const props = defineProps({
  html: { // string or object of shape ( title: string )
    required: true
  },
  targetId: {
    type: String,
    required: false
  }
});

const tooltipHtml = computed(() => (typeof props.html === 'string') ? props.html : props.html?.title);
</script>
