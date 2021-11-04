<template>
  <div>
    {{ getIntegrationData.query }}
    <h5 class="text-orange"
      v-if="card && card.title && getIntegrationData._query">
      {{ card.title.replace('%{query}', getIntegrationData._query) }}
    </h5>
    <template v-if="card && card.fields">
      <div
        v-for="field in card.fields"
        :key="field.label">
        <span class="text-orange pr-2">
          {{ field.label }}
        </span>
        <!-- TODO defrag -->
        <!-- TODO pivot -->
        <!-- table field -->
        <span v-if="field.type === 'table'">
          <integration-card-table
            :fields="field.fields"
            :table-data="getFieldValue(field)"
          />
        </span> <!-- /table field -->
        <!-- array field -->
        <span v-else-if="field.type === 'array'">
          <span v-if="field.join"> <!-- TODO test -->
              {{ getFieldValue(field).join(field.join || ', ') }}
          </span>
          <!-- TODO better style -->
          <!-- TODO better display a max? -->
          <div v-else
            :key="val"
            v-for="val in getFieldValue(field)">
            {{ val }}
          </div>
        </span> <!-- /array field -->
        <!-- url field -->
        <span v-else-if="field.type === 'url'">
          <a
            target="_blank"
            rel="noopener noreferrer"
            :href="getFieldValue(field)">
            {{ getFieldValue(field) }}
          </a>
        </span> <!-- /url field -->
        <!-- json field -->
        <span v-else-if="field.type === 'json'">
          {{ JSON.stringify(getFieldValue(field)) }}
        </span> <!-- /json field -->
        <!-- default string field -->
        <span v-else>
          {{ getFieldValue(field) }}
        </span> <!-- /default string field -->
      </div>
    </template>
    <template v-else-if="!card">
      WHOOPS! <!-- TODO display error -->
    </template>
    <!-- raw -->
    <b-card
      v-if="Object.keys(getIntegrationData).length">
      <small>
        <h5 class="card-title"
          v-b-toggle.collapse-raw>
          raw
        </h5>
        <b-collapse id="collapse-raw">
          <pre class="text-accent">{{ getIntegrationData.data }}</pre>
        </b-collapse>
      </small>
    </b-card> <!-- /raw -->
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import IntegrationCardTable from '@/components/integrations/IntegrationCardTable';

export default {
  name: 'IntegrationCard',
  components: { IntegrationCardTable },
  computed: {
    ...mapGetters(['getIntegrationData', 'getIntegrations']),
    card () {
      const { source } = this.$store.state.displayIntegration;
      if (!this.getIntegrations[source]) { return {}; }
      console.log('CARD', this.getIntegrations[source].card); // TODO remove
      console.log('DATA', this.getIntegrationData); // TODO remove
      return this.getIntegrations[source].card;
    }
  },
  methods: {
    getFieldValue (field) {
      let value = this.getIntegrationData.data;

      for (const p of field.path) {
        value = value[p];
      }

      return value;
    }
  }
};
</script>
