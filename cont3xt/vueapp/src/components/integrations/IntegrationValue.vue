<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span
    :title="value.full"
    v-if="value.value !== undefined"
    :class="{'cursor-help':value.full}">
    <v-tooltip
      v-if="value.full"
      activator="parent"
      location="top">{{ value.full }}</v-tooltip>
    <div
      class="mt-1"
      :class="field.type === 'table' || field.type === 'array' ? 'd-flex justify-space-between align-center' : 'd-inline'">
      <label
        tabindex="-1"
        v-if="!hideLabel"
        @click="toggleValue"
        :class="(field.type === 'table' || field.type === 'array') ? 'flex-grow-1 cursor-pointer' : 'pr-2'">
        <span class="text-warning">
          {{ field.label }}
          <v-icon
            v-if="field.type === 'table' || field.type === 'array'"
            :icon="visible ? 'mdi-menu-down' : 'mdi-menu-up'" />
        </span>
        <span
          v-if="field.type === 'table'"
          class="ml-1"
          :class="getTableLength() === 0 ? 'table-count-low' : 'text-default'">({{ getTableLength() }})
        </span>
      </label>
      <div
        v-if="field.type === 'table'"
        class="d-flex ga-1">
        <v-btn
          size="x-small"
          tabindex="-1"
          @click="copy"
          variant="outlined"
          color="success"
          v-tooltip="'Copy table as CSV string'"
          title="Copy table as CSV string">
          <v-icon icon="mdi-content-copy mdi-fw" />
        </v-btn>
        <v-btn
          size="x-small"
          tabindex="-1"
          @click="download"
          variant="outlined"
          color="success"
          v-tooltip="'Download table as CSV'"
          title="Download table as CSV">
          <v-icon icon="mdi-download mdi-fw" />
        </v-btn>
      </div>
    </div>
    <template v-if="visible">
      <!-- table field -->
      <template v-if="field.type === 'table'">
        <integration-table
          v-if="value.value"
          :fields="field.fields"
          :table-data="value.value"
          :default-sort-field="field.defaultSortField"
          :default-sort-direction="field.defaultSortDirection"
          @table-filtered-data-changed="tableFilteredDataChanged" />
      </template> <!-- /table field -->
      <!-- array field -->
      <template v-else-if="field.type === 'array'">
        <integration-array
          :field="field"
          v-if="value.value"
          :array-data="value.value"
          :highlights-array="highlights" />
      </template> <!-- /array field -->
      <!-- external link field -->
      <template v-else-if="field.type === 'externalLink'">
        <v-btn
          target="_blank"
          :href="value.value"
          size="x-small"
          class="integration-external-link-button square-btn-xs"
          variant="outlined"
          color="primary"
          v-tooltip="field.altText != null ? field.altText : value.value">
          <v-icon icon="mdi-open-in-new" />
        </v-btn>
      </template> <!-- /external link field -->
      <!-- url field -->
      <template v-else-if="field.type === 'url'">
        <a
          target="_blank"
          :href="value.value"
          rel="noopener noreferrer"
          data-testid="integration-url">
          <highlightable-text
            :content="value.value"
            :highlights="highlights" />
        </a>
      </template> <!-- /url field -->
      <!-- json field -->
      <template v-else-if="field.type === 'json'">
        <pre class="text-info"><code><highlightable-text
          :content="value.value"
          :highlights="highlights" /></code></pre>
      </template> <!-- /json field -->
      <!-- DnsRecords field -->
      <template v-else-if="field.type === 'dnsRecords'">
        <DnsRecords :data="value.value" />
      </template> <!-- /DnsRecords field -->
      <!-- /default string, ms, seconds, & date field -->
      <template v-else>
        <template v-if="field.pivot">
          <cont3xt-field
            pull-left
            :data="data"
            :value="value.value"
            :options="field.options"
            :highlights="highlights" />
        </template>
        <template v-else>
          <highlightable-text
            :content="value.value"
            :highlights="highlights" />
        </template>
      </template> <!-- /default string, ms, seconds, & date field -->
    </template>
  </span>
</template>

<script>
import { mapGetters } from 'vuex';

import Cont3xtField from '@/utils/Field.vue';
import IntegrationArray from '@/components/integrations/IntegrationArray.vue';
import IntegrationTable from '@/components/integrations/IntegrationTable.vue';
import HighlightableText from '@/utils/HighlightableText.vue';
import { formatPostProcessedValue } from '@/utils/formatValue';
import DnsRecords from '@/utils/DnsRecords.vue';
import { clipboardCopyText } from '@/utils/clipboardCopyText';

export default {
  name: 'IntegrationValue',
  components: {
    DnsRecords,
    Cont3xtField,
    IntegrationArray,
    IntegrationTable,
    HighlightableText
  },
  props: {
    data: { // the data to search for values within
      type: Object,
      required: true
    },
    field: { // the field to determine where the value is within the data
      type: Object,
      required: true
    },
    hideLabel: { // whether to hide the field label (used for tables)
      type: Boolean,
      default: false
    },
    truncate: { // whether to truncate the value if it is long (used for tables)
      type: Boolean,
      default: false
    },
    highlights: { // array of highlighted spans (or array of highlighted span arrays [for integration-arrays])
      type: Array,
      default () {
        return null;
      }
    }
  },
  data () {
    return {
      visible: true,
      tableFilteredData: null
    };
  },
  computed: {
    ...mapGetters(['getRenderingTable', 'getRenderingArray', 'getActiveIndicator', 'getActiveSource']),
    value () {
      let full;
      let value = this.findValue(this.data, this.field);

      // truncate long values
      if (this.truncate && value && typeof value === 'string' && value.length > (this.field.len || 100)) {
        full = value;
        value = `${value.substring(0, this.field.len || 100)}...`;
      }

      return { value, full };
    }
  },
  mounted () {
    // automatically collapses empty tables
    if (this.getTableLength() === 0) {
      this.visible = false;
    }
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    toggleValue () {
      if (this.field.type !== 'table' && this.field.type !== 'array') {
        return;
      }

      this.visible = !this.visible;
    },
    copy () {
      clipboardCopyText(this.generateCSVString());
    },
    download () {
      const a = document.createElement('a');
      const file = new Blob([this.generateCSVString()], { type: 'text/csv' });
      a.href = URL.createObjectURL(file);
      const source = this.getActiveSource.replaceAll(' ', '_');
      a.download = `${new Date().toISOString()}_${source}_${this.field.path.join('.')}_${this.getActiveIndicator.query}.csv`;
      a.click();
      URL.revokeObjectURL(a.href);
    },
    tableFilteredDataChanged (newFilteredData) { // syncs filteredData with table
      this.tableFilteredData = newFilteredData;
    },
    /* helpers ------------------------------------------------------------- */
    findValue (data, field) {
      return formatPostProcessedValue(data, field);
    },
    getTableData () {
      if (this.tableFilteredData) {
        return this.tableFilteredData;
      }
      return Array.isArray(this.value.value) ? this.value.value : [this.value.value];
    },
    getTableLength () {
      return this.getTableData().length;
    },
    generateCSVString () {
      let csvStr = '';

      const fieldLabels = [];
      for (const field of this.field.fields) {
        fieldLabels.push(field.label);
      }

      csvStr += `${fieldLabels.join(',')}\n`;

      const value = this.getTableData();

      for (const valueRow of value) {
        const values = this.field.fields.map(field => {
          // double quotes in the text are escaped as two consecutive double quotes
          let valueStr = this.findValue(valueRow, field)?.toString()?.replaceAll('"', '""');
          if (valueStr == null) { valueStr = ''; }
          // text containing commas or line breaks is wrapped in double quotes
          if (valueStr.includes(',') || valueStr.includes('\n')) {
            valueStr = `"${valueStr}"`;
          }
          return valueStr;
        });
        csvStr += `${values.join(',')}\n`;
      }

      return csvStr;
    }
  }
};
</script>

<style scoped>
.table-count-low {
  color: gray;
}
</style>
