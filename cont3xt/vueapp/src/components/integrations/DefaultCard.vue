<!--

TODO: toby, determine whether to keep split or combine with IntegrationCard!!

-->
<template>

  <b-card
    class="mb-2"
    v-if="(Object.keys(getIntegrationData) == null || Object.keys(getIntegrationData).length === 0) && card">
    <h5 class="text-warning mb-3">
      {{ card.title.replace('%{query}', query) }}
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
    <template> <!-- data TODO toby -->
      <!-- card template -->
      <template v-if="card && card.fields">
        <div
          v-for="{ field, fieldData } in cardDataFields"
          :key="field.label">
          <integration-value
            :field="field"
            v-if="field && fieldData"
            :data="fieldData"
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

import IntegrationValue from '@/components/integrations/IntegrationValue';

// TODO: toby update docs?
// NOTE: IntegrationCard displays IntegrationValues AND IntegrationTables
// IntegrationTables can ALSO display IntegrationValues, so:
// IntegrationCard -> IntegrationValue
// IntegrationCard -> IntegrationValue -> IntegrationTable -> IntegrationValue
export default {
  name: 'DefaultCard',
  components: { IntegrationValue },
  props: {
    data: { // the data returned from cont3xt search
      type: Object,
      required: true
    },
    query: { // TODO: toby doc
      type: String,
      required: true
    },
    itype: { // TODO: toby doc
      type: String,
      required: true
    }
  },
  data () {
    return {
      error: '',
      loading: false
    };
  },
  computed: { // TODO: toby, getIntegrationData what???
    ...mapGetters(['getIntegrationData', 'getIntegrations', 'getOverviewCardMap']),
    card () {
      return this.getOverviewCardMap[this.itype];
    },
    cardDataFields () {
      const dataFields = this.card?.fields?.map(fieldRef => {
        const field = this.fieldFor(fieldRef, this.getIntegrations);
        if (field && fieldRef.alias) { Object.assign(field, { label: fieldRef.alias }); }

        return {
          field,
          fieldData: this.dataFor(fieldRef, this.integrationData)
        };
      });

      // remove fields that are incorrectly linked or do not yet have data
      return dataFields.filter(({ field, fieldData }) => (
        field != null && fieldData != null
      ));
    },
    integrationData () {
      return this.gatherIntegrationData(this.data, this.itype, this.query);
    }
  },
  methods: {
    fieldFor (fieldReference, integrations) { // TODO: toby, check for this.getIntegrations
      const integrationCard = integrations?.[fieldReference.from]?.card;
      return integrationCard.fields?.find(field => field?.label === fieldReference.field);
    },
    dataFor (fieldReference, integrationData) {
      return integrationData?.[fieldReference.from];
    },
    // TODO: toby dedupe!
    gatherIntegrationData (data, itype, query) { // restructures data into the shape {[integrationName]: data}
      const iTypeStub = data?.[itype] || {};
      const integrationPairs = Object.entries(iTypeStub)
        .filter(([key, _]) => key !== '_query')
        .map(([integrationName, dataEntryArray]) => [integrationName, dataEntryArray?.find(dataEntry => dataEntry._query === query)?.data])
        .filter(([_, val]) => val != null);

      return Object.fromEntries(integrationPairs);
    }
  },
  updated () { // card data is rendered
    this.$nextTick(() => {
      this.$store.commit('SET_RENDERING_CARD', false); // TODO: toby??? need this?
    });
  }
};
</script>
