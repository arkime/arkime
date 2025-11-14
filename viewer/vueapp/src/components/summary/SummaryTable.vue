<template>
  <BTable
    small
    striped
    must-sort
    :items="data"
    :fields="tableFields"
    class="summary-table">
    <!-- Custom cell rendering for columns with useSessionField -->
    <template
      v-for="column in columns"
      :key="`cell-${column.key}`"
      #[`cell(${column.key})`]="{ item }">
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
  </BTable>
</template>

<script setup>
import { computed } from 'vue';
import { BTable } from 'bootstrap-vue-next';
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

// Transform columns to BTable fields format
const tableFields = computed(() => {
  return props.columns.map(column => ({
    key: column.key,
    label: column.header,
    sortable: true, // Enable sorting for all columns
    thClass: column.align ? `text-${column.align}` : '',
    tdClass: column.align ? `text-${column.align}` : ''
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
