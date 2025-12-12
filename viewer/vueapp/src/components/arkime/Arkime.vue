<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div
    class="arkime-page"
    :class="{'hide-tool-bars': !showToolBars}">
    <ArkimeCollapsible>
      <span class="fixed-header">
        <!-- search navbar -->
        <arkime-search
          :hide-actions="true"
          @change-search="loadSummary"
          @recalc-collapse="$emit('recalc-collapse')" />

        <!-- toolbar row -->
        <div class="d-flex justify-content-start align-items-center m-1">
          <!-- results per widget dropdown -->
          <b-dropdown
            size="sm"
            variant="secondary"
            class="ms-2"
            :text="String(summaryResultsLimit)">
            <b-dropdown-item
              :active="summaryResultsLimit === 10"
              @click="updateSummaryResultsLimit(10)">
              10
            </b-dropdown-item>
            <b-dropdown-item
              :active="summaryResultsLimit === 20"
              @click="updateSummaryResultsLimit(20)">
              20
            </b-dropdown-item>
            <b-dropdown-item
              :active="summaryResultsLimit === 50"
              @click="updateSummaryResultsLimit(50)">
              50
            </b-dropdown-item>
            <b-dropdown-item
              :active="summaryResultsLimit === 100"
              @click="updateSummaryResultsLimit(100)">
              100
            </b-dropdown-item>
          </b-dropdown>

          <!-- summary field visibility dropdown -->
          <FieldSelectDropdown
            class="ms-2"
            :selected-fields="summaryFields"
            :tooltip-text="$t('sessions.summary.toggleFields')"
            :search-placeholder="$t('sessions.summary.searchFields')"
            :include-summary-fields="true"
            field-id-key="exp"
            @toggle="toggleSummaryField" />

          <!-- summary config dropdown -->
          <SummaryConfigDropdown
            class="ms-2"
            :current-config="currentSummaryConfig"
            @load="loadSummaryConfigFromShareable"
            @reset="resetSummaryToDefaults"
            @message="displayMessage" />
        </div>
      </span>
    </ArkimeCollapsible>

    <!-- visualizations -->
    <arkime-visualizations
      v-if="graphData && showToolBars"
      :primary="true"
      :map-data="mapData"
      :graph-data="graphData"
      :timeline-data-filters="timelineDataFilters" />

    <!-- error message -->
    <div
      v-if="error"
      class="alert alert-danger mx-2">
      <span class="fa fa-exclamation-triangle me-1" />
      {{ error }}
      <button
        type="button"
        class="btn-close float-end"
        @click="error = ''" />
    </div>

    <!-- summary content -->
    <div class="arkime-content ms-2">
      <arkime-summary-view
        ref="summaryView"
        :summary-fields="summaryFields"
        @update-visualizations="updateVisualizationsData"
        @reorder-fields="reorderSummaryFields"
        @widget-config-changed="updateWidgetConfigs"
        @recalc-collapse="$emit('recalc-collapse')" />
    </div>
  </div>
</template>

<script>
import ArkimeSearch from '../search/Search.vue';
import ArkimeCollapsible from '../utils/CollapsibleWrapper.vue';
import ArkimeVisualizations from '../visualizations/Visualizations.vue';
import ArkimeSummaryView from '../summary/Summary.vue';
import FieldSelectDropdown from '../utils/FieldSelectDropdown.vue';
import SummaryConfigDropdown from '../summary/SummaryConfigDropdown.vue';
import Utils from '../utils/utils';

export default {
  name: 'Arkime',
  components: {
    ArkimeSearch,
    ArkimeCollapsible,
    ArkimeVisualizations,
    ArkimeSummaryView,
    FieldSelectDropdown,
    SummaryConfigDropdown
  },
  emits: ['recalc-collapse'],
  data: function () {
    return {
      // Summary configuration
      summaryResultsLimit: parseInt(this.$route.query.summaryLength) || 20,
      summaryFields: [],
      widgetConfigs: [],
      // Visualization data
      mapData: undefined,
      graphData: undefined,
      // UI state
      error: ''
    };
  },
  computed: {
    showToolBars: function () {
      return this.$store.state.showToolBars;
    },
    user: function () {
      return this.$store.state.user;
    },
    timelineDataFilters: function () {
      if (!this.user?.settings?.timelineDataFilters) {
        return [];
      }
      return this.user.settings.timelineDataFilters;
    },
    /**
     * Returns the current summary configuration for saving
     * Combines summaryFields with widgetConfigs and resultsLimit
     */
    currentSummaryConfig: function () {
      const fields = this.summaryFields.map(fieldExp => {
        const widgetConfig = this.widgetConfigs.find(w => w.field === fieldExp);
        return {
          field: fieldExp,
          viewMode: widgetConfig?.viewMode || 'bar',
          metricType: widgetConfig?.metricType || 'sessions'
        };
      });

      return {
        fields,
        resultsLimit: this.summaryResultsLimit
      };
    }
  },
  watch: {
    // Handle browser back/forward navigation for summaryLength
    '$route.query.summaryLength': function (newValue) {
      const newLimit = parseInt(newValue) || 20;
      if (this.summaryResultsLimit !== newLimit) {
        this.summaryResultsLimit = newLimit;
        this.reloadSummaryView();
      }
    }
  },
  created: function () {
    this.loadSummaryConfig();
  },
  methods: {
    /**
     * Loads summary data when search is triggered
     */
    loadSummary: function () {
      this.reloadSummaryView();
    },
    updateSummaryResultsLimit: function (newLimit) {
      this.summaryResultsLimit = newLimit;
      this.$router.replace({
        query: { ...this.$route.query, summaryLength: newLimit }
      });
      this.reloadSummaryView();
    },
    toggleSummaryField: function (fieldExp) {
      const index = this.summaryFields.indexOf(fieldExp);
      if (index >= 0) {
        this.summaryFields.splice(index, 1);
      } else {
        this.summaryFields.push(fieldExp);
      }
      this.reloadSummaryView();
    },
    reorderSummaryFields: function ({ oldIndex, newIndex }) {
      const field = this.summaryFields.splice(oldIndex, 1)[0];
      this.summaryFields.splice(newIndex, 0, field);
    },
    updateWidgetConfigs: function (configs) {
      this.widgetConfigs = configs;
    },
    loadSummaryConfigFromShareable: function (shareable) {
      const configData = shareable.data;
      if (!configData?.fields?.length) {
        return;
      }

      this.summaryFields = configData.fields.map(f => f.field);
      this.widgetConfigs = configData.fields.map(f => ({
        field: f.field,
        viewMode: f.viewMode || 'bar',
        metricType: f.metricType || 'sessions'
      }));

      if (configData.resultsLimit) {
        this.updateSummaryResultsLimit(configData.resultsLimit);
      } else {
        this.reloadSummaryView();
      }
    },
    displayMessage: function ({ msg, type }) {
      if (type === 'danger') {
        this.error = msg;
      }
    },
    loadSummaryConfig: function () {
      this.summaryFields = Utils.getDefaultSummaryFields();
    },
    reloadSummaryView: function () {
      this.$nextTick(() => {
        this.$refs.summaryView?.reloadSummary?.();
      });
    },
    resetSummaryToDefaults: function () {
      this.summaryFields = Utils.getDefaultSummaryFields();
      this.widgetConfigs = [];
      this.updateSummaryResultsLimit(20);
    },
    updateVisualizationsData: function (data) {
      this.mapData = data.mapData;
      this.graphData = data.graphData;
    }
  }
};
</script>

<style scoped>
.arkime-page {
  margin-top: -6px;
}

.arkime-page.hide-tool-bars .fixed-header {
  display: none;
}

.arkime-content {
  margin-right: 0.5rem;
}
</style>
