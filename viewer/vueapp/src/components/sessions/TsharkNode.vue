<!--
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <li class="tshark-node">
    <details v-if="hasChildren">
      <summary>
        <span class="tshark-label">{{ displayLabel }}</span>
      </summary>
      <ul>
        <tshark-node
          v-for="(child, i) in node.fields"
          :key="i"
          :node="child" />
      </ul>
    </details>
    <span
      v-else
      class="tshark-leaf">
      <span class="tshark-label">{{ displayLabel }}</span>
    </span>
  </li>
</template>

<script setup>
import { computed } from 'vue';

const props = defineProps({
  node: { type: Object, required: true }
});

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
</script>

<style scoped>
.tshark-node {
  list-style: none;
}
.tshark-node ul {
  padding-left: 1.25em;
}
.tshark-label {
  font-family: var(--bs-font-monospace, monospace);
  font-size: 0.85rem;
}
</style>
