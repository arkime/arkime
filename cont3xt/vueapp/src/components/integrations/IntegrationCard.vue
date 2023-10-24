<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <b-card
    class="mb-2">
    <h5 class="text-warning mb-3"
      v-if="card && card.title">
      {{ card.title.replace('%{query}', this.indicator.query) }}
      <div class="float-right mt-1">
        <b-button
          size="sm"
          tabindex="-1"
          @click="copy"
          v-b-tooltip.hover
          title="Copy as raw JSON"
          variant="outline-success">
          <span class="fa fa-copy fa-fw" />
        </b-button>
        <b-button
          size="sm"
          tabindex="-1"
          @click="download"
          v-b-tooltip.hover
          variant="outline-success"
          title="Download as raw JSON">
          <span class="fa fa-download fa-fw" />
        </b-button>
        <b-button
          size="sm"
          tabindex="-1"
          @click="refresh"
          v-b-tooltip.hover
          variant="outline-info"
          :title="`Queried ${$options.filters.moment(integrationData._cont3xt.createTime, 'from')}\n${$options.filters.dateString(integrationData._cont3xt.createTime)}`">
          <span class="fa fa-refresh fa-fw" />
        </b-button>
      </div>
    </h5>
    <!-- error with data -->
    <b-alert
      :show="!!error"
      variant="danger">
      <span class="pr-2">
        <span class="fa fa-exclamation-triangle fa-fw fa-3x" />
      </span>
      <div class="display-inline-block">
        <strong>Error:</strong>
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
    <template v-if="Object.keys(integrationData).length === 1 && integrationData._cont3xt.createTime">
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
            v-if="integrationData"
            :data="integrationData"
          />
        </div>
      </template> <!-- /card template -->
      <!-- raw -->
      <b-card class="mt-2">
        <h6 tabindex="-1"
          v-b-toggle.collapse-raw
          class="card-title mb-1 text-warning">
          raw
          <span class="pull-right">
            <span class="when-open fa fa-caret-up" />
            <span class="when-closed fa fa-caret-down" />
          </span>
        </h6>
        <b-collapse
          class="mt-2"
          tabindex="-1"
          id="collapse-raw">
          <pre class="text-info">{{ integrationData }}</pre>
        </b-collapse>
      </b-card> <!-- /raw -->
    </template> <!-- /data -->
  </b-card>
</template>

<script>
import { mapGetters } from 'vuex';

import Cont3xtService from '@/components/services/Cont3xtService';
import IntegrationValue from '@/components/integrations/IntegrationValue';
import { Cont3xtIndicatorProp, getIntegrationData } from '@/utils/cont3xtUtil';

// NOTE: IntegrationCard displays IntegrationValues AND IntegrationTables
// IntegrationTables can ALSO display IntegrationValues, so:
// IntegrationCard -> IntegrationValue
// IntegrationCard -> IntegrationValue -> IntegrationTable -> IntegrationValue
export default {
  name: 'IntegrationCard',
  components: { IntegrationValue },
  props: {
    source: { // the name of the integration to display data from
      type: String,
      required: true
    },
    indicator: Cont3xtIndicatorProp // the indicator to display data for
  },
  data () {
    return {
      error: '',
      loading: false
    };
  },
  computed: {
    ...mapGetters(['getIntegrations', 'getResults']),
    card () {
      if (!this.getIntegrations[this.source]) { return {}; }
      return this.getIntegrations[this.source].card;
    },
    integrationData () {
      return getIntegrationData(this.getResults, this.indicator, this.source);
    }
  },
  methods: {
    refresh () {
      // display loading overload (parent update hides overlay)
      this.$store.commit('SET_RENDERING_CARD', true);
      Cont3xtService.refresh({ indicator: this.indicator, source: this.source }).then((response) => {
        // update the results in the parent so subsequent clicks on this
        // integration value's button has the updated results
        this.$emit('update-results', response);
      }).catch((err) => {
        this.error = err;
      });
    },
    copy () {
      this.error = '';

      if (!this.integrationData) {
        this.error = 'No raw data to copy!';
        return;
      }

      this.$copyText(JSON.stringify(this.integrationData, false, 2));
    },
    download () {
      this.error = '';

      if (!this.integrationData) {
        this.error = 'No raw data to copy!';
        return;
      }

      const a = document.createElement('a');
      const file = new Blob([JSON.stringify(this.integrationData, false, 2)], { type: 'application/json' });
      a.href = URL.createObjectURL(file);
      a.download = `${new Date().toISOString()}_${this.source.replaceAll(' ', '_')}_${this.indicator.query}.json`;
      a.click();
      URL.revokeObjectURL(a.href);
    }
  },
  updated () { // card data is rendered
    this.$nextTick(() => {
      this.$store.commit('SET_RENDERING_CARD', false);
    });
  }
};
</script>
