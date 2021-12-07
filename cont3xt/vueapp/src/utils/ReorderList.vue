<template>
  <div
    @drop="drop($event)"
    @dragover.prevent="dragOver($event, index)">
    <span
      draggable
      class="cursor-grab"
      @drag="drag($event, index)">
      <slot name="handle"></slot>
    </span>
    <slot name="default"></slot>
  </div>
</template>

<script>
let dragging;
let draggedOver;

export default {
  name: 'ReorderList',
  props: {
    list: {
      type: Array,
      required: true
    },
    index: {
      type: Number,
      required: true
    }
  },
  methods: {
    drag (e, index) {
      dragging = index; // index of the field being dragged
    },
    dragOver (e, index) {
      draggedOver = index; // index of the field that is being dragged over
    },
    drop (e) {
      const clone = [...this.list];
      // remove the dragged field from the list
      const draggedField = clone.splice(dragging, 1)[0];
      // and replace it in the new position
      clone.splice(draggedOver, 0, draggedField);
      // update the parent
      this.$emit('update', { list: clone });
    }
  }
};
</script>
