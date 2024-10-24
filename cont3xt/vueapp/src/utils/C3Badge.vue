<template>
  <span class="badge" :class=[bgClass]><slot/></span>
</template>

<script setup>
import { defineProps, computed } from 'vue';

const props = defineProps({
  variant: {
    type: String,
    required: true
  }
});

const bgClass = computed(() => {
  switch (props.variant) {
  case 'success':
    return 'bg-success';
  case 'danger':
  case 'error':
    return 'bg-error';
  case 'secondary':
    return 'bg-secondary-gray';
  case 'light':
  default:
    return 'bg-light';
  }
});
</script>

<style scoped>
/* bootstrap styling of badges */
.badge {
  display: inline-block;

  border-radius: 0.25rem;
  padding: .25em .4em;

  font-size: 75%;
  font-weight: 700;
  line-height: 1;

  text-align: center;
  vertical-align: baseline;
  white-space: nowrap;
}
</style>
