<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <cont3xt-card class="mb-2">
    <h5 class="text-warning mb-3"
      v-if="card.title && indicator.query">
      {{ card.title.replace('%{query}', indicator.query) }}
    </h5>

    <!-- field errors -->
    <v-alert
        v-if="warningCount > 0"
        color="warning"
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
        <ol class="ma-0">
          <li v-for="(warningMessage, i) in warningMessages" :key="i">
            {{ warningMessage }}
          </li>
        </ol>
      </div>
    </v-alert> <!-- /field errors -->
    <!-- card template -->
    <div v-if="card.fields" class="d-flex flex-column ga-1">
      <div
        v-for="{ field, fieldData } in fillableCardDataFields"
        :key="field.label">
        <integration-value
            :field="field"
            :data="fieldData"
        />
      </div>
    </div> <!-- /card template -->
    <!-- raw -->
    <!-- TODO: toby - update alongside integration card -->
    <v-expansion-panels class="mt-2">
      <v-expansion-panel color="cont3xt-card">
        <template #title>
          <strong class="text-warning">
            raw
          </strong>
        </template>
        <template #text>
          <pre class="text-info overflow-x-auto">{{ integrationDataMap }}</pre>
        </template>
      </v-expansion-panel>
    </v-expansion-panels>
    <!-- /raw -->
  </cont3xt-card>
</template>

<script>
import { mapGetters } from 'vuex';

import IntegrationValue from '@/components/integrations/IntegrationValue.vue';
import { Cont3xtIndicatorProp, getIntegrationDataMap } from '@/utils/cont3xtUtil';
import Cont3xtCard from '@/utils/Cont3xtCard.vue';
import { normalizeCardField } from '@/utils/normalizeCardField.js';

// NOTE: OverviewCard displays IntegrationValues AND IntegrationTables
// IntegrationTables can ALSO display IntegrationValues, so:
// OverviewCard -> IntegrationValue
// OverviewCard -> IntegrationValue -> IntegrationTable -> IntegrationValue
export default {
  name: 'OverviewCard',
  components: { IntegrationValue, Cont3xtCard },
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
