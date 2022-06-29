<template>
  <span
    v-b-tooltip.hover
    :title="value.full"
    v-if="value.value !== undefined"
    :class="{'cursor-help':value.full}">
    <div :class="field.type === 'table' || field.type === 'array' ? 'd-flex justify-content-between align-items-center' : 'd-inline'">
      <label
        tabindex="-1"
        v-if="!hideLabel"
        class="text-warning"
        @click="toggleValue"
        :class="field.type === 'table' || field.type === 'array' ? 'flex-grow-1 cursor-pointer': 'pr-2'">
        {{ field.label }}
        <span
          class="fa"
          v-if="field.type === 'table' || field.type === 'array'"
          :class="{'fa-caret-down':visible,'fa-caret-up':!visible}"
        />
        <span v-if="field.type === 'table'"
            :class="getTableLength() === 0 ? 'table-count-low' : 'table-count-normal'">({{ getTableLength() }})
        </span>
      </label>
      <div class="d-inline">
        <b-button
          size="xs"
          tabindex="-1"
          @click="copy"
          variant="outline-success"
          v-if="field.type === 'table'"
          v-b-tooltip.hover
          title="Copy table as CSV string">
          <span class="fa fa-copy fa-fw" />
        </b-button>
        <b-button
          size="xs"
          tabindex="-1"
          @click="download"
          variant="outline-success"
          v-if="field.type === 'table'"
          v-b-tooltip.hover
          title="Download table as CSV">
          <span class="fa fa-download fa-fw" />
        </b-button>
      </div>
    </div>
    <template v-if="visible">
      <!-- table field -->
      <template v-if="field.type === 'table'">
        <b-overlay
          no-center
          rounded="sm"
          blur="0.2rem"
          opacity="0.9"
          variant="transparent"
          :show="getRenderingTable">
          <integration-table
            v-if="value.value"
            :fields="field.fields"
            :table-data="value.value"
            :default-sort-field="field.defaultSortField"
            :default-sort-direction="field.defaultSortDirection"
            @tableFilteredDataChanged="tableFilteredDataChanged"
          />
          <template #overlay>
            <div class="overlay-loading">
              <span class="fa fa-circle-o-notch fa-spin fa-2x" />
              <p>Rendering table data...</p>
            </div>
          </template>
        </b-overlay>
      </template> <!-- /table field -->
      <!-- array field -->
      <template v-else-if="field.type === 'array'">
        <b-overlay
          no-center
          rounded="sm"
          blur="0.2rem"
          opacity="0.9"
          variant="transparent"
          :show="getRenderingArray">
          <integration-array
            :field="field"
            v-if="value.value"
            :array-data="value.value"
            :highlights-array="highlights"
          />
          <template #overlay>
            <div class="overlay-loading">
              <span class="fa fa-circle-o-notch fa-spin fa-2x" />
              <p>Rendering array data...</p>
            </div>
          </template>
        </b-overlay>
      </template> <!-- /array field -->
      <!-- url field -->
      <template v-else-if="field.type === 'url'">
        <a
          target="_blank"
          :href="value.value"
          rel="noopener noreferrer"
          data-testid="integration-url">
          <highlightable-text
            :content="value.value"
            :highlights="highlights"/>
        </a>
      </template> <!-- /url field -->
      <!-- json field -->
      <template v-else-if="field.type === 'json'">
        <pre class="text-info"><code><highlightable-text
            :content="value.value"
            :highlights="highlights"/></code></pre>
      </template> <!-- /json field -->
      <!-- /default string, ms, seconds, & date field -->
      <template v-else>
        <template v-if="field.pivot">
          <cont3xt-field
            :value="value.value"
            :highlights="highlights"
          />
        </template>
        <template v-else>
          <highlightable-text
              :content="value.value"
              :highlights="highlights"/>
        </template>
      </template> <!-- /default string, ms, seconds, & date field -->
    </template>
  </span>
</template>

<script>
import { mapGetters } from 'vuex';

import Cont3xtField from '@/utils/Field';
import IntegrationArray from '@/components/integrations/IntegrationArray';
import IntegrationTable from '@/components/integrations/IntegrationTable';
import HighlightableText from '@/utils/HighlightableText';
import { formatValue } from '@/utils/formatValue';

export default {
  name: 'IntegrationValue',
  components: {
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
    ...mapGetters(['getRenderingTable', 'getRenderingArray', 'getIntegrationData']),
    value () {
      let full;
      let value = this.findValue(this.data, this.field);

      // truncate long values
      if (this.truncate && value && value.length > (this.field.len || 100)) {
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
      this.$copyText(this.generateCSVString());
    },
    download () {
      const a = document.createElement('a');
      const file = new Blob([this.generateCSVString()], { type: 'text/csv' });
      a.href = URL.createObjectURL(file);
      let { source } = this.$store.state.displayIntegration;
      source = source.replaceAll(' ', '_');
      a.download = `${new Date().toISOString()}_${source}_${this.field.path.join('.')}_${this.getIntegrationData._query}.csv`;
      a.click();
      URL.revokeObjectURL(a.href);
    },
    tableFilteredDataChanged (newFilteredData) { // syncs filteredData with table
      this.tableFilteredData = newFilteredData;
    },
    /* helpers ------------------------------------------------------------- */
    findValue (data, field) {
      return formatValue(data, field);
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
.table-count-normal {
  color: #EEE;
}
.table-count-low {
  color: gray;
}
</style>
