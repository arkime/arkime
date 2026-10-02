<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Field widget rendered as an "Intersection" table: the unique value combinations
across 1-3 fields with their session counts, self-fetched from
/api/spigraphhierarchy. One field → a flat value/count table; 2-3 fields → nested
combinations (a column pair per field level, grouped by parent). Count-based.
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
      class="intersection-table">
      <thead>
        <tr>
          <template
            v-for="(f, i) in fieldObjs"
            :key="i">
            <th>{{ f?.friendlyName || fieldExps[i] }}</th>
            <th class="num">
              {{ $t('sessions.summary.count') }}
            </th>
          </template>
        </tr>
      </thead>
      <tbody>
        <tr
          v-for="(row, r) in rows"
          :key="r">
          <template
            v-for="(cell, c) in row"
            :key="c">
            <td>
              <arkime-session-field
                v-if="fieldObjs[c]"
                :field="fieldObjs[c]"
                :value="cell.name"
                :expr="fieldObjs[c].exp"
                :parse="true"
                :session-btn="true"
                :pull-left="true" />
              <template v-else>
                {{ cell.name }}
              </template>
            </td>
            <td class="num">
              {{ formatNumber(cell.size) }}
            </td>
          </template>
        </tr>
      </tbody>
    </table>
  </WidgetCard>
</template>

<script setup>
import { ref, computed } from 'vue';
import WidgetCard from './WidgetCard.vue';
import ArkimeSessionField from '../../sessions/SessionField.vue';
import FieldService from '../../search/FieldService';
import { commaString } from '@common/vueFilters.js';
import { useSpigraphWidget } from './useSpigraphWidget';
import { fetchHierarchy, widgetFields } from './widgetData';

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  infoItems: { type: Array, default: () => [] }
});

defineEmits(['edit', 'remove']);

const tableResults = ref([]);

const { loading, error, fetchData } = useSpigraphWidget(
  () => props.widget,
  () => props.reloadNonce,
  (res) => { tableResults.value = res.tableResults || []; },
  fetchHierarchy
);

const fieldExps = computed(() => widgetFields(props.widget));
const fieldObjs = computed(() => fieldExps.value.map(exp => FieldService.getField(exp, true)));

const title = computed(() => props.widget.title ||
  fieldObjs.value.map((f, i) => f?.friendlyName || fieldExps.value[i]).join(' / '));

// each row = the ancestor chain (parents) plus the leaf, one {name,size} per field level
const rows = computed(() => tableResults.value.map(r => [...(r.parents || []), { name: r.name, size: r.size }]));

const hasData = computed(() => rows.value.length > 0);

const formatNumber = (n) => commaString(n || 0);
</script>

<style scoped>
.intersection-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.85rem;
}
.intersection-table th,
.intersection-table td {
  padding: 2px 8px;
  text-align: left;
  white-space: nowrap;
}
.intersection-table th {
  position: sticky;
  top: 0;
  background: rgb(var(--v-theme-quaternary-lightest));
  font-weight: 600;
  z-index: 1;
}
.intersection-table .num {
  text-align: right;
}
.intersection-table tbody tr:nth-of-type(odd) > td {
  background-color: rgb(var(--v-theme-neutral-lighter));
}
</style>
