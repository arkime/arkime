<template>
  <span>
    <label
      draggable
      v-for="(field, index) in list"
      :key="index"
      @drop="drop($event)"
      @drag="drag($event, index)"
      @dragover.prevent="dragOver($event, index)"
      @click.stop.prevent="doNothing"
      class="badge badge-secondary mr-1 mb-1"
      :class="{dragging:dragging > -1}">
      {{ field.friendlyName }}
      <button
        class="close ml-2"
        @click.stop.prevent="removeField(index)">
        <span>&times;</span>
      </button>
    </label>
    <div
      @dragover.prevent
      @dragenter.prevent
      @drop="drop($event)"
      class="invisible-drop">
    </div>
  </span>
</template>

<script>
export default {
  name: 'DragList',
  props: {
    list: Array
  },
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
    drop (e, index) {
      const listClone = [...this.list];
      // remove the dragged field from the list
      const draggedField = listClone.splice(this.dragging, 1)[0];
      // and replace it in the new position
      listClone.splice(this.draggedOver, 0, draggedField);
      this.$emit('reorder', listClone);
      this.dragging = undefined;
    },
    removeField (index) {
      this.$emit('remove', index);
    }
  }
};
</script>

<style scoped>
label {
  display: inline-flex;
}
label:hover {
  cursor: grab;
}
label:active, label.grabbing {
  cursor: grabbing !important;
}

button {
  margin-top: -6px
}

div.invisible-drop {
  width: 40px;
  height: 30px;
  margin-left: -.5rem;
  margin-bottom: -10px;
  display: inline-flex;
}
</style>
