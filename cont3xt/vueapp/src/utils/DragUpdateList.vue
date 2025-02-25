<template>
  <vue-draggable-next :model-value="value" @update:model-value="update" ghost-class="drag-ghost" handle=".drag-handle">
    <slot/>
  </vue-draggable-next>
</template>

<script setup>
import { VueDraggableNext } from 'vue-draggable-next';

const props = defineProps({
  value: {
    type: Array,
    required: true
  }
});

const emit = defineEmits(['update']);

function update (newList) {
  emit('update', {
    oldList: props.value,
    newList
  });
}
</script>
