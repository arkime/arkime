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
    class="arkime-drag-badge me-1"
    :class="{dragging:dragging > -1}">
    {{ field.friendlyName }}
    <button
      :aria-label="$t('common.remove')"
      type="button"
      class="arkime-drag-close ms-1"
      @click.stop.prevent="$emit('remove', index)">
      <v-icon icon="mdi-close" />
    </button>
  </label>
  <div
    @dragover.prevent
    @dragenter.prevent
    @drop="drop($event)"
    class="invisible-drop" />
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
/* draggable field chip: themed pill with inline close button. */
.arkime-drag-badge {
  display: inline-flex;
  align-items: center;
  padding: 0.45rem 0.5rem;
  font-size: 0.875rem;
  font-weight: 600;
  line-height: 1;
  color: rgb(var(--v-theme-button-fg));
  background-color: rgb(var(--v-theme-neutral));
  border-radius: 0.375rem;
  white-space: nowrap;
  vertical-align: baseline;
}
.arkime-drag-badge:hover {
  cursor: grab;
}
.arkime-drag-badge:active, .arkime-drag-badge.grabbing {
  cursor: grabbing !important;
}
/* close button inside the chip: bare button with a hover tint */
.arkime-drag-close {
  background: transparent;
  border: none;
  padding: 0;
  margin: 0;
  cursor: pointer;
  color: inherit;
  font-size: 0.75rem;
  line-height: 1;
  opacity: 0.7;
  transition: opacity 0.15s ease;
}
.arkime-drag-close:hover {
  opacity: 1;
}

div.invisible-drop {
  width: 40px;
  height: 30px;
  margin-left: -.5rem;
  margin-bottom: -10px;
  display: inline-flex;
}
</style>
