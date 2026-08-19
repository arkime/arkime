<!--
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0
-->
<!--
  Three-pane Wireshark-style container: packet list, dissection tree and hex
  bytes. Every layout is two nested splits, so a layout is just the pair of
  directions used by the outer and inner split.
-->
<template>
  <div
    ref="rootRef"
    class="shark-panes-root"
    :style="{ height: `${height}px` }">
    <div
      ref="outerRef"
      class="shark-split shark-split--body"
      :class="`shark-split--${outerDir}`">
      <div
        class="shark-pane"
        :style="basis(sizes[0])">
        <slot name="list" />
      </div>
      <div
        class="shark-handle"
        :class="`shark-handle--${outerDir}`"
        title="Drag to resize"
        @mousedown="startDrag(0, $event)"
        @dblclick="reset(0)" />
      <div
        ref="innerRef"
        class="shark-pane shark-split"
        :class="`shark-split--${innerDir}`">
        <div
          class="shark-pane"
          :style="basis(sizes[1])">
          <slot name="fields" />
        </div>
        <div
          class="shark-handle"
          :class="`shark-handle--${innerDir}`"
          title="Drag to resize"
          @mousedown="startDrag(1, $event)"
          @dblclick="reset(1)" />
        <div class="shark-pane">
          <slot name="bytes" />
        </div>
      </div>
    </div>
    <div
      class="shark-handle shark-handle--col shark-handle--height"
      title="Drag to resize the whole area"
      @mousedown="startHeightDrag"
      @dblclick="resetHeight" />
  </div>
</template>

<script setup>
import { computed, ref } from 'vue';
import { SHARK_LAYOUTS, SHARK_DEFAULT_SIZES, SHARK_DEFAULT_HEIGHT } from './sharkLayouts.js';

const props = defineProps({
  layout: { type: String, default: 'wireshark' },
  // [outer split fraction, inner split fraction], each 0..1
  sizes: { type: Array, default: () => SHARK_DEFAULT_SIZES.slice() },
  height: { type: Number, default: 520 }
});
const emit = defineEmits(['update:sizes', 'update:height']);

const outerRef = ref(null);
const innerRef = ref(null);

const dirs = computed(() => (SHARK_LAYOUTS[props.layout] || SHARK_LAYOUTS.wireshark).dirs);
const outerDir = computed(() => dirs.value[0]);
const innerDir = computed(() => dirs.value[1]);

const basis = (f) => ({ flex: `0 0 ${(f * 100).toFixed(2)}%` });

const MIN_FRACTION = 0.1;
const MAX_FRACTION = 0.9;

const startDrag = (idx, e) => {
  e.preventDefault();
  const el = idx === 0 ? outerRef.value : innerRef.value;
  if (!el) { return; }
  const dir = idx === 0 ? outerDir.value : innerDir.value;
  const rect = el.getBoundingClientRect();
  const span = dir === 'col' ? rect.height : rect.width;
  if (span <= 0) { return; }

  const onMove = (ev) => {
    const offset = dir === 'col' ? ev.clientY - rect.top : ev.clientX - rect.left;
    const f = Math.max(MIN_FRACTION, Math.min(MAX_FRACTION, offset / span));
    const next = props.sizes.slice();
    next[idx] = f;
    emit('update:sizes', next);
  };
  const onUp = () => {
    window.removeEventListener('mousemove', onMove);
    window.removeEventListener('mouseup', onUp);
    document.body.style.userSelect = '';
    document.body.style.cursor = '';
  };
  window.addEventListener('mousemove', onMove);
  window.addEventListener('mouseup', onUp);
  document.body.style.userSelect = 'none';
  document.body.style.cursor = dir === 'col' ? 'row-resize' : 'col-resize';
};

const reset = (idx) => {
  const next = props.sizes.slice();
  next[idx] = SHARK_DEFAULT_SIZES[idx];
  emit('update:sizes', next);
};

const MIN_HEIGHT = 240;
const MAX_HEIGHT = 4000;

// Grip along the bottom edge that grows/shrinks the whole three-pane area.
const startHeightDrag = (e) => {
  e.preventDefault();
  const startY = e.clientY;
  const startH = props.height;

  const onMove = (ev) => {
    const h = Math.round(Math.max(MIN_HEIGHT, Math.min(MAX_HEIGHT, startH + (ev.clientY - startY))));
    if (h !== props.height) { emit('update:height', h); }
  };
  const onUp = () => {
    window.removeEventListener('mousemove', onMove);
    window.removeEventListener('mouseup', onUp);
    document.body.style.userSelect = '';
    document.body.style.cursor = '';
  };
  window.addEventListener('mousemove', onMove);
  window.addEventListener('mouseup', onUp);
  document.body.style.userSelect = 'none';
  document.body.style.cursor = 'row-resize';
};

const resetHeight = () => emit('update:height', SHARK_DEFAULT_HEIGHT);
</script>

<style scoped>
.shark-panes-root {
  display: flex;
  flex-direction: column;
  border: 1px solid rgb(var(--v-theme-neutral));
  border-radius: 4px;
  overflow: hidden;
  min-height: 240px;
}
.shark-split--body {
  flex: 1 1 auto;
}
.shark-split {
  /* always a flex child of either the root column or the outer split */
  display: flex;
  min-height: 0;
  min-width: 0;
}
.shark-split--col {
  flex-direction: column;
}
.shark-split--row {
  flex-direction: row;
}
.shark-pane {
  flex: 1 1 0;
  min-height: 0;
  min-width: 0;
  overflow: auto;
}
/* the nested split is itself a pane and must not scroll -- its children do */
.shark-pane.shark-split {
  overflow: hidden;
}
.shark-handle {
  flex: 0 0 6px;
  background: rgb(var(--v-theme-neutral));
  position: relative;
  transition: background 0.15s;
}
.shark-handle--col {
  cursor: row-resize;
}
.shark-handle--row {
  cursor: col-resize;
}
.shark-handle::before {
  /* faint grip line hinting at the drag direction */
  content: '';
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  background: rgba(0, 0, 0, 0.35);
  box-shadow: 0 0 0 1px rgba(255, 255, 255, 0.2);
  border-radius: 1px;
}
.shark-handle--col::before {
  width: 24px;
  height: 2px;
}
.shark-handle--row::before {
  width: 2px;
  height: 24px;
}
.shark-handle:hover,
.shark-handle:active {
  background: rgb(var(--v-theme-primary));
}
.shark-handle--height {
  flex: 0 0 8px;
}
</style>
