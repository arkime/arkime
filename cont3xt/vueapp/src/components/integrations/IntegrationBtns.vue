<template>
  <span>
    <template v-for="(integration, source) in getIntegrations">
      <span
        :key="itype + source"
        v-if="data[itype] && data[itype][source]">
        <b-button
          size="sm"
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

export default {
  name: 'IntegrationBtns',
  props: {
    data: Object,
    itype: String,
    value: String
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
