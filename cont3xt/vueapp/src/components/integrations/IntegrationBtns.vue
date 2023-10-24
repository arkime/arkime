<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="mx-2"
    :class="{'wrap-btns d-flex justify-content-between': buttonIntegrations.length > 4}">
    <overview-selector
      v-if="getActiveIndicator"
      :i-type="getActiveIndicator.itype"
      :selected-overview="selectedOverview"
      @set-override-overview="setOverrideOverview"
    />
    <template v-for="integration in buttonIntegrations">
      <b-button
        v-b-tooltip.hover.noninteractive="integration.name"
        size="xs"
        tabindex="0"
        variant="outline-dark"
        class="mr-1 mb-1 no-wrap"
        :id="`${indicatorId}-${integration.name}-btn`"
        :key="`${indicatorId}-${integration.name}`"
        @click="setAsActive(integration)">
        <img
          :alt="integration.name"
          :src="integration.icon"
          data-testid="integration-btn-icon"
          class="integration-img cursor-pointer"
        />
        <b-badge
          class="btn-badge"
          v-if="shouldDisplayCountedIntegrationBtn(integration, integrationDataMap[integration.name])"
          :variant="integrationCountSeverity(integrationDataMap[integration.name])">
          {{ integrationDataMap[integration.name]._cont3xt.count | humanReadableNumber }}
        </b-badge>
      </b-button>
    </template>
    <template v-if="!buttonIntegrations.length">
      <b-badge
          variant="light" class="d-flex align-items-center mb-1">
        <span>No Integrations</span>
      </b-badge>
    </template>
  </div>
</template>

<script>
import { mapGetters } from 'vuex';
import {
  getIntegrationDataMap,
  shouldDisplayIntegrationBtn,
  integrationCountSeverity, shouldDisplayCountedIntegrationBtn, indicatorFromId
} from '@/utils/cont3xtUtil';
import OverviewSelector from '../overviews/OverviewSelector.vue';

// Clicking an integration button commits to the store which integration, itype,
// and value to display integration data for. The Cont3xt component watches for
// store changes and sets the integration data in the store. the IntegrationCard
// component watches for changes to the integration data to display.
export default {
  name: 'IntegrationBtns',
  components: { OverviewSelector },
  props: {
    /**
     * the global indicator id to display integration buttons for
     *   we use the id instead of indicator itself to ensure that
     *   the correct node is selected in the UI when a button is pressed
     */
    indicatorId: {
      type: String,
      required: true
    },
    /**
     * undefined - show all integrations
     * 'success' | 'secondary' | 'danger' - show only integration buttons with that icon color/severity
     */
    countSeverityFilter: {
      type: String,
      required: false
    },
    selectedOverview: {
      type: Object,
      required: true
    }
  },
  computed: {
    ...mapGetters(['getIntegrationsArray', 'getLoading', 'getResults', 'getActiveIndicator']),
    /**
     * object of { itype, query }
     * * itype - the itype to display the integration data for (if clicked)
     * * query - the value of the query to display the integration data for
     * *     (there may be multiple IPs for instance, so the value
     * *     indicates which IP to display information for)
     */
    indicator () {
      return indicatorFromId(this.indicatorId);
    },
    integrations () {
      return this.getIntegrationsArray.slice().sort((a, b) => {
        return a.order - b.order;
      });
    },
    buttonIntegrations () {
      const sortedIntegrations = this.getIntegrationsArray.slice().sort((a, b) => {
        return a.order - b.order;
      });

      return sortedIntegrations.filter(integration => {
        const integrationData = this.integrationDataMap[integration.name];
        // filter out buttons whose severity don't match countSeverityFilter, if we have one
        if (this.countSeverityFilter) {
          return shouldDisplayCountedIntegrationBtn(integration, integrationData) &&
              this.integrationCountSeverity(integrationData) === this.countSeverityFilter;
        }
        return shouldDisplayIntegrationBtn(integration, integrationData);
      });
    },
    /** @returns a map of integration names to integration data objects */
    integrationDataMap () {
      return getIntegrationDataMap(this.getResults, this.indicator);
    }
  },
  methods: {
    shouldDisplayCountedIntegrationBtn,
    integrationCountSeverity,
    setAsActive (integration) {
      this.$store.commit('SET_QUEUED_INTEGRATION', { indicatorId: this.indicatorId, source: integration.name });
    },
    setOverrideOverview (id) {
      this.$emit('set-override-overview', id);
    }
  }
};
</script>

<style scoped>
.wrap-btns {
  display: flex;
  flex-wrap: wrap;
}
.wrap-btns  .btn {
  flex: 1 1 auto;
}

.integration-img {
  height: 25px;
  margin: 0 6px;
}

.btn-badge {
  margin-right: 0.25rem;
}
</style>
