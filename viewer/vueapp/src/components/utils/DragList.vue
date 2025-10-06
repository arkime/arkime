<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <label
    :draggable="true"
    v-for="(field, index) in list"
    :key="index"
    @drop="drop($event)"
    @drag="drag($event, index)"
    @dragover.prevent="dragOver($event, index)"
    @click.stop.prevent="doNothing"
    class="badge bg-secondary me-1 mt-1"
    :class="{dragging:dragging > -1}"
  >
    {{ field.friendlyName }}
    <button
      class="btn-close btn-sm ms-1"
      @click.stop.prevent="$emit('remove', index)"
    />
  </label>
  <div
    @dragover.prevent
    @dragenter.prevent
    @drop="drop($event)"
    class="invisible-drop"
  />
</template>

<script>
export default {
  name: 'DragList',
  props: {
    list: {
      type: Array,
      default: () => []
    }
  },
  emits: ['remove', 'reorder'],
  data () {
    return {
      dragging: -1,
      draggedOver: undefined
    };
  },
  methods: {
    // need this so removeField doesn't get called when clicking label
    doNothing () {},
    drag (e, index) {
      this.dragging = index; // index of the field being dragged
    },
    dragOver (e, index) {
      this.draggedOver = index; // index of the field that is being dragged over
    },
    drop (e) {
      const listClone = [...this.list];
      // remove the dragged field from the list
      const draggedField = listClone.splice(this.dragging, 1)[0];
      // and replace it in the new position
      listClone.splice(this.draggedOver, 0, draggedField);
      this.$emit('reorder', listClone);
      this.dragging = undefined;
    }
  }
};
</script>

<style scoped>
label:hover {
  cursor: grab;
}
label:active, label.grabbing {
  cursor: grabbing !important;
}

div.invisible-drop {
  width: 40px;
  height: 30px;
  margin-left: -.5rem;
  margin-bottom: -10px;
  display: inline-flex;
}
</style>
