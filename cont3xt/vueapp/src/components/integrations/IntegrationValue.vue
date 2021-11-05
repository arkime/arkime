<template>
  <span>
    <label v-if="!hideLabel"
      class="text-orange pr-2">
      {{ field.label }}
    </label>
    <!-- TODO defrag -->
    <!-- TODO pivot -->
    <!-- table field -->
    <template v-if="field.type === 'table'">
      <integration-table
        :fields="field.fields"
        :table-data="getFieldValue(field, data)"
      />
    </template> <!-- /table field -->
    <!-- array field -->
    <template v-else-if="field.type === 'array'">
      <template v-if="field.join"> <!-- TODO test -->
        {{ getFieldValue(field, data).join(field.join || ', ') }}
      </template>
      <!-- TODO better style -->
      <!-- TODO better display a max? -->
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
      <code>{{ JSON.stringify(getFieldValue(field, data)) }}</code>
    </template> <!-- /json field -->
    <!-- default string field -->
    <template v-else>
      {{ getFieldValue(field, data) }}
    </template> <!-- /default string field -->
  </span>
</template>

<script>
import IntegrationTable from '@/components/integrations/IntegrationTable';

export default {
  name: 'IntegrationValue',
  props: {
    data: Object,
    field: Object,
    hideLabel: Boolean
  },
  components: { IntegrationTable },
  methods: {
    getFieldValue (field, data) {
      let value = data;

      for (const p of field.path) {
        value = value[p];
      }

      return value;
    }
  }
};
</script>
