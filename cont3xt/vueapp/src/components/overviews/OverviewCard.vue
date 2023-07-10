<template>
  <b-card class="mb-2">
    <h5 class="text-warning mb-3"
        v-if="card.title && indicator.query">
    {{ card.title.replace('%{query}', indicator.query) }}
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
        <pre class="text-info">{{ integrationDataMap }}</pre>
      </b-collapse>
    </b-card> <!-- /raw -->
  </b-card>
</template>

<script>
import { mapGetters } from 'vuex';

import IntegrationValue from '@/components/integrations/IntegrationValue';
import { Cont3xtIndicatorProp, getIntegrationDataMap } from '@/utils/cont3xtUtil';

// NOTE: OverviewCard displays IntegrationValues AND IntegrationTables
// IntegrationTables can ALSO display IntegrationValues, so:
// OverviewCard -> IntegrationValue
// OverviewCard -> IntegrationValue -> IntegrationTable -> IntegrationValue
export default {
  name: 'OverviewCard',
  components: { IntegrationValue },
  props: {
    indicator: Cont3xtIndicatorProp, // the indicator to be overviewed
    card: { // the configuration for this overview card
      type: Object,
      required: true
    }
  },
  computed: {
    ...mapGetters(['getIntegrations', 'getResults']),
    cardDataFields () {
      if (this.card.fields == null) { return []; }

      const dataFields = this.card.fields.map(fieldRef => {
        let field = this.fieldFor(fieldRef, this.getIntegrations);
        const fieldData = this.dataFor(fieldRef, this.integrationDataMap);

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
    integrationDataMap () {
      return getIntegrationDataMap(this.getResults, this.indicator);
    }
  },
  methods: {
    fieldFor (fieldReference, integrations) {
      const integrationCard = integrations?.[fieldReference.from]?.card;
      return integrationCard?.fields?.find(field => field?.label === fieldReference.field);
    },
    dataFor (fieldReference, integrationDataMap) {
      return integrationDataMap?.[fieldReference.from];
    }
  }
};
</script>
