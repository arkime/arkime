<template>
  <span v-if="value !== undefined">
    <label v-if="!hideLabel"
      class="text-orange pr-2">
      {{ field.label }}
    </label>
    <!-- table field -->
    <template v-if="field.type === 'table'">
      <b-overlay
        no-center
        rounded="sm"
        variant="dark"
        :show="getRenderingTable">
        <integration-table
          :fields="field.fields"
          :table-data="value"
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
      <template v-if="field.join">
        {{ value.join(field.join || ', ') }}
      </template>
      <div v-else
        :key="val"
        v-for="val in value">
        {{ val }}
      </div>
    </template> <!-- /array field -->
    <!-- url field -->
    <template v-else-if="field.type === 'url'">
      <a
        target="_blank"
        rel="noopener noreferrer"
        :href="value">
        {{ value }}
      </a>
    </template> <!-- /url field -->
    <!-- json field -->
    <template v-else-if="field.type === 'json'">
      <pre><code>{{ JSON.stringify(value, null, 2) }}</code></pre>
    </template> <!-- /json field -->
    <!-- default string field -->
    <template v-else>
      <template v-if="field.pivot">
        <cont3xt-field
          :value="value"
        />
      </template>
      <template v-else>
        {{ value }}
      </template>
    </template> <!-- /default string field -->
  </span>
</template>

<script>
import dr from 'defang-refang';
import { mapGetters } from 'vuex';

import Cont3xtField from '@/utils/Field';
import IntegrationTable from '@/components/integrations/IntegrationTable';

export default {
  name: 'IntegrationValue',
  components: {
    Cont3xtField,
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
    hideLabel: { // whether to hide the field label (used for table values)
      type: Boolean,
      default: false
    }
  },
  computed: {
    ...mapGetters(['getRenderingTable']),
    value () {
      let value = JSON.parse(JSON.stringify(this.data));

      for (const p of this.field.path) {
        if (!value) {
          console.warn(`Can't resolve path: ${this.field.path.join('.')}`);
          return '';
        }
        value = value[p];
      }

      if (this.field.defang) {
        value = dr.defang(value);
      }

      // don't show empty tables, lists, or strings
      if (this.field.type !== 'json' && value && value.length === 0) {
        // ignores ms because it's a number and value.length is undefined
        value = undefined;
      }

      return value;
    }
  }
};
</script>
