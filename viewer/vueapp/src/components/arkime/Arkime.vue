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
            <!-- chart color palette -->
            <v-menu>
              <template #activator="{ props }">
                <v-btn
                  v-bind="props"
                  size="large"
                  variant="flat"
                  color="secondary"
                  :aria-label="$t('sessions.summary.colorPalette')">
                  <v-icon icon="mdi-palette" />
                  <v-icon
                    end
                    icon="mdi-menu-down" />
                  <v-tooltip
                    activator="parent"
                    :open-delay="500">
                    {{ $t('sessions.summary.colorPalette') }}
                  </v-tooltip>
                </v-btn>
              </template>
              <v-list density="compact">
                <v-list-item
                  v-for="p in palettes"
                  :key="p.value"
                  :active="colorScheme === p.value"
                  @click="setColorScheme(p.value)">
                  <v-list-item-title>{{ p.label }}</v-list-item-title>
                </v-list-item>
              </v-list>
            </v-menu>

            <!-- add a widget -->
            <v-btn
              size="large"
              variant="flat"
              color="secondary"
              @click="addDashboardWidget">
              <v-icon
                start
                icon="mdi-plus" />
              {{ $t('sessions.summary.addWidget') }}
            </v-btn>

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

            <!-- save / load dashboard config -->
            <v-btn-group
              variant="flat"
              color="secondary"
              class="dashboard-btn-group">
              <!-- v-btn-group resets its children to density-default/size-default;
                   force them back to density-compact + size-large to match the
                   sibling toolbar buttons -->
              <v-defaults-provider :defaults="{ VBtn: { density: 'compact', size: 'large' } }">
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
    </template>

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
        :color-scheme="colorScheme"
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
import ArkimeSummaryView from '../summary/Summary.vue';
import SummaryConfigDropdown from '../summary/SummaryConfigDropdown.vue';
import Utils from '../utils/utils';
import UserService from '../users/UserService';
import { createShareableService } from '../users/ShareableService';
import { CHART_PALETTES, normalizePalette } from '../summary/widgets/chartColors';
import { DEFAULT_VIEW_MODES, isFieldMode } from '../summary/widgets/viewModes';

const DashboardService = createShareableService('summaryConfig');

// widgets span 1-4 columns and 1-8 rows; anything missing/invalid falls back to
// `def` (rows allow a larger max since the grid uses a fine 160px row unit)
const spanOrDefault = (v, def, max = 4) => {
  const n = parseInt(v, 10);
  return n >= 1 && n <= max ? n : def;
};

// map v7-only field view modes to the closest mode a v6 viewer can render
// (v6 knows only bar/pie/table) for the dual-written legacy fields[] shape
const V6_VIEW_MODES = { bar: 'bar', pie: 'pie', table: 'table', intersection: 'table', heatmap: 'bar', treemap: 'bar' };

export default {
  name: 'Arkime',
  components: {
    ArkimeSearch,
    ArkimeCollapsible,
    PageLayout,
    ArkimeSummaryView,
    SummaryConfigDropdown
  },
  data: function () {
    return {
      // Dashboard configuration
      widgets: [],
      colorScheme: 'rainbow',
      palettes: CHART_PALETTES,
      // UI state
      error: '',
      configLoaded: false,
      summaryStreaming: false,
      summaryCanceled: false
    };
  },
  computed: {
    user: function () {
      return this.$store.state.user;
    },
    // Current dashboard configuration for saving (layout + widgets). Also
    // dual-writes the v6 summaryConfig shape (fields[]/resultsLimit/order) so a
    // v6 viewer in a mixed-version cluster (or after a downgrade) can still read
    // a dashboard saved by v7 — v6 reads fields[] and ignores widgets/colorScheme.
    // Session-wide widgets (timeline/map/stats/time) have no v6 equivalent and
    // are omitted from fields[]. See SHAREABLES.md.
    currentDashboardConfig: function () {
      // only true v6-style chart widgets (bar/pie/table/...) project into the
      // legacy fields[]; the map's geo field and session-wide widgets do not
      const fieldWidgets = this.widgets.filter(w => w.field && isFieldMode(w.viewMode));
      return {
        colorScheme: this.colorScheme,
        widgets: this.widgets,
        fields: fieldWidgets.map(w => ({
          field: w.field,
          viewMode: V6_VIEW_MODES[w.viewMode] || 'bar',
          metricType: w.metricType || 'sessions'
        })),
        resultsLimit: fieldWidgets[0]?.length || 20,
        order: fieldWidgets[0]?.order || 'desc'
      };
    }
  },
  watch: {
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
        fields: fieldExp ? [fieldExp] : [], // 1-3 fields for multi-field widgets
        viewMode: DEFAULT_VIEW_MODES[fieldExp] || 'bar',
        metricType: 'sessions',
        metrics: [], // table multi-metric columns (falls back to metricType)
        sortMetric: 'sessions',
        length: 20,
        order: 'desc',
        expression: '',
        view: '',
        height: 3, // 3 × 160px row unit = 480px (chart default)
        width: 2,
        title: '',
        ...overrides
      };
    },
    /**
     * Ensures a saved widget has an id and all expected fields
     */
    normalizeWidget: function (w) {
      // fields[] is the source of truth (1-3); field stays = fields[0] for back-compat
      const fields = Array.isArray(w.fields) && w.fields.length
        ? w.fields.slice(0, 3)
        : (w.field ? [w.field] : []);
      return this.makeWidgetDef(fields[0] || '', {
        id: w.id || Utils.createRandomString(),
        fields,
        viewMode: w.viewMode || DEFAULT_VIEW_MODES[w.field] || 'bar',
        metricType: w.metricType || 'sessions',
        metrics: Array.isArray(w.metrics) ? w.metrics.slice(0, 4) : [],
        sortMetric: w.sortMetric || w.metricType || 'sessions',
        length: w.length || 20,
        order: w.order || 'desc',
        expression: w.expression || '',
        view: w.view || '',
        height: spanOrDefault(w.height, 3, 8),
        width: spanOrDefault(w.width, 2),
        title: w.title || ''
      });
    },
    /**
     * Curated starter dashboard (no saved config): the capture-wide band
     * (stats/time/timeline), a geo map + top-talkers bar, then one of each
     * aggregating visualization spanning sessions / bytes / packets metrics.
     */
    defaultWidgets: function () {
      return [
        // capture-wide summary band
        this.makeWidgetDef('', { viewMode: 'stats', width: 2, height: 1 }),
        this.makeWidgetDef('', { viewMode: 'time', width: 2, height: 1 }),
        this.makeWidgetDef('', { viewMode: 'timeline', width: 4, height: 2 }),
        // geo distribution + all-IP top talkers (bar)
        this.makeWidgetDef('country.src', { viewMode: 'map', width: 2, height: 3 }),
        this.makeWidgetDef('ip', { viewMode: 'bar', width: 2, height: 3, length: 15 }),
        // protocols breakdown (pie)
        this.makeWidgetDef('protocols', { viewMode: 'pie', width: 2, height: 3, length: 8 }),
        // multi-metric table: Dst IP:Port ranked by bytes, with session/packet columns
        this.makeWidgetDef('ip.dst:port', {
          viewMode: 'table',
          width: 2,
          height: 3,
          metrics: ['sessions', 'bytes', 'packets'],
          sortMetric: 'bytes',
          metricType: 'bytes'
        }),
        // Dst IP heatmap by packets over time
        this.makeWidgetDef('ip.dst', { viewMode: 'heatmap', width: 2, height: 3, metricType: 'packets' }),
        // Src IP treemap sized by bytes
        this.makeWidgetDef('ip.src', { viewMode: 'treemap', width: 2, height: 3, metricType: 'bytes' }),
        // Src → Dst IP combinations (intersection)
        this.makeWidgetDef('ip.src', { viewMode: 'intersection', width: 4, height: 3, fields: ['ip.src', 'ip.dst'] })
      ];
    },
    /**
     * Loads summary data when search is triggered
     */
    loadSummary: function () {
      this.reloadSummaryView();
    },
    setColorScheme: function (scheme) {
      this.colorScheme = scheme;
      this.saveDashboardConfig();
    },
    addDashboardWidget: function () {
      // a new widget has no field yet — add it and open the editor to configure.
      // Don't persist until it's configured (saved) or it'll be pruned on cancel.
      const def = this.makeWidgetDef('');
      this.widgets.push(def);
      this.$refs.summaryView?.addWidget?.(def);
      this.$refs.summaryView?.openEdit?.(def.id);
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
     * ({ colorScheme, widgets }).
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
      this.colorScheme = normalizePalette(cfg?.colorScheme);
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
      this.colorScheme = 'rainbow';
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
