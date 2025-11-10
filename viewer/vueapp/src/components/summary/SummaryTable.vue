<template>
  <table class="table table-sm table-striped">
    <thead>
      <tr>
        <th
          v-for="(column, index) in columns"
          :key="index"
          :class="column.align ? `text-${column.align}` : ''">
          {{ column.header }}
        </th>
      </tr>
    </thead>
    <tbody>
      <tr
        v-for="(item, rowIndex) in data"
        :key="rowIndex">
        <td
          v-for="(column, colIndex) in columns"
          :key="colIndex"
          :class="column.align ? `text-${column.align}` : ''">
          <!-- ArkimeSessionField for first column with fieldConfig -->
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
        </td>
      </tr>
    </tbody>
  </table>
</template>

<script setup>
import ArkimeSessionField from '../sessions/SessionField.vue';
import { commaString, humanReadableBytes } from '@common/vueFilters.js';

defineProps({
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
.table th {
  position: sticky;
  top: 0;
}
</style>
