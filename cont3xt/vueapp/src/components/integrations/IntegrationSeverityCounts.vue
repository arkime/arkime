<template>
  <div class="d-flex flex-row text-nowrap severity-badge-container">
    <span v-if="severityTypes.some(severity => severityCounts[severity])" class="mr-1">
      <span :id="`${indicatorId}-severity-counts`">
        <template v-for="severity in severityTypes">
          <b-badge v-if="severityCounts[severity]"
                   :key="severity"
                   class="severity-badge"
                   :variant="severity">
            {{ severityCounts[severity] }}
          </b-badge>
        </template>
      </span>
      <b-tooltip :target="`${indicatorId}-severity-counts`" placement="top">
        <div class="d-flex flex-column gap-1">
          <template v-for="severity in severityTypes">
            <div v-if="severityCounts[severity]" :key="severity" class="d-flex flex-row">
              <span class="severity-emoji align-self-center mr-2">
                {{ severityEmojiMap[severity] }}
              </span>
              <integration-btns
                  :indicator-id="indicatorId"
                  :count-severity-filter="severity"/>
            </div>
          </template>
        </div>
      </b-tooltip>
    </span>
  </div>
</template>

<script>
import {
  getIntegrationDataMap,
  shouldDisplayCountedIntegrationBtn,
  integrationCountSeverity, indicatorFromId
} from '@/utils/cont3xtUtil';
import { mapGetters } from 'vuex';
import IntegrationBtns from '@/components/integrations/IntegrationBtns.vue';
export default {
  name: 'IntegrationSeverityCounts',
  components: { IntegrationBtns },
  props: {
    indicatorId: {
      type: String,
      required: true
    }
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
      return getIntegrationDataMap(this.getResults, indicatorFromId(this.indicatorId));
    },
    severityCounts () {
      const counts = { secondary: 0, success: 0, danger: 0 };
      for (const integration of this.getIntegrationsArray) {
        const integrationData = this.integrationDataMap[integration.name];
        if (shouldDisplayCountedIntegrationBtn(integration, integrationData)) {
          counts[integrationCountSeverity(integrationData)]++;
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
