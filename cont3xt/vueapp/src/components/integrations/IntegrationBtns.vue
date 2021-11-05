<template>
  <span>
    <template v-for="(integration, source) in getIntegrations">
      <span
        :key="itype + source"
        v-if="data[itype] && data[itype][source]">
        <b-button
          size="xs"
          v-b-tooltip.hover="source"
          variant="outline-dark pull-right ml-1"
          v-if="data[itype][source][0] && integration.icon"
          @click="$store.commit('SET_DISPLAY_INTEGRATION', { source, itype, value })">
          <img
            :alt="source"
            :src="integration.icon"
            class="integration-img cursor-pointer"
          />
          <b-badge
            :variant="getLoading.failures.indexOf(source) < 0 ? 'success' : 'secondary'"
            v-if="data[itype][source][0].data._count">
            {{ data[itype][source][0].data._count }}
          </b-badge>
        </b-button>
      </span>
    </template>
  </span>
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
    ...mapGetters(['getIntegrations', 'getLoading'])
  }
};
</script>

<style scoped>
.integration-img {
  height: 25px;
  margin: 0 6px;
}
</style>
