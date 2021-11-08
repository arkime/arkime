<template>
  <span>
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
          :table-data="getFieldValue(field, data)"
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
        {{ getFieldValue(field, data).join(field.join || ', ') }}
      </template>
      <div v-else
        :key="val"
        v-for="val in getFieldValue(field, data)">
        {{ val }}
      </div>
    </template> <!-- /array field -->
    <!-- url field -->
    <template v-else-if="field.type === 'url'">
      <a
        target="_blank"
        rel="noopener noreferrer"
        :href="getFieldValue(field, data)">
        {{ getFieldValue(field, data) }}
      </a>
    </template> <!-- /url field -->
    <!-- json field -->
    <template v-else-if="field.type === 'json'">
      <pre><code>{{ JSON.stringify(getFieldValue(field, data), null, 2) }}</code></pre>
    </template> <!-- /json field -->
    <!-- default string field -->
    <template v-else>
      <template v-if="field.pivot">
        <cont3xt-field
          :value="getFieldValue(field, data)"
        />
      </template>
      <template v-else>
        {{ getFieldValue(field, data) }}
      </template>
    </template> <!-- /default string field -->
  </span>
</template>

<script>
import dr from 'defang-refang';
import { mapGetters } from 'vuex';

import Cont3xtField from '@/utils/Field';
import IntegrationTable from '@/components/integrations/IntegrationTable';

const defaults = {
  ms: 0,
  url: '',
  json: {},
  table: [],
  array: [],
  string: ''
};

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
    ...mapGetters(['getRenderingTable'])
  },
  methods: {
    getFieldValue (field, data) {
      let value = data;

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

      return value || defaults[field.type];
    }
  }
};
</script>
