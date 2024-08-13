<template>
  <vue-draggable-next :model-value="value" ghost-class="drag-ghost" handle=".drag-handle" 
    @update:model-value="test">
    <!-- @update="change"  -->
    <slot/>
  </vue-draggable-next>
</template>

<script setup>
import { defineProps, defineEmits } from 'vue';
import { VueDraggableNext } from 'vue-draggable-next';

const props = defineProps({
  value: {
    type: Array,
    required: true
  }
});

// TODO: toby - type update?
const emit = defineEmits(['update']);

function test (newList) {
  // // remove the dragged field from the list
  // const draggedField = newList.splice(oldIndex, 1)[0];
  // // and replace it in the new position
  // newList.splice(newIndex, 0, draggedField);
  // // update the parent

  emit('update', {
    oldList: props.value,
    newList
    // oldIndex,
    // newIndex,
    // element: draggedField
  });
}

function change ({ oldIndex, newIndex }) {
  const oldList = props.value;
  const newList = [...props.value];

  // remove the dragged field from the list
  const draggedField = newList.splice(oldIndex, 1)[0];
  // and replace it in the new position
  newList.splice(newIndex, 0, draggedField);
  // update the parent

  emit('update', { oldList, newList, oldIndex, newIndex, element: draggedField });
}
</script>
