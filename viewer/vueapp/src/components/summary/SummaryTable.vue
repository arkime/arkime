<template>
  <v-data-table
    density="compact"
    must-sort
    :items="data"
    :headers="tableHeaders"
    :items-per-page="-1"
    hide-default-footer
    class="summary-table summary-table-striped">
    <!-- Custom cell rendering for columns with useSessionField -->
    <template
      v-for="column in columns"
      :key="`cell-${column.key}`"
      #[`item.${column.key}`]="{ item }">
      <!-- ArkimeSessionField for columns with fieldConfig -->
      <arkime-session-field
        v-if="column.useSessionField && fieldConfig"
        :field="fieldConfig"
        :value="item[column.key]"
        :expr="column.expr || fieldConfig.exp"
        :parse="true"
        :session-btn="true"
        :pull-left="true" />
      <!-- Formatted value -->
      <template v-else>
        {{ formatValue(item[column.key], column.format) }}
      </template>
    </template>
  </v-data-table>
</template>

<script setup>
import { computed } from 'vue';
import ArkimeSessionField from '../sessions/SessionField.vue';
import { commaString, humanReadableBytes } from '@common/vueFilters.js';

const props = defineProps({
  data: {
    type: Array,
    required: true
  },
  columns: {
    type: Array,
    required: true,
    validator: (columns) => {
      return columns.every(col => col.key && col.header);
    }
  },
  fieldConfig: {
    type: Object,
    default: null
  }
});

// Map Bootstrap-style alignment to Vuetify's align values
const alignMap = { left: 'start', right: 'end', center: 'center' };

// Transform columns to v-data-table headers format
const tableHeaders = computed(() => {
  return props.columns.map(column => ({
    key: column.key,
    title: column.header,
    sortable: true,
    align: column.align ? alignMap[column.align] : 'start'
  }));
});

const formatValue = (value, format) => {
  if (!format) return value;

  switch (format) {
  case 'number':
    return commaString(value || 0);
  case 'bytes':
    return humanReadableBytes(value || 0);
  default:
    return value;
  }
};
</script>

<style scoped>
.summary-table-striped :deep(tbody tr:nth-of-type(odd) > td) {
  background-color: rgb(var(--v-theme-neutral-lighter)) !important;
}
</style>
