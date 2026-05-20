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
        ref="rowRef"
        @click.alt.stop.prevent="copyValue">
        <span class="tshark-label">{{ displayLabel }}</span>
        <span
          v-if="byteRange"
          class="tshark-range">[{{ byteRange }}]</span>
        <v-icon
          v-if="copyable"
          icon="mdi-content-copy"
          size="x-small"
          class="tshark-copy"
          @click.stop.prevent="copyValue" />
      </summary>
      <v-tooltip
        v-if="rowRef && tooltipBody"
        :activator="rowRef"
        location="bottom start"
        :open-delay="200">
        <pre class="tshark-tooltip-body">{{ tooltipBody }}</pre>
      </v-tooltip>
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
      ref="rowRef"
      class="tshark-leaf"
      @click.alt.stop.prevent="copyValue">
      <span class="tshark-label">{{ displayLabel }}</span>
      <span
        v-if="byteRange"
        class="tshark-range">[{{ byteRange }}]</span>
      <v-icon
        v-if="copyable"
        icon="mdi-content-copy"
        size="x-small"
        class="tshark-copy"
        @click.stop.prevent="copyValue" />
      <v-tooltip
        v-if="rowRef && tooltipBody"
        :activator="rowRef"
        location="bottom start"
        :open-delay="200">
        <pre class="tshark-tooltip-body">{{ tooltipBody }}</pre>
      </v-tooltip>
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
const rowRef = ref(null);

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

const byteRange = computed(() => {
  const { pos, size } = props.node;
  if (pos === undefined || size === undefined || size === 0) { return ''; }
  return `${pos}+${size}`;
});

const copyable = computed(() => {
  const { show, value } = props.node;
  return (show !== undefined && show !== '') || (value !== undefined && value !== '');
});

// Format raw hex into readable groups of 2 bytes, wrapping every 32 bytes (64 hex chars).
const formatHex = (hex) => {
  if (!hex) { return ''; }
  const clean = String(hex).replace(/[^0-9a-f]/gi, '');
  const groups = clean.match(/.{1,2}/g) || [];
  const lines = [];
  for (let i = 0; i < groups.length; i += 16) {
    lines.push(groups.slice(i, i + 16).join(' '));
  }
  return lines.join('\n');
};

const tooltipBody = computed(() => {
  const { value, pos, size, name: nodeName, show } = props.node;
  if (pos === undefined && size === undefined && !value) { return ''; }
  const lines = [];
  if (nodeName) { lines.push(nodeName); }
  if (pos !== undefined && size !== undefined) {
    lines.push(`offset ${pos}, ${size} byte${size === 1 ? '' : 's'}`);
  }
  if (show !== undefined && show !== '') { lines.push(`value: ${show}`); }
  if (value) { lines.push(''); lines.push(formatHex(value)); }
  return lines.join('\n');
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
/* Hover-only: byte range + copy icon both fade in when the row is hovered. */
.tshark-range,
.tshark-copy {
  opacity: 0;
  transition: opacity 0.1s;
}
.tshark-range {
  margin-left: 0.5em;
  font-family: SFMono-Regular, Menlo, Monaco, Consolas, monospace;
  font-size: 0.7rem;
  color: rgb(var(--v-theme-foreground));
}
.tshark-copy {
  margin-left: 0.4em;
  cursor: pointer;
}
.tshark-node summary:hover .tshark-range,
.tshark-leaf:hover .tshark-range {
  opacity: 0.55;
}
.tshark-node summary:hover .tshark-copy,
.tshark-leaf:hover .tshark-copy {
  opacity: 0.7;
}
.tshark-copy:hover {
  opacity: 1 !important;
}
.tshark-leaf {
  display: inline-flex;
  align-items: center;
  gap: 0;
}
.tshark-tooltip-body {
  margin: 0;
  font-family: SFMono-Regular, Menlo, Monaco, Consolas, monospace;
  font-size: 0.75rem;
  white-space: pre;
  line-height: 1.35;
}
</style>
