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
      </label>
      <div class="d-inline">
        <b-button
          size="xs"
          tabindex="-1"
          @click="copy"
          variant="outline-success"
          v-if="field.type === 'table'"
          v-b-tooltip.hover="'Copy as table as CSV string'">
          <span class="fa fa-copy fa-fw" />
        </b-button>
        <b-button
          size="xs"
          tabindex="-1"
          @click="download"
          variant="outline-success"
          v-if="field.type === 'table'"
          v-b-tooltip.hover="'Download as table as CSV'">
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
          rel="noopener noreferrer">
          {{ value.value }}
        </a>
      </template> <!-- /url field -->
      <!-- json field -->
      <template v-else-if="field.type === 'json'">
        <pre class="text-info"><code>{{ JSON.stringify(value.value, null, 2) }}</code></pre>
      </template> <!-- /json field -->
      <!-- ms field -->
      <template v-else-if="field.type === 'ms'">
        {{ this.$options.filters.dateString(value.value) }}
      </template> <!-- /ms field -->
      <!-- seconds field -->
      <template v-else-if="field.type === 'seconds'">
        {{ this.$options.filters.dateString(value.value * 1000) }}
      </template> <!-- /seconds field -->
      <!-- date field -->
      <template v-else-if="field.type === 'date'">
        {{ this.$options.filters.reDateString(value.value) }}
      </template> <!-- /seconds field -->
      <!-- default string field -->
      <template v-else>
        <template v-if="field.pivot">
          <cont3xt-field
            :value="value.value"
          />
        </template>
        <template v-else>
          {{ value.value }}
        </template>
      </template> <!-- /default string field -->
    </template>
  </span>
</template>

<script>
import dr from 'defang-refang';
import { mapGetters } from 'vuex';

import Cont3xtField from '@/utils/Field';
import IntegrationArray from '@/components/integrations/IntegrationArray';
import IntegrationTable from '@/components/integrations/IntegrationTable';

export default {
  name: 'IntegrationValue',
  components: {
    Cont3xtField,
    IntegrationArray,
    IntegrationTable
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
    }
  },
  data () {
    return {
      visible: true
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
    /* helpers ------------------------------------------------------------- */
    findValue (data, field) {
      let value = JSON.parse(JSON.stringify(data));

      for (const p of field.path) {
        if (!value) {
          console.warn(`Can't resolve path: ${field.path.join('.')}`);
          return '';
        }
        value = value[p];
      }

      if (field.defang) {
        value = dr.defang(value);
      }

      // don't show empty tables, lists, or strings
      if (field.type !== 'json' && value && value.length === 0) {
        // ignores ms because it's a number and value.length is undefined
        value = undefined;
      }

      return value;
    },
    generateCSVString () {
      let csvStr = '';

      const fieldLabels = [];
      for (const field of this.field.fields) {
        fieldLabels.push(field.label);
      }

      csvStr += `${fieldLabels.join(',')}\n`;

      let value = this.value.value;
      if (!Array.isArray(value)) {
        value = [value];
      }

      for (const valueRow of value) {
        const values = [];
        for (const field of this.field.fields) {
          values.push(this.findValue(valueRow, field));
        }
        csvStr += `${values.join(',')}\n`;
      }

      return csvStr;
    }
  }
};
</script>
