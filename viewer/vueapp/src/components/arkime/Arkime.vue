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

          <!-- top/bottom results toggle -->
          <b-dropdown
            size="sm"
            variant="secondary"
            class="ms-2"
            :text="summaryOrder === 'asc' ? 'Bottom' : 'Top'">
            <b-dropdown-item
              :active="summaryOrder === 'desc'"
              @click="updateSummaryOrder('desc')">
              Top
            </b-dropdown-item>
            <b-dropdown-item
              :active="summaryOrder === 'asc'"
              @click="updateSummaryOrder('asc')">
              Bottom
            </b-dropdown-item>
          </b-dropdown>

          <!-- export all charts as PNG -->
          <button
            id="exportAllPNGBtn"
            class="btn btn-sm btn-secondary ms-2"
            @click="exportAllPNG">
            <span class="fa fa-download" />
          </button>
          <BTooltip target="exportAllPNGBtn">
            {{ $t('sessions.summary.exportAllPNG') }} — {{ $t('sessions.summary.exportPNGTableWarning') }}
          </BTooltip>

          <!-- summary field visibility dropdown -->
          <FieldSelectDropdown
            class="ms-2"
            :selected-fields="summaryFields"
            :tooltip-text="$t('sessions.summary.toggleFields')"
            :search-placeholder="$t('sessions.summary.searchFields')"
            :include-summary-fields="true"
            field-id-key="exp"
            @toggle="toggleSummaryField"
            @clear="clearSummaryFields" />

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
      :timeline-data-filters="timelineDataFilters"
      @spanning-change="reloadSummaryView" />

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
        v-if="configLoaded"
        ref="summaryView"
        :summary-fields="summaryFields"
        @update-visualizations="updateVisualizationsData"
        @reorder-fields="reorderSummaryFields"
        @widget-config-changed="updateWidgetConfigs"
        @remove-field="toggleSummaryField"
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
import FieldService from '../search/FieldService';
import UserService from '../users/UserService';

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
      summaryOrder: this.$route.query.summaryOrder || 'desc',
      summaryFields: [],
      widgetConfigs: [],
      // Visualization data
      mapData: undefined,
      graphData: undefined,
      // UI state
      error: '',
      configLoaded: false
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
      return this.user.settings.timelineDataFilters.map(i => FieldService.getField(i));
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
        resultsLimit: this.summaryResultsLimit,
        order: this.summaryOrder
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
    },
    // Handle browser back/forward navigation for summaryOrder
    '$route.query.summaryOrder': function (newValue) {
      const newOrder = newValue || 'desc';
      if (this.summaryOrder !== newOrder) {
        this.summaryOrder = newOrder;
        this.reloadSummaryView();
      }
    },
    // Handle fetch viz data button click (re-fetch visualizations)
    '$store.state.fetchGraphData': function (value) {
      if (value) { this.reloadSummaryView(); }
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
    updateSummaryResultsLimit: async function (newLimit) {
      this.summaryResultsLimit = newLimit;
      await this.$router.replace({
        query: { ...this.$route.query, summaryLength: newLimit }
      });
      this.reloadSummaryView();
      this.saveSummaryConfig();
    },
    updateSummaryOrder: async function (newOrder) {
      this.summaryOrder = newOrder;
      await this.$router.replace({
        query: { ...this.$route.query, summaryOrder: newOrder }
      });
      this.reloadSummaryView();
      this.saveSummaryConfig();
    },
    toggleSummaryField: function (fieldExp) {
      const index = this.summaryFields.indexOf(fieldExp);
      if (index >= 0) {
        this.summaryFields.splice(index, 1);
      } else {
        this.summaryFields.push(fieldExp);
      }
      this.reloadSummaryView();
      this.saveSummaryConfig();
    },
    clearSummaryFields: function () {
      this.summaryFields = [];
      this.reloadSummaryView();
      this.saveSummaryConfig();
    },
    reorderSummaryFields: function ({ oldIndex, newIndex }) {
      const field = this.summaryFields.splice(oldIndex, 1)[0];
      this.summaryFields.splice(newIndex, 0, field);
      this.saveSummaryConfig();
    },
    updateWidgetConfigs: function (configs) {
      this.widgetConfigs = configs;
      this.saveSummaryConfig();
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

      if (configData.order) {
        this.updateSummaryOrder(configData.order);
      }

      if (configData.resultsLimit) {
        this.updateSummaryResultsLimit(configData.resultsLimit);
      } else {
        this.reloadSummaryView();
        this.saveSummaryConfig();
      }
    },
    displayMessage: function ({ msg, type }) {
      if (type === 'danger') {
        this.error = msg;
      }
    },
    loadSummaryConfig: async function () {
      try {
        const response = await UserService.getPageConfig('summary');
        if (response?.summaryConfig?.fields?.length) {
          const config = response.summaryConfig;
          this.summaryFields = config.fields.map(f => f.field);
          this.widgetConfigs = config.fields.map(f => ({
            field: f.field,
            viewMode: f.viewMode || 'bar',
            metricType: f.metricType || 'sessions'
          }));
          if (config.order) {
            this.summaryOrder = config.order;
          }
          if (config.resultsLimit) {
            this.summaryResultsLimit = config.resultsLimit;
          }
          this.configLoaded = true;
          return;
        }
      } catch (err) {
        // fall through to defaults
      }
      this.summaryFields = Utils.getDefaultSummaryFields();
      this.configLoaded = true;
    },
    saveSummaryConfig: function () {
      UserService.saveState(this.currentSummaryConfig, 'summary');
    },
    reloadSummaryView: function () {
      this.$nextTick(() => {
        this.$refs.summaryView?.reloadSummary?.();
      });
    },
    resetSummaryToDefaults: function () {
      this.summaryFields = Utils.getDefaultSummaryFields();
      this.widgetConfigs = [];
      this.summaryOrder = 'desc';
      this.updateSummaryOrder('desc');
      this.updateSummaryResultsLimit(20);
    },
    exportAllPNG: function () {
      this.$refs.summaryView?.exportAllPNG?.();
    },
    updateVisualizationsData: function (data) {
      this.mapData = data.mapData;
      this.graphData = data.graphData;
    }
  }
};
</script>

<style scoped>
.arkime-page.hide-tool-bars .fixed-header {
  display: none;
}

.arkime-content {
  margin-right: 0.5rem;
}
</style>
