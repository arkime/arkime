<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Table widget with 2-3 fields shown side by side: each selected field's independent
top-N values with the chosen metric (NOT combinations). Self-fetches one
/api/sessions/summary batch (a sub-widget per field) and renders a [Value | metric]
column pair per field, rows aligned by rank.
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
    <table
      v-if="hasData"
      class="mf-table">
      <thead>
        <tr>
          <template
            v-for="(fd, i) in fieldsData"
            :key="i">
            <th>
              {{ fd.friendlyName }}
              <v-icon
                v-if="fd.error"
                icon="mdi-alert"
                size="x-small"
                color="error"
                class="ms-1"
                :title="fd.error" />
            </th>
            <th class="num">
              {{ metricLabel }}
            </th>
          </template>
        </tr>
      </thead>
      <tbody>
        <tr
          v-for="r in maxRows"
          :key="r">
          <template
            v-for="(fd, i) in fieldsData"
            :key="i">
            <td>
              <arkime-session-field
                v-if="fd.data[r - 1] && fieldObjs[i]"
                :field="fieldObjs[i]"
                :value="fd.data[r - 1].item"
                :expr="fieldObjs[i].exp"
                :parse="true"
                :session-btn="true"
                :pull-left="true" />
            </td>
            <td class="num">
              {{ fd.data[r - 1] ? formatMetric(fd.data[r - 1].value) : '' }}
            </td>
          </template>
        </tr>
      </tbody>
    </table>
  </WidgetCard>
</template>

<script setup>
import { ref, computed } from 'vue';
import { useI18n } from 'vue-i18n';
import WidgetCard from './WidgetCard.vue';
import ArkimeSessionField from '../../sessions/SessionField.vue';
import FieldService from '../../search/FieldService';
import { useSpigraphWidget } from './useSpigraphWidget';
import { fetchSummaryFields, formatMetricValue } from './widgetData';

const { t } = useI18n();

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  infoItems: { type: Array, default: () => [] }
});

defineEmits(['edit', 'remove']);

const fieldsData = ref([]); // [{ exp, friendlyName, data: [{item, value, ...}] }]

const { loading, error, fetchData } = useSpigraphWidget(
  () => props.widget,
  () => props.reloadNonce,
  (res) => { fieldsData.value = Array.isArray(res) ? res : []; },
  fetchSummaryFields
);

const fieldObjs = computed(() => fieldsData.value.map(fd => FieldService.getField(fd.exp, true)));
const maxRows = computed(() => fieldsData.value.reduce((m, fd) => Math.max(m, fd.data?.length || 0), 0));
const hasData = computed(() => maxRows.value > 0);

const title = computed(() => props.widget.title ||
  fieldsData.value.map(fd => fd.friendlyName).join(' / ') ||
  props.widget.field);

// the selected metric's column label + formatting (shared across all fields)
const metricLabel = computed(() => {
  const m = props.widget.metricType || 'sessions';
  if (m === 'sessions') { return t('sessions.summary.sessions'); }
  return FieldService.getField(m, true)?.friendlyName || m;
});
const formatMetric = (v) => formatMetricValue(props.widget.metricType, v);
</script>

<style scoped>
.mf-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.85rem;
}
.mf-table th,
.mf-table td {
  padding: 2px 8px;
  text-align: left;
  white-space: nowrap;
}
.mf-table th {
  position: sticky;
  top: 0;
  background: rgb(var(--v-theme-quaternary-lightest));
  font-weight: 600;
  z-index: 1;
}
/* a faint divider between each field's column pair */
.mf-table th:nth-of-type(odd):not(:first-of-type),
.mf-table td:nth-of-type(odd):not(:first-of-type) {
  border-left: 1px solid rgb(var(--v-theme-neutral-lighter));
}
.mf-table .num {
  text-align: right;
}
.mf-table tbody tr:nth-of-type(odd) > td {
  background-color: rgb(var(--v-theme-neutral-lighter));
}
</style>
