<template>
  <div class="wrap-btns">
    <template v-for="integration in integrations">
      <b-button
        size="xs"
        tabindex="0"
        variant="outline-dark"
        class="ml-1 mt-1 float-right"
        :id="`${itype}-${integration.name}-${value}`"
        :key="integration.name"
        v-if="instanceData[integration.name] && integration.icon"
        @click="$store.commit('SET_DISPLAY_INTEGRATION', { source: integration.name, itype, value })">
        <img
          :alt="integration.name"
          :src="integration.icon"
          data-testid="integration-btn-icon"
          class="integration-img cursor-pointer"
        />
        <b-badge
          class="btn-badge"
          v-if="instanceData[integration.name]._cont3xt.count !== undefined"
          :variant="countBadgeColor(instanceData[integration.name])">
          {{ instanceData[integration.name]._cont3xt.count | humanReadableNumber }}
        </b-badge>
        <b-tooltip
            :target="`${itype}-${integration.name}-${value}`">
          {{ integration.name }}
        </b-tooltip>
      </b-button>
    </template>
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

// Clicking an integration button commits to the store which integration, itype,
// and value to display integration data for. The Cont3xt component watches for
// store changes and sets the integration data in the store. the IntegrationCard
// component watches for changes to the integration data to display.
export default {
  name: 'IntegrationBtns',
  props: {
    data: { // the configured integrations
      type: Object,
      required: true
    },
    itype: { // the itype to display the integration data for (if clicked)
      type: String,
      required: true
    },
    value: { // the value of the query to display the integration data for
      type: String, // (there may be multiple IPs for instance, so the value
      required: true // indicates which IP to display information for)
    }
  },
  computed: {
    ...mapGetters(['getIntegrationsArray', 'getLoading']),
    integrations () {
      return this.getIntegrationsArray.slice().sort((a, b) => {
        return a.order - b.order;
      });
    },
    /** @returns a map of integration names to integration data objects */
    instanceData () {
      const instanceDataMap = {};
      for (const integration of this.integrations) {
        const dataArrayForIntegration = this.data?.[this.itype]?.[integration.name];
        if (dataArrayForIntegration == null) { continue; }

        // look for data object matching query in the integration's data-array
        for (const queryElement of dataArrayForIntegration) {
          if (this.value === queryElement?._query) {
            instanceDataMap[integration.name] = queryElement.data;
            break;
          }
        }
      }
      return instanceDataMap;
    }
  },
  methods: {
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
