<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Time of day widget: the search's traffic as a week of hours (weekday by hour,
colored by the widget's metric), or one small grid per top value of a split
field. Self-fetches the hour by hour timeline from /api/spigraph (global
search AND the widget's local filter) and folds it client side.
-->
<template>
  <WidgetCard
    :title="displayTitle"
    :loading="loading"
    :error="error"
    :has-data="hasData"
    :info-items="infoItems"
    :link="link"
    :graph="graphDef"
    @edit="$emit('edit')"
    @remove="$emit('remove')"
    @retry="fetchData">
    <TimeOfDayChart
      v-if="hasData && !split"
      :graph="graph"
      :metric="graphType"
      :metric-label="metricLabel"
      :timezone="timezone"
      :color-scheme="colorScheme" />
    <div
      v-else-if="hasData"
      class="tod-grid">
      <div
        v-for="item in items"
        :key="item.name"
        class="tod-grid__cell">
        <div class="tod-grid__title">
          <arkime-session-field
            v-if="splitFieldObj"
            :field="splitFieldObj"
            :value="item.name"
            :expr="splitFieldObj.exp"
            :parse="true"
            :session-btn="true"
            :pull-left="true" />
        </div>
        <div class="tod-grid__chart">
          <TimeOfDayChart
            :graph="item.graph || {}"
            :metric="graphType"
            :metric-label="metricLabel"
            :timezone="timezone"
            :color-scheme="colorScheme"
            compact />
        </div>
      </div>
    </div>
  </WidgetCard>
</template>

<script setup>
import { ref, computed } from 'vue';
import { useStore } from 'vuex';
import { useI18n } from 'vue-i18n';
import WidgetCard from './WidgetCard.vue';
import TimeOfDayChart from '../../visualizations/TimeOfDayChart.vue';
import ArkimeSessionField from '../../sessions/SessionField.vue';
import FieldService from '../../search/FieldService';
import { useSpigraphWidget } from './useSpigraphWidget';
import { metricHistoKey, fetchTimeOfDay, metricLabel as metricLabelFor } from './widgetData';

const { t } = useI18n();
const store = useStore();

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  colorScheme: { type: String, default: 'rainbow' },
  infoItems: { type: Array, default: () => [] },
  link: { type: Object, default: null },
  graphDef: { type: Object, default: null }
});

defineEmits(['edit', 'remove']);

const graph = ref(null);
const items = ref([]);
const split = computed(() => !!props.widget.splitField);

const { loading, error, fetchData } = useSpigraphWidget(
  () => props.widget,
  () => props.reloadNonce,
  (res) => {
    graph.value = res.graph || null;
    items.value = split.value ? (res.items || []) : [];
  },
  fetchTimeOfDay
);

// the histo series to fold: sessions, or the metric field's <dbField>Histo
const graphType = computed(() => metricHistoKey({ metricType: props.widget.metricType }));
const metricLabel = computed(() => metricLabelFor(props.widget.metricType || 'sessions', 'sum', t));
const hasData = computed(() => !!graph.value && (!split.value || items.value.length > 0));
const splitFieldObj = computed(() => (props.widget.splitField ? FieldService.getField(props.widget.splitField, true) : null));
const displayTitle = computed(() => props.widget.title ||
  (split.value ? `${t('sessions.summary.timeofdayView')} / ${splitFieldObj.value?.friendlyName || props.widget.splitField}` : t('sessions.summary.timeofdayView')));
const timezone = computed(() => store.state.user?.settings?.timezone || 'local');
</script>

<style scoped>
.tod-grid {
  flex: 1;
  min-height: 0;
  overflow: auto;
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(220px, 1fr));
  grid-auto-rows: 150px;
  gap: 0.4rem;
}
.tod-grid__cell {
  display: flex;
  flex-direction: column;
  min-width: 0;
  min-height: 0;
}
.tod-grid__title {
  font-size: 0.75rem;
  font-weight: 600;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.tod-grid__chart {
  flex: 1;
  min-height: 0;
}
</style>
