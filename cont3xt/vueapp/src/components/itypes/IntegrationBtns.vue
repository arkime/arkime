<template>
  <span>
    <template v-for="(value, key) in getIntegrations">
      <b-button
        :key="key"
        size="sm"
        v-b-tooltip.hover="key"
        variant="outline-dark pull-right ml-1"
        v-if="data[itype][key] && value.icon"
        @click="$store.commit('SET_DISPLAY_INTEGRATION', { source: key, itype })">
        <img
          :alt="key"
          :src="value.icon"
          class="integration-img cursor-pointer"
        />
        <b-badge
          :variant="getLoading.failures.indexOf(key) < 0 ? 'success' : 'secondary'"
          v-if="data[itype][key].data._count">
          {{ data[itype][key].data._count }}
        </b-badge>
      </b-button>
    </template>
  </span>
</template>

<script>
import { mapGetters } from 'vuex';

export default {
  name: 'IntegrationBtns',
  props: {
    data: Object,
    itype: String
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
