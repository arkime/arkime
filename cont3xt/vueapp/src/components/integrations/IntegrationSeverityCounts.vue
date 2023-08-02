<template>
  <div class="d-flex flex-row text-nowrap severity-badge-container">
    <span v-if="severityTypes.some(color => severityCounts[color])" class="mr-1">
      <span :id="`severity-counts:${indicatorId(indicator)}`">
        <template v-for="color in severityTypes">
          <b-badge v-if="severityCounts[color]"
                   :key="color"
                   class="severity-badge"
                   :variant="color">
            {{ severityCounts[color] }}
          </b-badge>
        </template>
      </span>
      <b-tooltip :target="`severity-counts:${indicatorId(indicator)}`" placement="top">
        <div class="d-flex flex-column gap-1">
          <template v-for="color in severityTypes">
            <div v-if="severityCounts[color]" :key="color" class="d-flex flex-row">
              <span class="severity-emoji align-self-center mr-2">
                {{ severityEmojiMap[color] }}
              </span>
              <integration-btns
                  :indicator="indicator"
                  :count-severity-filter="color"/>
            </div>
          </template>
        </div>
      </b-tooltip>
    </span>
  </div>
</template>

<script>
import {
  Cont3xtIndicatorProp,
  getIntegrationDataMap,
  shouldDisplayCountedIntegrationBtn,
  integrationCountColor, indicatorId
} from '@/utils/cont3xtUtil';
import { mapGetters } from 'vuex';
import IntegrationBtns from '@/components/integrations/IntegrationBtns.vue';

export default {
  name: 'IntegrationSeverityCounts',
  components: { IntegrationBtns },
  methods: { indicatorId },
  props: {
    indicator: Cont3xtIndicatorProp
  },
  data () {
    return {
      severityTypes: ['success', 'secondary', 'danger'],
      severityEmojiMap: {
        success: '😀',
        secondary: '😐',
        danger: '😡'
      }
    };
  },
  computed: {
    ...mapGetters(['getResults', 'getIntegrationsArray']),
    integrationDataMap () {
      return getIntegrationDataMap(this.getResults, this.indicator);
    },
    severityCounts () {
      const counts = { secondary: 0, success: 0, danger: 0 };

      for (const integration of this.getIntegrationsArray) {
        const integrationData = this.integrationDataMap[integration.name];
        if (shouldDisplayCountedIntegrationBtn(integration, integrationData)) {
          counts[integrationCountColor(integrationData)]++;
        }
      }

      return counts;
    }
  }
};
</script>

<style scoped>
.severity-badge-container {
  height: min-content;
  align-self: center;
}

/* first and/or middle */
.severity-badge:not(:last-child) {
  border-top-right-radius: 0;
  border-bottom-right-radius: 0;
}
/* middle and/or last */
.severity-badge:not(:first-child) {
  border-top-left-radius: 0;
  border-bottom-left-radius: 0;
}

.severity-emoji {
  font-size: 1.5rem;
}
</style>
