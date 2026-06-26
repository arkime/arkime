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
            <!-- column count toggle -->
            <v-menu>
              <template #activator="{ props }">
                <v-btn
                  v-bind="props"
                  size="large"
                  variant="flat"
                  color="secondary">
                  <v-icon
                    start
                    icon="mdi-view-column" />
                  {{ columnCount }}
                  <v-icon
                    end
                    icon="mdi-menu-down" />
                </v-btn>
              </template>
              <v-list density="compact">
                <v-list-item
                  v-for="n in [2, 3]"
                  :key="n"
                  :active="columnCount === n"
                  @click="setColumnCount(n)">
                  <v-list-item-title>{{ $t('sessions.summary.columns', { n }) }}</v-list-item-title>
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
              variant="flat"
              color="secondary"
              class="dashboard-btn-group">
              <!-- v-btn-group resets its children to density-default/size-default;
                   force them back to density-compact + size-large to match the
                   sibling toolbar buttons -->
              <v-defaults-provider :defaults="{ VBtn: { density: 'compact', size: 'large' } }">
                <FieldSelectDropdown
                  :selected-fields="selectedFields"
                  :tooltip-text="$t('sessions.summary.toggleFields')"
                  :search-placeholder="$t('sessions.summary.searchFields')"
                  :include-summary-fields="true"
                  field-id-key="exp"
                  @toggle="toggleSummaryField"
                  @clear="clearSummaryFields" />
                <SummaryConfigDropdown
                  :current-config="currentDashboardConfig"
                  @load="loadSummaryConfigFromShareable"
                  @reset="resetSummaryToDefaults"
                  @message="displayMessage" />
              </v-defaults-provider>
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
        :widgets="widgets"
        :column-count="columnCount"
        @update-visualizations="updateVisualizationsData"
        @widget-config-changed="updateWidgetConfigs"
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
import { createShareableService } from '../users/ShareableService';

const DashboardService = createShareableService('summaryConfig');

// Default per-field widget view modes (mirrors the server's fieldMetadata)
const DEFAULT_VIEW_MODES = {
  ip: 'bar',
  'ip.src': 'bar',
  'ip.dst': 'bar',
  'port.src': 'bar',
  'port.dst': 'bar',
  protocols: 'pie',
  tags: 'pie',
  'ip.dst:port': 'table',
  'host.http': 'table',
  'dns.query.host': 'table'
};

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
      // Dashboard configuration
      widgets: [],
      columnCount: 2,
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
    // Field expressions currently on the dashboard (drives the field toggle dropdown)
    selectedFields: function () {
      return this.widgets.map(w => w.field);
    },
    // Current dashboard configuration for saving (layout + widgets)
    currentDashboardConfig: function () {
      return {
        columnCount: this.columnCount,
        widgets: this.widgets
      };
    }
  },
  watch: {
    '$store.state.stickyViz': function () {
      // pin toggle teleports the viz between chrome and scroll content;
      // charts/map cache geometry, so nudge their resize handling
      this.$nextTick(() => window.dispatchEvent(new Event('resize')));
    },
    // Handle fetch viz data button click (re-fetch visualizations)
    '$store.state.fetchGraphData': function (value) {
      if (value) { this.reloadSummaryView(); }
    }
  },
  created: function () {
    this.loadDashboardConfig();
  },
  methods: {
    /**
     * Builds a default widget definition for a field expression
     */
    makeWidgetDef: function (fieldExp, overrides = {}) {
      return {
        id: Utils.createRandomString(),
        field: fieldExp,
        viewMode: DEFAULT_VIEW_MODES[fieldExp] || 'bar',
        metricType: 'sessions',
        length: 20,
        order: 'desc',
        expression: '',
        height: 'standard',
        width: 'standard',
        title: '',
        ...overrides
      };
    },
    /**
     * Ensures a saved widget has an id and all expected fields
     */
    normalizeWidget: function (w) {
      return this.makeWidgetDef(w.field, {
        id: w.id || Utils.createRandomString(),
        viewMode: w.viewMode || DEFAULT_VIEW_MODES[w.field] || 'bar',
        metricType: w.metricType || 'sessions',
        length: w.length || 20,
        order: w.order || 'desc',
        expression: w.expression || '',
        height: w.height || 'standard',
        width: w.width || 'standard',
        title: w.title || ''
      });
    },
    /**
     * Default set of widgets when no saved config exists
     */
    defaultWidgets: function () {
      return Utils.getDefaultSummaryFields().map(f => this.makeWidgetDef(f));
    },
    /**
     * Loads summary data when search is triggered
     */
    loadSummary: function () {
      this.reloadSummaryView();
    },
    setColumnCount: function (n) {
      this.columnCount = n;
      this.saveDashboardConfig();
    },
    toggleSummaryField: function (fieldExp) {
      // The field dropdown is a presence toggle: turning a field off removes
      // every widget for that field (duplicates are still creatable via the
      // per-widget edit modal's change-field path), turning it on adds one.
      const matching = this.widgets.filter(w => w.field === fieldExp);
      if (matching.length) {
        this.widgets = this.widgets.filter(w => w.field !== fieldExp);
        matching.forEach(w => this.$refs.summaryView?.removeWidget?.(w.id));
      } else {
        const def = this.makeWidgetDef(fieldExp);
        this.widgets.push(def);
        this.$refs.summaryView?.addWidget?.(def);
      }
      this.saveDashboardConfig();
    },
    clearSummaryFields: function () {
      this.widgets = [];
      this.reloadSummaryView();
      this.saveDashboardConfig();
    },
    // Summary emits the full widget list after any per-widget change
    // (view mode, metric, edit, reorder, remove)
    updateWidgetConfigs: function (configs) {
      this.widgets = configs;
      this.saveDashboardConfig();
    },
    loadSummaryConfigFromShareable: function (shareable) {
      if (!shareable?.data) { return; }
      this.applyConfig(shareable.data);
      this.reloadSummaryView();
      this.saveDashboardConfig();
    },
    displayMessage: function ({ msg, type }) {
      if (type === 'danger') {
        this.error = msg;
      }
    },
    /**
     * Applies a saved config object to local state. Tolerates the legacy
     * shape ({ fields, resultsLimit, order }) and the new shape
     * ({ columnCount, widgets }).
     */
    applyConfig: function (cfg) {
      if (Array.isArray(cfg?.widgets) && cfg.widgets.length) {
        this.widgets = cfg.widgets.map(w => this.normalizeWidget(w));
      } else if (Array.isArray(cfg?.fields) && cfg.fields.length) {
        // Legacy summaryConfig: migrate fields[] into widget objects
        this.widgets = cfg.fields.map(f => this.makeWidgetDef(f.field, {
          viewMode: f.viewMode || DEFAULT_VIEW_MODES[f.field] || 'bar',
          metricType: f.metricType || 'sessions',
          length: cfg.resultsLimit || 20,
          order: cfg.order || 'desc'
        }));
      } else {
        this.widgets = this.defaultWidgets();
      }
      // Guarantee unique widget ids — hand-edited imports may collide, which
      // would leave a widget stuck loading (stream chunks are matched by id)
      const seen = new Set();
      for (const w of this.widgets) {
        if (!w.id || seen.has(w.id)) { w.id = Utils.createRandomString(); }
        seen.add(w.id);
      }
      if (cfg?.columnCount === 2 || cfg?.columnCount === 3) {
        this.columnCount = cfg.columnCount;
      }
    },
    loadDashboardConfig: async function () {
      // Prefer the user's default dashboard if one is set
      const defaultId = this.user?.settings?.defaultDashboardId;
      if (defaultId) {
        try {
          const response = await DashboardService.get(defaultId);
          if (response?.shareable?.data) {
            this.applyConfig(response.shareable.data);
            this.configLoaded = true;
            return;
          }
        } catch (err) {
          // fall through to per-user state
        }
      }

      // Otherwise restore the last per-user dashboard state
      try {
        const response = await UserService.getPageConfig('summary');
        if (response?.summaryConfig) {
          this.applyConfig(response.summaryConfig);
          this.configLoaded = true;
          return;
        }
      } catch (err) {
        // fall through to defaults
      }

      this.widgets = this.defaultWidgets();
      this.configLoaded = true;
    },
    saveDashboardConfig: function () {
      UserService.saveState(this.currentDashboardConfig, 'summary');
    },
    reloadSummaryView: function () {
      this.$nextTick(() => {
        this.$refs.summaryView?.reloadSummary?.();
      });
    },
    resetSummaryToDefaults: function () {
      this.widgets = this.defaultWidgets();
      this.columnCount = 2;
      this.reloadSummaryView();
      this.saveDashboardConfig();
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

/* The sibling toolbar buttons are density-compact + size-large = 32px tall.
   v-btn-group's density sets its own (taller) height and its children get an
   inline height:auto, so pin the group to 32px and force the child buttons to
   fill it (overriding the inline style) so the group lines up with the siblings. */
.dashboard-btn-group.v-btn-group {
  height: 32px;
}
.dashboard-btn-group :deep(.v-btn) {
  height: 100% !important;
}
</style>
