<template>
  <div v-if="Object.keys(getIntegrationData).length">
    <h5 class="text-orange"
      v-if="card && card.title && getIntegrationData._query">
      {{ card.title.replace('%{query}', getIntegrationData._query) }}
    </h5>
    <template v-if="card && card.fields">
      <div
        v-for="field in card.fields"
        :key="field.label">
        <integration-value
          :field="field"
          :data="getIntegrationData.data"
        />
      </div>
    </template>
    <b-alert
      :show="!card"
      variant="warning">
      <span class="pr-2">
        <span class="fa fa-exclamation-triangle fa-fw fa-3x" />
      </span>
      <div class="display-inline-block">
        Missing information to render the data.
        <br>
        Please make sure your integration has a "card" attribute.
      </div>
    </b-alert>
    <!-- raw -->
    <b-card>
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

import IntegrationValue from '@/components/integrations/IntegrationValue';

// NOTE: IntegrationCard displays IntegrationValues AND IntegrationTables
// IntegrationTables can ALSO display IntegrationValues, so:
// IntegrationCard -> IntegrationValue
// IntegrationCard -> IntegrationValue -> IntegrationTable -> IntegrationValue
export default {
  name: 'IntegrationCard',
  components: { IntegrationValue },
  computed: {
    ...mapGetters(['getIntegrationData', 'getIntegrations']),
    card () {
      const { source } = this.$store.state.displayIntegration;
      if (!this.getIntegrations[source]) { return {}; }
      return this.getIntegrations[source].card;
    }
  }
};
</script>
