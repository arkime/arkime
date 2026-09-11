<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Field widget: the search timeline. Self-fetches its own graph (global search AND
the widget's local filter) so it can plot any numeric field's metric and apply a
per-widget View/expression. The plotted series follows the widget's metric:
'sessions' -> sessionsHisto, else the field's <dbField>Histo. Drag-select emits a
new time range to the host.
-->
<template>
  <WidgetCard
    :title="displayTitle"
    :loading="loading"
    :error="error"
    :has-data="hasData"
    :info-items="infoItems"
    @edit="$emit('edit')"
    @remove="$emit('remove')"
    @retry="fetchData">
    <div class="timeline-widget">
      <div class="timeline-widget__plot">
        <TimelineGraph
          :graph-data="graphData"
          :graph-type="graphType"
          series-type="bars"
          y-scale="linear"
          fit-height
          :timeline-data-filters="timelineDataFilters"
          :timezone="timezone"
          @update-time-range="$emit('time-range', $event)" />
      </div>
    </div>
  </WidgetCard>
</template>

<script setup>
import { ref, computed } from 'vue';
import { useStore } from 'vuex';
import { useI18n } from 'vue-i18n';
import WidgetCard from './WidgetCard.vue';
import TimelineGraph from '../../visualizations/TimelineGraph.vue';
import { useSpigraphWidget } from './useSpigraphWidget';
import { metricHistoKey, fetchGraph } from './widgetData';

const { t } = useI18n();
const store = useStore();

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  timelineDataFilters: { type: Array, default: () => [] },
  infoItems: { type: Array, default: () => [] }
});

defineEmits(['edit', 'remove', 'time-range']);

const graphData = ref(null);

// self-fetch the graph; re-runs on metric/expression/view/time change (the
// useSpigraphWidget dependency key) so a field change updates without a refresh
const { loading, error, fetchData } = useSpigraphWidget(
  () => props.widget,
  () => props.reloadNonce,
  (res) => { graphData.value = res.graph || null; },
  fetchGraph
);

const hasData = computed(() => !!graphData.value);
const displayTitle = computed(() => props.widget.title || t('sessions.summary.timelineView'));
const timezone = computed(() => store.state.user?.settings?.timezone || 'local');

// series to plot follows the widget's metric: sessions -> sessionsHisto, else
// the numeric field's <dbField>Histo (the graph fetch requests it via `metric`)
const graphType = computed(() => metricHistoKey({ metricType: props.widget.metricType }));
</script>

<style scoped>
.timeline-widget {
  display: flex;
  flex-direction: column;
  flex: 1;
  min-height: 0;
}
/* fill the card: the plot grows/shrinks with the widget's configured row span
   (TimelineGraph reads this height via fit-height) */
.timeline-widget__plot {
  flex: 1;
  min-height: 0;
}
</style>
