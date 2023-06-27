<template>
  <b-card class="mb-2">
    <h5 class="text-warning mb-3"
        v-if="card.title && query">
    {{ card.title.replace('%{query}', query) }}
    </h5>

    <!-- field errors -->
    <b-alert
        :show="fieldErrorCount > 0"
        variant="warning">
      <span class="pr-2">
        <span class="fa fa-exclamation-triangle fa-fw" />
      </span>
      <div class="display-inline-block">
        {{ fieldErrorCount }} {{ (fieldErrorCount === 1) ? 'field is' : 'fields are' }} incorrectly linked.
        <a class="no-decoration" href="settings#overviews">Fix configuration here.</a>
      </div>
    </b-alert> <!-- /field errors -->
    <!-- card template -->
    <template v-if="card.fields">
      <div
        v-for="{ field, fieldData } in fillableCardDataFields"
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
  </b-card>
</template>

<script>
import { mapGetters } from 'vuex';
import { gatherIntegrationData } from '@/utils/gatherIntegrationData';

import IntegrationValue from '@/components/integrations/IntegrationValue';

// NOTE: OverviewCard displays IntegrationValues AND IntegrationTables
// IntegrationTables can ALSO display IntegrationValues, so:
// OverviewCard -> IntegrationValue
// OverviewCard -> IntegrationValue -> IntegrationTable -> IntegrationValue
export default {
  name: 'OverviewCard',
  components: { IntegrationValue },
  props: {
    fullData: { // the data returned from cont3xt search
      type: Object,
      required: true
    },
    query: { // the indicator to be overviewed
      type: String,
      required: true
    },
    itype: { // the itype of the indicator to be overviewed
      type: String,
      required: true
    },
    card: { // the configuration for this overview card
      type: Object,
      required: true
    }
  },
  computed: {
    ...mapGetters(['getIntegrations']),
    cardDataFields () {
      if (this.card.fields == null) { return []; }

      const dataFields = this.card.fields.map(fieldRef => {
        let field = this.fieldFor(fieldRef, this.getIntegrations);
        const fieldData = this.dataFor(fieldRef, this.integrationData);

        if (field && fieldRef.alias) { // re-label fields with aliases
          field = { ...field, label: fieldRef.alias };
        }

        return { field, fieldData };
      });

      // remove fields that are incorrectly linked
      return dataFields.filter(({ field }) => (field != null));
    },
    fieldErrorCount () {
      return this.card.fields.length - this.cardDataFields.length;
    },
    fillableCardDataFields () {
      return this.cardDataFields.filter(({ fieldData }) => fieldData != null);
    },
    integrationData () {
      return gatherIntegrationData(this.fullData, this.itype, this.query);
    }
  },
  methods: {
    fieldFor (fieldReference, integrations) {
      const integrationCard = integrations?.[fieldReference.from]?.card;
      return integrationCard?.fields?.find(field => field?.label === fieldReference.field);
    },
    dataFor (fieldReference, integrationData) {
      return integrationData?.[fieldReference.from];
    }
  }
};
</script>
