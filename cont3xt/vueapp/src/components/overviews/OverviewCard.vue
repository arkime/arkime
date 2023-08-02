<template>
  <b-card class="mb-2">
    <h5 class="text-warning mb-3"
        v-if="card.title && indicator.query">
    {{ card.title.replace('%{query}', indicator.query) }}
    </h5>

    <!-- field errors -->
    <b-alert
        :show="warningCount > 0"
        variant="warning"
        class="d-flex flex-column">
      <div>
        <span class="pr-2">
          <span class="fa fa-exclamation-triangle fa-fw" />
        </span>
        <div class="display-inline-block">
          {{ warningCount }} {{ (warningCount === 1) ? 'field is' : 'fields are' }} incorrectly linked.
          <a class="no-decoration text-secondary pointer-cursor" @click="showWarningDetails = !showWarningDetails">
            {{ showWarningDetails ? 'Hide' : 'Show' }} details.
          </a>
          <a class="no-decoration" href="settings#overviews">Fix configuration in Overview Settings.</a>
        </div>
      </div>
      <div v-if="showWarningDetails">
        <hr>
        <ol class="m-0">
          <li v-for="(warningMessage, i) in warningMessages" :key="i">
            {{ warningMessage }}
          </li>
        </ol>
      </div>
    </b-alert> <!-- /field errors -->
    <!-- card template -->
    <template v-if="card.fields">
      <div
        v-for="{ field, fieldData } in fillableCardDataFields"
        :key="field.label">
        <integration-value
            :field="field"
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
import normalizeCardField from '../../../../normalizeCardField';

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
  data () {
    return {
      showWarningDetails: false
    };
  },
  computed: {
    ...mapGetters(['getIntegrations', 'getResults']),
    allCardDataFields () {
      if (this.card.fields == null) { return []; }
      return this.card.fields.map(fieldRef => {
        let field;
        switch (fieldRef.type) {
        case 'custom':
          field = normalizeCardField(fieldRef.custom);
          if (field?.path == null) {
            return { errMsg: 'Custom field failed to create path. Is "field" specified?' };
          }
          break;
        case 'linked':
          field = this.createLinkedField(fieldRef, this.getIntegrations);
          if (field == null) {
            return { errMsg: `Unable to find linked field: '${fieldRef.field}' in integration '${fieldRef.from}'` };
          }
          break;
        default:
          return { errMsg: `Unknown field type: '${fieldRef.type}'` };
        }

        return {
          type: 'field',
          field,
          fieldData: this.integrationDataMap[fieldRef.from]
        };
      });
    },
    usableCardDataFields () {
      // filter out errored fields
      return this.allCardDataFields.filter(({ errMsg }) => errMsg == null);
    },
    fillableCardDataFields () {
      return this.usableCardDataFields.filter(({ fieldData }) => fieldData != null);
    },
    warningMessages () {
      return this.allCardDataFields.filter(({ errMsg }) => errMsg != null).map(({ errMsg }) => errMsg);
    },
    warningCount () {
      return this.warningMessages.length;
    },
    integrationDataMap () {
      return getIntegrationDataMap(this.getResults, this.indicator);
    }
  },
  methods: {
    createLinkedField (fieldRef, integrations) {
      const integrationCard = integrations?.[fieldRef.from]?.card;
      let field = integrationCard?.fields?.find(f => f?.label === fieldRef.field);

      if (field && fieldRef.alias) { // re-label fields with aliases
        field = { ...field, label: fieldRef.alias };
      }

      return field;
    }
  }
};
</script>
