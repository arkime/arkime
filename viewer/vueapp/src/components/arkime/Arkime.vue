<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <page-layout class="arkime-page">
    <template #chrome>
      <ArkimeCollapsible>
        <div class="page-toolbar">
          <!-- search navbar -->
          <arkime-search
            :hide-actions="true"
            @change-search="loadSummary" />

          <!-- toolbar row -->
          <div class="d-flex justify-start align-center ms-2 gap-2 page-subnav">
            <!-- results per widget dropdown -->
            <v-menu>
              <template #activator="{ props }">
                <v-btn
                  v-bind="props"
                  size="large"
                  variant="flat"
                  color="secondary">
                  {{ summaryResultsLimit }}
                  <v-icon
                    end
                    icon="mdi-menu-down" />
                </v-btn>
              </template>
              <v-list density="compact">
                <v-list-item
                  v-for="opt in [10, 20, 50, 100]"
                  :key="opt"
                  :active="summaryResultsLimit === opt"
                  @click="updateSummaryResultsLimit(opt)">
                  <v-list-item-title>{{ opt }}</v-list-item-title>
                </v-list-item>
              </v-list>
            </v-menu>

            <!-- top/bottom results toggle -->
            <v-menu>
              <template #activator="{ props }">
                <v-btn
                  v-bind="props"
                  size="large"
                  variant="flat"
                  color="secondary">
                  {{ summaryOrder === 'asc' ? 'Bottom' : 'Top' }}
                  <v-icon
                    end
                    icon="mdi-menu-down" />
                </v-btn>
              </template>
              <v-list density="compact">
                <v-list-item
                  :active="summaryOrder === 'desc'"
                  @click="updateSummaryOrder('desc')">
                  <v-list-item-title>Top</v-list-item-title>
                </v-list-item>
                <v-list-item
                  :active="summaryOrder === 'asc'"
                  @click="updateSummaryOrder('asc')">
                  <v-list-item-title>Bottom</v-list-item-title>
                </v-list-item>
              </v-list>
            </v-menu>

            <!-- export all charts as PNG -->
            <v-btn
              :aria-label="$t('sessions.summary.exportAllPNG')"
              size="large"
              variant="flat"
              color="secondary"
              @click="exportAllPNG">
              <v-icon icon="mdi-download" />
              <v-tooltip
                activator="parent"
                :open-delay="500">
                {{ $t('sessions.summary.exportAllPNG') }} — {{ $t('sessions.summary.exportPNGTableWarning') }}
              </v-tooltip>
            </v-btn>

            <!-- summary field visibility + config group -->
            <v-btn-group
              divided
              density="compact"
              variant="flat"
              color="secondary">
              <FieldSelectDropdown
                :selected-fields="summaryFields"
                :tooltip-text="$t('sessions.summary.toggleFields')"
                :search-placeholder="$t('sessions.summary.searchFields')"
                :include-summary-fields="true"
                field-id-key="exp"
                @toggle="toggleSummaryField"
                @clear="clearSummaryFields" />
              <SummaryConfigDropdown
                :current-config="currentSummaryConfig"
                @load="loadSummaryConfigFromShareable"
                @reset="resetSummaryToDefaults"
                @message="displayMessage" />
            </v-btn-group>

            <!-- cancel loading button -->
            <v-btn
              v-if="summaryStreaming"
              size="large"
              variant="flat"
              color="warning"
              @click="cancelSummaryLoading">
              <v-icon icon="mdi-cancel" />&nbsp;
              {{ $t('common.cancel') }}
            </v-btn>
          </div>
        </div>
      </ArkimeCollapsible>
      <!-- pinned visualizations land here (teleported from below) -->
      <div id="viz-pin-anchor" />
    </template>

    <!-- visualizations: pinned = chrome row above the scroll container,
         unpinned = scrolls away with the content -->
    <teleport
      defer
      to="#viz-pin-anchor"
      :disabled="!stickyViz">
      <arkime-visualizations
        v-if="graphData && showToolBars"
        :primary="true"
        :map-data="mapData"
        :graph-data="graphData"
        :timeline-data-filters="timelineDataFilters"
        @spanning-change="reloadSummaryView" />
    </teleport> <!-- /visualizations -->

    <!-- error message -->
    <v-alert
      v-if="error"
      type="error"
      variant="tonal"
      density="compact"
      closable
      class="mx-2"
      @click:close="error = ''">
      {{ error }}
    </v-alert>

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
        @streaming-state="summaryStreaming = $event"
        @canceled-state="summaryCanceled = $event" />
    </div>

    <!-- stale data warning after cancellation -->
    <v-alert
      v-if="summaryCanceled && !summaryStreaming"
      type="warning"
      variant="tonal"
      density="compact"
      closable
      class="position-fixed fixed-bottom m-0 rounded-0"
      @click:close="summaryCanceled = false">
      {{ $t('sessions.summary.canceledSearch') }}
      — {{ $t('sessions.summary.staleDataWarning') }}
      <v-btn
        color="success"
        variant="flat"
        size="x-small"
        density="comfortable"
        class="ms-2"
        @click="retryAllFailed">
        <v-icon
          icon="mdi-refresh"
          class="me-1" />
        {{ $t('sessions.summary.retryAllFailed') }}
      </v-btn>
    </v-alert>
  </page-layout>
</template>

<script>
import ArkimeSearch from '../search/Search.vue';
import ArkimeCollapsible from '../utils/CollapsibleWrapper.vue';
import PageLayout from '../utils/PageLayout.vue';
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
    PageLayout,
    ArkimeVisualizations,
    ArkimeSummaryView,
    FieldSelectDropdown,
    SummaryConfigDropdown
  },
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
      configLoaded: false,
      summaryStreaming: false,
      summaryCanceled: false
    };
  },
  computed: {
    showToolBars: function () {
      return this.$store.state.showToolBars;
    },
    stickyViz: function () {
      return this.$store.state.stickyViz;
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
    '$store.state.stickyViz': function () {
      // pin toggle teleports the viz between chrome and scroll content;
      // charts/map cache geometry, so nudge their resize handling
      this.$nextTick(() => window.dispatchEvent(new Event('resize')));
    },
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
        this.$refs.summaryView?.removeField?.(fieldExp);
      } else {
        this.summaryFields.push(fieldExp);
        this.$refs.summaryView?.addField?.(fieldExp);
      }
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
    cancelSummaryLoading: function () {
      this.$refs.summaryView?.cancelLoading?.();
    },
    retryAllFailed: function () {
      this.$refs.summaryView?.retryAllFailed?.();
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
.arkime-content {
  margin-right: 0.5rem;
}
</style>
