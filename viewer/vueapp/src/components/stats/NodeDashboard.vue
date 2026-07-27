<!--
  Per-node gauge-card dashboard (issue #4085) — a responsive grid of node cards,
  each with a worst-gauge status dot and one ResourceBar per metric.
  Data-driven: the parent passes the node rows and a `gauges` descriptor, so the
  same component serves both Capture Stats and ES Nodes.
-->
<template>
  <div class="node-dashboard">
    <div
      v-if="!data || !data.length"
      class="text-center text-medium-emphasis py-6">
      <v-icon
        icon="mdi-server-off"
        class="me-1" />{{ noResultsMsg }}
    </div>
    <div
      v-else
      class="node-grid">
      <div
        v-for="(node, index) in sortedData"
        :key="nodeLabel(node) + index"
        class="node-card">
        <div class="node-card-header">
          <span
            class="node-status-dot"
            :style="{ backgroundColor: dotColor(node) }" />
          <span
            class="node-name"
            :title="nodeLabel(node)">{{ nodeLabel(node) }}</span>
          <span
            v-if="badge"
            class="node-badge-chip">{{ badge(node) }}</span>
          <span
            v-if="statusText"
            class="node-status-text"
            :style="{ color: dotColor(node) }">{{ statusText(node) }}</span>
        </div>
        <div class="node-gauges">
          <div
            v-for="gauge in gauges"
            :key="gauge.title"
            class="node-gauge"
            :class="{ 'node-gauge--value': (gauge.kind || 'bar') === 'value' }">
            <span class="node-gauge-title">{{ gauge.title }}</span>
            <resource-bar
              v-if="(gauge.kind || 'bar') === 'bar'"
              :percent="numeric(gauge.percent(node))"
              :invert="gauge.invert"
              :color="gauge.color ? gauge.color(node) : null"
              :label="gauge.label(node)" />
            <span
              v-else
              class="node-gauge-value"
              :style="gauge.color ? { color: `rgb(var(--v-theme-${gauge.color(node)}))` } : null">{{ gauge.text(node) }}</span>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue';
import ResourceBar from './ResourceBar.vue';
import { worstResourceColor } from './resourceColor.js';

const props = defineProps({
  data: { type: Array, default: () => [] },
  // [{ title, percent: (node) => Number, label: (node) => String, invert }]
  gauges: { type: Array, default: () => [] },
  nodeName: { type: Function, default: null },
  // optional (node) => 'success'|'warning'|'error' for the card status dot;
  // falls back to the worst gauge color when not provided
  status: { type: Function, default: null },
  // optional (node) => String shown next to the node name (e.g. "capturing")
  statusText: { type: Function, default: null },
  // optional (node) => String rendered as a small chip in the header (e.g. version)
  badge: { type: Function, default: null },
  noResultsMsg: { type: String, default: '' }
});

const nodeLabel = (node) => props.nodeName ? props.nodeName(node) : (node.nodeName || node.name || '');

// cards are sorted by node name (natural order so foo-2 sorts before foo-15)
const sortedData = computed(() =>
  (props.data || []).slice().sort((a, b) => nodeLabel(a).localeCompare(nodeLabel(b), undefined, { numeric: true }))
);

const numeric = (v) => {
  const n = Number(v);
  return isFinite(n) ? n : null;
};

const dotColor = (node) => {
  const token = props.status
    ? props.status(node)
    : worstResourceColor(props.gauges.map(g => ({ percent: numeric(g.percent(node)), invert: g.invert })));
  return `rgb(var(--v-theme-${token}))`;
};
</script>

<style scoped>
.node-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(240px, 1fr));
  gap: 0.75rem;
  padding: 0.5rem 0;
}
.node-card {
  padding: 0.6rem 0.75rem;
  border-radius: 8px;
  background: rgb(var(--v-theme-quaternary-lightest));
  border: 1px solid rgba(var(--v-theme-foreground), 0.08);
}
.node-card-header {
  display: flex;
  align-items: center;
  gap: 0.4rem;
  margin-bottom: 0.5rem;
  font-weight: 600;
}
.node-status-dot {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  flex: 0 0 auto;
}
.node-name {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.node-badge-chip {
  flex: 0 0 auto;
  font-size: 0.6rem;
  font-weight: 600;
  padding: 0.05rem 0.35rem;
  border-radius: 4px;
  background: rgba(var(--v-theme-foreground), 0.08);
  opacity: 0.85;
}
.node-status-text {
  margin-left: auto;
  flex: 0 0 auto;
  font-size: 0.62rem;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.03em;
}
.node-gauges {
  display: flex;
  flex-direction: column;
  gap: 0.4rem;
}
.node-gauge {
  display: flex;
  flex-direction: column;
  gap: 0.1rem;
}
.node-gauge--value {
  flex-direction: row;
  align-items: baseline;
  justify-content: space-between;
  gap: 0.5rem;
}
.node-gauge-title {
  font-size: 0.68rem;
  text-transform: uppercase;
  letter-spacing: 0.03em;
  opacity: 0.75;
}
.node-gauge-value {
  font-size: 0.85rem;
  font-weight: 600;
  white-space: nowrap;
}
</style>
