<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Field widget rendered in "heatmap" view mode: the field's top-N values, each as
a time series (a "timeline applied to a field"). Self-fetches /api/spigraph
(global search AND the widget's local filter) and renders ArkimeHeatmap.
-->
<template>
  <WidgetCard
    :title="title"
    :loading="loading"
    :error="error"
    :has-data="hasData"
    :info-items="infoItems"
    scroll
    @edit="$emit('edit')"
    @remove="$emit('remove')"
    @retry="fetchData">
    <ArkimeHeatmap
      v-if="hasData"
      :items="items"
      :graph="graph"
      :field-obj="fieldObj"
      metric="sessionsHisto"
      :timeline-data-filters="timelineDataFilters"
      :color-scheme="colorScheme"
      sort-by="graph" />
  </WidgetCard>
</template>

<script setup>
import { ref, computed } from 'vue';
import { useStore } from 'vuex';
import WidgetCard from './WidgetCard.vue';
import ArkimeHeatmap from '../../spigraph/Heatmap.vue';
import FieldService from '../../search/FieldService';
import { useSpigraphWidget } from './useSpigraphWidget';

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  colorScheme: { type: String, default: 'rainbow' },
  infoItems: { type: Array, default: () => [] }
});
defineEmits(['edit', 'remove']);

const store = useStore();

const items = ref([]);
const graph = ref(null);

const { loading, error, fetchData } = useSpigraphWidget(
  () => props.widget,
  () => props.reloadNonce,
  (res) => { items.value = res.items || []; graph.value = res.graph || null; }
);

const fieldObj = computed(() => FieldService.getField(props.widget.field, true));
const title = computed(() => props.widget.title || fieldObj.value?.friendlyName || props.widget.field);
const hasData = computed(() => !!graph.value && items.value.length > 0);

const timelineDataFilters = computed(() => {
  const filters = store.state.user?.settings?.timelineDataFilters;
  if (!filters) { return []; }
  return filters.map(i => FieldService.getField(i));
});
</script>
