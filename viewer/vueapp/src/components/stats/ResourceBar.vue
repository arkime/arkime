<!--
  Shared color-coded resource meter (issue #4085).
  Give it a `percent` (0-100) OR `value` + `max`, plus a display `label`.
  Fill length tracks the percent; color comes from the fixed thresholds in
  resourceColor.js. Set `invert` for "free" metrics where low = bad.
-->
<template>
  <v-progress-linear
    v-if="isValid"
    class="resource-bar"
    :model-value="pct"
    :color="color"
    bg-color="progress-track"
    height="18"
    rounded>
    <span class="resource-bar-label">{{ label }}</span>
  </v-progress-linear>
  <span
    v-else
    class="resource-bar-text">{{ label }}</span>
</template>

<script setup>
import { computed } from 'vue';
import { resourceColor } from './resourceColor.js';

const props = defineProps({
  percent: { type: Number, default: null }, // precomputed 0-100
  value: { type: Number, default: null }, // used with max when percent is absent
  max: { type: Number, default: null },
  label: { type: String, default: '' }, // overlay text, formatted by the parent
  invert: { type: Boolean, default: false }, // free metric: low value = bad
  color: { type: String, default: null } // theme token override; else from thresholds
});

const rawPercent = computed(() => {
  if (props.percent !== null && props.percent !== undefined) { return Number(props.percent); }
  if (props.max) { return (Number(props.value) / Number(props.max)) * 100; }
  return NaN;
});

const isValid = computed(() => isFinite(rawPercent.value));

const pct = computed(() => Math.max(0, Math.min(100, rawPercent.value)));

const color = computed(() => props.color || resourceColor(rawPercent.value, props.invert));
</script>

<style scoped>
.resource-bar {
  min-width: 60px;
  border-radius: 4px;
}

/* halo keeps the label legible over any bar color */
.resource-bar-label {
  font-size: 0.72rem;
  font-weight: 600;
  white-space: nowrap;
  color: rgb(var(--v-theme-foreground));
  text-shadow:
    0 0 2px rgb(var(--v-theme-background)),
    0 0 2px rgb(var(--v-theme-background));
}

.resource-bar-text {
  font-size: 0.75rem;
}
</style>
