<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-flex flex-row text-no-wrap severity-badge-container">
    <span
      v-if="severityTypes.some(severity => severityCounts[severity])"
      class="mr-1"
    >
      <span :id="`${indicatorId}-severity-counts`">
        <template v-for="severity in severityTypes">
          <c3-badge
            v-if="severityCounts[severity]"
            :key="severity"
            class="severity-badge"
            :variant="severity"
          >
            {{ severityCounts[severity] }}
          </c3-badge>
        </template>
      </span>
      <interactive-tooltip
        :target="`${indicatorId}-severity-counts`"
        location="top"
        class="ma-1"
      >
        <div class="d-flex flex-column ga-1 ma-1">
          <template v-for="severity in severityTypes">
            <div
              v-if="severityCounts[severity]"
              :key="severity"
              class="d-flex flex-row align-center"
            >
              <span class="severity-emoji align-self-center mr-2">
                {{ severityEmojiMap[severity] }}
              </span>
              <integration-btns
                hide-overview-selector
                :margin-bottom="false"
                :indicator-id="indicatorId"
                :count-severity-filter="severity"
              />
            </div>
          </template>
        </div>
      </interactive-tooltip>
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
import InteractiveTooltip from '@/utils/InteractiveTooltip.vue';
export default {
  name: 'IntegrationSeverityCounts',
  components: { IntegrationBtns, InteractiveTooltip },
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
.severity-badge {
  font-size: 100% !important;
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
