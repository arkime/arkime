<template>
  <div class="wrap-btns">
    <template v-if="!buttonIntegrations.length">
      <b-badge
          variant="light" class="d-flex align-items-center">
        <span>No Integrations</span>
      </b-badge>
    </template>
    <template v-for="integration in buttonIntegrations">
      <b-button
        v-b-tooltip.hover.noninteractive="integration.name"
        size="xs"
        tabindex="0"
        variant="outline-dark"
        class="mr-1 float-right no-wrap"
        :id="`${indicator.itype}-${integration.name}-${indicator.query}`"
        :key="integration.name"
        @click="setAsActive(integration)">
        <img
          :alt="integration.name"
          :src="integration.icon"
          data-testid="integration-btn-icon"
          class="integration-img cursor-pointer"
        />
        <b-badge
          class="btn-badge"
          v-if="integrationDataMap[integration.name]._cont3xt.count !== undefined"
          :variant="countBadgeColor(integrationDataMap[integration.name])">
          {{ integrationDataMap[integration.name]._cont3xt.count | humanReadableNumber }}
        </b-badge>
      </b-button>
    </template>
  </div>
</template>

<script>
import { mapGetters } from 'vuex';
import { Cont3xtIndicatorProp, getIntegrationDataMap } from '@/utils/cont3xtUtil';

// Clicking an integration button commits to the store which integration, itype,
// and value to display integration data for. The Cont3xt component watches for
// store changes and sets the integration data in the store. the IntegrationCard
// component watches for changes to the integration data to display.
export default {
  name: 'IntegrationBtns',
  props: {
    /**
     * object of { itype, query }
     * * itype - the itype to display the integration data for (if clicked)
     * * query - the value of the query to display the integration data for
     * *     (there may be multiple IPs for instance, so the value
     * *     indicates which IP to display information for)
     */
    indicator: Cont3xtIndicatorProp
  },
  computed: {
    ...mapGetters(['getIntegrationsArray', 'getLoading', 'getResults']),
    integrations () {
      return this.getIntegrationsArray.slice().sort((a, b) => {
        return a.order - b.order;
      });
    },
    buttonIntegrations () {
      return this.integrations.filter(integration =>
        this.integrationDataMap[integration.name] && integration.icon
      );
    },
    /** @returns a map of integration names to integration data objects */
    integrationDataMap () {
      return getIntegrationDataMap(this.getResults, this.indicator);
    }
  },
  methods: {
    setAsActive (integration) {
      this.$store.commit('SET_QUEUED_INTEGRATION', { indicator: this.indicator, source: integration.name });
    },
    countBadgeColor (data) {
      if (data._cont3xt.count === 0) {
        return 'secondary';
      } else if (data._cont3xt.severity === 'high') {
        return 'danger';
      } else {
        return 'success';
      }
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
