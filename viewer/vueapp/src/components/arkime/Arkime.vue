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
                  @saved="onDashboardSaved"
                  @changed="dashboardsVersion++"
                  @reset="resetSummaryToDefaults"
                  @message="displayMessage" />
              </v-defaults-provider>
            </v-btn-group>

            <!-- dashboard settings (its filter, ...) -->
            <v-btn
              size="large"
              variant="flat"
              color="secondary"
              :aria-label="$t('sessions.summary.dashboardSettings')"
              @click="settingsShow = true">
              <v-icon icon="mdi-cog" />
              <v-tooltip
                activator="parent"
                :open-delay="500">
                {{ $t('sessions.summary.dashboardSettings') }}
              </v-tooltip>
            </v-btn>

            <!-- the active dashboard filter, so it's clear what every widget is
                 filtered by; opens the settings to change it -->
            <v-chip
              v-if="dashboardExpression"
              size="small"
              variant="tonal"
              prepend-icon="mdi-filter-outline"
              class="dashboard-filter-chip"
              :title="`${$t('sessions.summary.dashboardFilter')}: ${dashboardExpression}`"
              @click="settingsShow = true">
              {{ dashboardExpression }}
            </v-chip>

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
        :dashboards-version="dashboardsVersion"
        @widget-config-changed="updateWidgetConfigs"
        @streaming-state="summaryStreaming = $event"
        @canceled-state="summaryCanceled = $event" />
    </div>

    <DashboardSettingsModal
      :show="settingsShow"
      :expression="dashboardExpression"
      @close="settingsShow = false"
      @apply="applyDashboardSettings" />

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
import DashboardSettingsModal from '../summary/DashboardSettingsModal.vue';
import UserService from '../users/UserService';
import { createShareableService } from '../users/ShareableService';
import { CHART_PALETTES, normalizePalette } from '../summary/widgets/chartColors';
import { toV6Shape, makeWidgetDef, defaultWidgets, widgetsFromConfig, migrateDashboard, DASHBOARD_VERSION } from '../summary/dashboardConfig';
import { systemDashboard, isSystemId } from '../summary/systemDashboards';
import { ArkimeError } from '@common/fetchWrapper.js';

const DashboardService = createShareableService('summaryConfig');

export default {
  name: 'Arkime',
  components: {
    ArkimeSearch,
    ArkimeCollapsible,
    PageLayout,
    ArkimeSummaryView,
    SummaryConfigDropdown,
    DashboardSettingsModal
  },
  data: function () {
    return {
      // Dashboard configuration
      widgets: [],
      colorScheme: 'rainbow',
      // the dashboard's own filter (data.expression), ANDed with the search
      // for every widget; edited in the settings dialog
      dashboardExpression: '',
      settingsShow: false,
      // bumped when a dashboard is saved, renamed or deleted, so the lists
      // on the page (the navigation widget) reload
      dashboardsVersion: 0,
      dashboardLoadSeq: 0, // the latest loadDashboardById call; earlier ones are dropped
      palettes: CHART_PALETTES,
      // the saved or built-in dashboard the page shows (the ?dashboard url
      // param), '' for the user's own unsaved state
      loadedDashboardId: '',
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
        version: DASHBOARD_VERSION,
        colorScheme: this.colorScheme,
        ...(this.dashboardExpression ? { expression: this.dashboardExpression } : {}),
        widgets: this.widgets,
        ...toV6Shape(this.widgets)
      };
    }
  },
  watch: {
    // Handle fetch viz data button click (re-fetch visualizations)
    '$store.state.fetchGraphData': function (value) {
      if (value) { this.reloadSummaryView(); }
    },
    // a dashboard link (the navigation widget, a markdown note, a pasted url)
    // changes the ?dashboard param while the page stays mounted
    '$route.query.dashboard': async function (id) {
      if (!id) {
        // the param was dropped (browser back): what the page shows is now the
        // user's own unsaved copy, so the same dashboard can be opened again
        this.loadedDashboardId = '';
        return;
      }
      if (id === this.loadedDashboardId) { return; }
      const loaded = await this.loadDashboardById(id);
      // the url moved on while this one loaded: that load applies, not this
      if (id !== this.$route.query.dashboard) { return; }
      if (loaded) {
        this.reloadSummaryView();
        this.saveDashboardConfig();
      } else if (loaded === false) {
        // an unknown id doesn't stay in the url (a failed request keeps it,
        // the error is shown and a reload can try again)
        this.setDashboardParam('');
      }
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
    // the dashboard filter lives in the store too, where the widget fetch
    // helpers AND it with the global search
    setDashboardExpression: function (value) {
      this.dashboardExpression = typeof value === 'string' ? value.trim() : '';
      this.$store.commit('setDashboardExpression', this.dashboardExpression);
    },
    // the settings dialog's Apply: a changed filter reloads every widget
    applyDashboardSettings: function ({ expression }) {
      if ((expression || '') === this.dashboardExpression) { return; }
      this.setDashboardExpression(expression);
      this.reloadSummaryView();
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
      this.setDashboardParam(shareable.id || '');
      this.reloadSummaryView();
      this.saveDashboardConfig();
    },
    // the shown dashboard was saved (as new or back into its saved copy):
    // bind the page to it so later edits can be saved to it by name
    onDashboardSaved: function (shareable) {
      if (shareable?.id) { this.setDashboardParam(shareable.id); }
    },
    /** The data of the saved (shareable) or system dashboard with this id, undefined when there is none. */
    dashboardData: async function (id) {
      return isSystemId(id)
        ? (await systemDashboard(id))?.data
        : (await DashboardService.get(id))?.shareable?.data;
    },
    /**
     * Loads a saved (shareable) or system dashboard by id into local state.
     * @returns {boolean|null} true when it was applied; false when the viewer
     *   answered that there is no such dashboard (404) or none this user may
     *   see (403); null when the request failed some other way (the network,
     *   a 500), so the id may still be good
     */
    loadDashboardById: async function (id) {
      if (typeof id !== 'string' || !id) { return false; }
      const seq = ++this.dashboardLoadSeq;
      try {
        const data = await this.dashboardData(id);
        // a later load (the user switched again) wins; this one is dropped
        if (seq !== this.dashboardLoadSeq) { return null; }
        if (!data) { return false; }
        this.applyConfig(data);
        this.loadedDashboardId = id;
        this.error = ''; // an earlier load's error doesn't outlive this one
        return true;
      } catch (err) {
        if (seq !== this.dashboardLoadSeq) { return null; }
        this.error = err.text || String(err);
        return (err instanceof ArkimeError && (err.status === 404 || err.status === 403)) ? false : null;
      }
    },
    /**
     * Records the shown dashboard in the url (?dashboard=<id>) so the page can
     * be linked to and reloaded; '' drops the param.
     */
    setDashboardParam: function (id) {
      this.loadedDashboardId = id;
      if ((this.$route.query.dashboard || '') === id) { return; }
      const query = { ...this.$route.query };
      if (id) { query.dashboard = id; } else { delete query.dashboard; }
      this.$router.replace({ query });
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
    applyConfig: function (rawCfg) {
      const cfg = migrateDashboard(rawCfg);
      this.widgets = widgetsFromConfig(cfg);
      this.colorScheme = normalizePalette(cfg?.colorScheme);
      this.setDashboardExpression(cfg?.expression);
    },
    loadDashboardConfig: async function () {
      // a dashboard named in the url comes first; an unknown one is dropped
      // (a failed request keeps it, so a reload can try again)
      const loaded = await this.loadDashboardById(this.$route.query.dashboard);
      if (loaded) {
        this.configLoaded = true;
        return;
      }
      if (loaded === false && this.$route.query.dashboard) { this.setDashboardParam(''); }

      // Prefer the user's default dashboard if one is set (a saved or a
      // system one)
      const defaultId = this.user?.settings?.defaultDashboardId;
      if (typeof defaultId === 'string' && defaultId) {
        try {
          const data = await this.dashboardData(defaultId);
          if (data) {
            this.applyConfig(data);
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
      this.setDashboardExpression('');
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
      this.setDashboardExpression('');
      this.setDashboardParam('');
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

.dashboard-filter-chip {
  max-width: 360px;
  cursor: pointer;
}
.dashboard-filter-chip :deep(.v-chip__content) {
  overflow: hidden;
  text-overflow: ellipsis;
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
