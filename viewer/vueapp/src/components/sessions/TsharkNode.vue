<!--
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <li class="tshark-node">
    <details
      v-if="hasChildren"
      ref="detailsRef">
      <summary
        @click.alt.stop.prevent="copyValue">
        <span class="tshark-label">{{ displayLabel }}</span>
      </summary>
      <ul>
        <tshark-node
          v-for="(child, i) in node.fields"
          :key="i"
          :node="child"
          :expand-signal="expandSignal" />
      </ul>
    </details>
    <span
      v-else
      class="tshark-leaf"
      @click.alt.stop.prevent="copyValue">
      <span class="tshark-label">{{ displayLabel }}</span>
    </span>
  </li>
</template>

<script setup>
import { computed, ref, watch } from 'vue';

const props = defineProps({
  node: { type: Object, required: true },
  // monotonic counter; when it changes, force-open (>0) / force-close (<0).
  expandSignal: { type: Number, default: 0 }
});

const detailsRef = ref(null);

const hasChildren = computed(() => Array.isArray(props.node.fields) && props.node.fields.length > 0);

// PDML's `showname` is often "Label: value" already (e.g. "Frame Length: 66 bytes"),
// but for some protocols (geninfo) it's just a bare label. If `show` is present and
// the label doesn't already contain it, append ": value" for readability.
const displayLabel = computed(() => {
  const n = props.node;
  const base = n.label || n.name || '';
  const show = n.show;
  if (show === undefined || show === '' || show === null) { return base; }
  if (base.includes(':')) { return base; }
  return `${base}: ${show}`;
});

const copyValue = () => {
  const text = props.node.show ?? props.node.value ?? props.node.label ?? '';
  if (!text) { return; }
  if (navigator.clipboard?.writeText) {
    navigator.clipboard.writeText(String(text));
  }
};

watch(() => props.expandSignal, (v) => {
  if (!detailsRef.value || v === 0) { return; }
  detailsRef.value.open = v > 0;
}, { flush: 'post' });
</script>

<style scoped>
.tshark-node {
  list-style: none;
}
.tshark-node ul {
  padding-left: 1.25em;
}
.tshark-label {
  font-family: SFMono-Regular, Menlo, Monaco, Consolas, monospace;
  font-size: 0.85rem;
}
.tshark-leaf {
  display: inline-flex;
  align-items: center;
  gap: 0;
}
</style>
