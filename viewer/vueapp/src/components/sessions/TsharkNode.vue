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
        class="shark-tree-row"
        :class="[{ 'tshark-selected': isSelected }, expertClass]"
        @click="onClick">
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
      ref="rowRef"
      class="tshark-leaf shark-tree-row"
      :class="[{ 'tshark-selected': isSelected }, expertClass]"
      @click="onClick">
      <span class="tshark-label">{{ displayLabel }}</span>
    </span>
  </li>
</template>

<script setup>
import { computed, inject, nextTick, ref, watch, watchEffect } from 'vue';

const props = defineProps({
  node: { type: Object, required: true },
  // monotonic counter; when it changes, force-open (>0) / force-close (<0).
  expandSignal: { type: Number, default: 0 }
});

const detailsRef = ref(null);
const rowRef = ref(null);

// provided by SessionDetail so the hex pane can follow the tree selection
const selection = inject('sharkSelection', null);

// keyboard navigation walks the rendered rows, so each one carries its node
watchEffect(() => { if (rowRef.value) { rowRef.value.__sharkNode = props.node; } });

const hasChildren = computed(() => Array.isArray(props.node.fields) && props.node.fields.length > 0);

const isSelected = computed(() => selection?.node === props.node);

// Wireshark colors fields carrying expert info by severity (its "Severity level: Error" etc).
// The field that raised it and its ancestor layers get the color.
const EXPERT_SEVERITIES = { 0x00100000: 'chat', 0x00400000: 'note', 0x00600000: 'warn', 0x00800000: 'error' };
const EXPERT_LABELS = { chat: 'chat', note: 'note', warning: 'warn', warn: 'warn', error: 'error' };

const expertSeverity = (n) => {
  const sev = n.fields?.find(f => f.name === '_ws.expert.severity');
  if (sev) { return EXPERT_SEVERITIES[parseInt(sev.show)]; }
  return EXPERT_LABELS[/\((\w+)\//.exec(n.label || '')?.[1]?.toLowerCase()];
};

const SEVERITY_RANK = { chat: 1, note: 2, warn: 3, error: 4 };

const ownSeverity = (n) => {
  const expert = n.name === '_ws.expert' ? n : n.fields?.find(f => f.name === '_ws.expert');
  return expert && expertSeverity(expert);
};

// worst severity at or below a node, so the field and every ancestor layer carry the color
const worstSeverity = (n) => {
  let worst = ownSeverity(n);
  for (const c of n.fields || []) {
    const sev = worstSeverity(c);
    if (sev && (!worst || SEVERITY_RANK[sev] > SEVERITY_RANK[worst])) { worst = sev; }
  }
  return worst;
};

// the "Expert Info (...)" node itself is only shaded grey, like Wireshark
const expertClass = computed(() => {
  if (props.node.name === '_ws.expert') { return 'tshark-expert-info'; }
  const sev = worstSeverity(props.node);
  return sev ? `tshark-expert-${sev}` : '';
});

// alt-click copies the value (and must not toggle the <details>), plain click selects
const onClick = (e) => {
  if (e.altKey) {
    e.preventDefault();
    e.stopPropagation();
    copyValue();
    return;
  }
  if (selection) { selection.node = props.node; }
};

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

// Selecting a byte in the hex pane picks a field that may be buried in
// collapsed nodes. `selection.path` holds every node above it, so each ancestor
// opens itself and the field itself scrolls into view. Parents run before
// children, so the row is visible by the time it asks to be scrolled to.
watch(() => selection?.node, (node) => {
  if (!node) { return; }
  if (detailsRef.value && selection.path?.has(props.node)) { detailsRef.value.open = true; }
  if (node === props.node) {
    nextTick(() => rowRef.value?.scrollIntoView?.({ block: 'nearest' }));
  }
}, { flush: 'post' });

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
  cursor: pointer;
}
.tshark-expert-error { background: rgba(var(--v-theme-error), 0.45); border-radius: 2px; }
.tshark-expert-warn { background: rgba(var(--v-theme-warning), 0.45); border-radius: 2px; }
.tshark-expert-note { background: rgba(var(--v-theme-info), 0.45); border-radius: 2px; }
.tshark-expert-chat { background: rgba(var(--v-theme-primary), 0.3); border-radius: 2px; }
.tshark-expert-info { background: rgba(var(--v-theme-on-surface), 0.08); border-radius: 2px; }
.tshark-selected {
  background: rgb(var(--v-theme-primary));
  color: rgb(var(--v-theme-on-primary));
  border-radius: 2px;
}
.tshark-selected .tshark-label {
  color: inherit;
}
</style>
