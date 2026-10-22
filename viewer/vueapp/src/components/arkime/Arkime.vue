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
          <div class="d-flex justify-start align-center ms-2 gap-2 page-subnav page-subnav--tall">
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
import UserService from '../users/UserService';
import { createShareableService } from '../users/ShareableService';
import { CHART_PALETTES, normalizePalette } from '../summary/widgets/chartColors';
import { toV6Shape, makeWidgetDef, defaultWidgets, widgetsFromConfig } from '../summary/dashboardConfig';

const DashboardService = createShareableService('summaryConfig');

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
      return {
        colorScheme: this.colorScheme,
        widgets: this.widgets,
        ...toV6Shape(this.widgets)
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
    makeWidgetDef,
    defaultWidgets,
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
      this.widgets = widgetsFromConfig(cfg);
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
