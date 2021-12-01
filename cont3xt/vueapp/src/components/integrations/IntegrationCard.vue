<template>
  <b-card
    class="mb-2"
    v-if="Object.keys(getIntegrationData).length">
    <h5 class="text-warning mb-3"
      v-if="card && card.title && getIntegrationData._query">
      {{ card.title.replace('%{query}', getIntegrationData._query) }}
      <b-button
        size="sm"
        @click="refresh"
        variant="outline-info"
        class="ml-1 mt-1 float-right"
        v-b-tooltip.hover="`Queried ${$options.filters.moment(getIntegrationData.data._createTime, 'from')}\n${$options.filters.dateString(getIntegrationData.data._createTime)}`">
        <span class="fa fa-refresh fa-fw" />
      </b-button>
    </h5>
    <!-- error with data -->
    <b-alert
      :show="!!error"
      variant="danger">
      <span class="pr-2">
        <span class="fa fa-exclamation-triangle fa-fw fa-3x" />
      </span>
      <div class="display-inline-block">
        Error fetching data:
        <br>
        {{ error }}
      </div>
    </b-alert> <!-- error with data -->
    <!-- no template -->
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
    </b-alert> <!-- no template -->
    <!-- no data -->
    <template v-if="Object.keys(getIntegrationData.data).length === 1 && getIntegrationData.data._createTime">
      <h5 class="display-4 text-center mt-4 mb-4 text-muted">
        <span class="fa fa-folder-open" />
        <br>
        No data
      </h5>
    </template> <!-- /no data -->
    <template v-else> <!-- data -->
      <!-- card template -->
      <template v-if="card && card.fields">
        <div
          v-for="field in card.fields"
          :key="field.label">
          <integration-value
            :field="field"
            v-if="getIntegrationData.data"
            :data="getIntegrationData.data"
          />
        </div>
      </template> <!-- /card template -->
      <!-- raw -->
      <b-card class="mt-2">
        <h6 v-b-toggle.collapse-raw
          class="card-title mb-1 text-warning">
          raw
          <span class="pull-right">
            <span class="when-open fa fa-caret-up" />
            <span class="when-closed fa fa-caret-down" />
          </span>
        </h6>
        <b-collapse
          class="mt-2"
          id="collapse-raw">
          <pre class="text-info">{{ getIntegrationData.data }}</pre>
        </b-collapse>
      </b-card> <!-- /raw -->
    </template> <!-- /data -->
  </b-card>
</template>

<script>
import { mapGetters } from 'vuex';

import Cont3xtService from '@/components/services/Cont3xtService';
import IntegrationValue from '@/components/integrations/IntegrationValue';

// NOTE: IntegrationCard displays IntegrationValues AND IntegrationTables
// IntegrationTables can ALSO display IntegrationValues, so:
// IntegrationCard -> IntegrationValue
// IntegrationCard -> IntegrationValue -> IntegrationTable -> IntegrationValue
export default {
  name: 'IntegrationCard',
  components: { IntegrationValue },
  data () {
    return {
      error: '',
      loading: false
    };
  },
  computed: {
    ...mapGetters(['getIntegrationData', 'getIntegrations']),
    card () {
      const { source } = this.$store.state.displayIntegration;
      if (!this.getIntegrations[source]) { return {}; }
      return this.getIntegrations[source].card;
    }
  },
  methods: {
    refresh () {
      // display loading overload (parent update hides overlay)
      this.$store.commit('SET_RENDERING_CARD', true);
      const { itype, source, value } = this.$store.state.displayIntegration;
      Cont3xtService.refresh({ itype, source, value }).then((response) => {
        this.$store.commit('SET_INTEGRATION_DATA', response);
        // update the results in the parent so subsequent clicks on this
        // integration value's button has the updated results
        this.$emit('update-results', { itype, source, value, data: response });
      }).catch((err) => {
        this.error = err;
      });
    }
  },
  updated () { // card data is rendered
    this.$nextTick(() => {
      this.$store.commit('SET_RENDERING_CARD', false);
    });
  }
};
</script>
