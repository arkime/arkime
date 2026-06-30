<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Session-wide widget: the search timeline, fed by the host's global stats chunk
(graph histos). A small selector picks which series to show (sessions or one of
the user's timeline data filters); drag-select emits a new time range to the host.
-->
<template>
  <WidgetCard
    :title="displayTitle"
    :has-data="hasData"
    :info-items="infoItems"
    @edit="$emit('edit')"
    @remove="$emit('remove')">
    <div class="timeline-widget">
      <div class="timeline-widget__controls">
        <v-select
          v-model="graphType"
          :items="graphTypeItems"
          density="compact"
          variant="outlined"
          hide-details
          style="max-width: 240px" />
      </div>
      <div class="timeline-widget__plot">
        <TimelineGraph
          :graph-data="graphData"
          :graph-type="graphType"
          series-type="bars"
          y-scale="linear"
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

const { t } = useI18n();
const store = useStore();

const props = defineProps({
  graphData: { type: Object, default: () => null },
  timelineDataFilters: { type: Array, default: () => [] },
  title: { type: String, default: '' },
  infoItems: { type: Array, default: () => [] }
});

defineEmits(['edit', 'remove', 'time-range']);

const hasData = computed(() => !!props.graphData);
const displayTitle = computed(() => props.title || t('sessions.summary.timelineView'));
const timezone = computed(() => store.state.user?.settings?.timezone || 'local');

// which series to plot: sessions, or one of the user's timeline data filters
const graphType = ref('sessionsHisto');
const graphTypeItems = computed(() => [
  { title: t('common.sessions'), value: 'sessionsHisto' },
  ...props.timelineDataFilters.map(f => ({ title: f.friendlyName, value: `${f.dbField}Histo` }))
]);
</script>

<style scoped>
.timeline-widget {
  display: flex;
  flex-direction: column;
  flex: 1;
  min-height: 0;
}
.timeline-widget__controls {
  display: flex;
  justify-content: flex-end;
  margin-bottom: 4px;
}
.timeline-widget__plot {
  flex: 1;
  min-height: 200px;
}
</style>
