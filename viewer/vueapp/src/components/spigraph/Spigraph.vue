<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <page-layout class="spigraph-page">
    <template #chrome>
      <ArkimeCollapsible>
        <div class="page-toolbar">
          <!-- search navbar -->
          <arkime-search
            :num-matching-sessions="filtered"
            @change-search="onSearch" /> <!-- /search navbar -->

          <!-- spigraph sub navbar -->
          <div class="spigraph-form mx-1">
            <div class="d-flex flex-wrap align-center gap-1 page-subnav">
              <!-- main graph type switcher -->
              <div class="d-flex align-center spigraph-type-switcher">
                <v-btn-toggle
                  :model-value="spiGraphType"
                  @update:model-value="changeSpiGraphType"
                  mandatory
                  divided
                  density="comfortable"
                  variant="outlined"
                  color="primary"
                  class="spigraph-type-toggle">
                  <v-btn
                    v-for="t in graphTypeOptions"
                    :key="t"
                    :value="t"
                    size="small">
                    {{ $t(`spigraph.graphType-${t}`) }}
                  </v-btn>
                </v-btn-toggle>
              </div> <!-- /main graph type switcher -->

              <!-- field select -->
              <div
                class="arkime-input-group"
                v-if="spiGraphType !== 'connections' && fields && fields.length && fieldTypeahead">
                <span class="arkime-input-label">
                  {{ $t('spigraph.field') }}:
                </span>
                <arkime-field-typeahead
                  :fields="fields"
                  query-param="exp"
                  :initial-value="fieldTypeahead"
                  @field-selected="changeField"
                  page="Spigraph" />
              </div> <!-- /field select -->

              <!-- maxElements select -->
              <div
                class="arkime-input-group"
                v-if="spiGraphType !== 'connections'">
                <span
                  id="maxElements"
                  class="arkime-input-label cursor-help">
                  {{ $t('spigraph.maxElements') }}:
                </span>
                <v-tooltip activator="#maxElements">
                  {{ $t('spigraph.maxElementsTip') }}
                </v-tooltip>
                <select
                  class="arkime-input-control"
                  :value="query.size"
                  @change="changeMaxElements(Number($event.target.value))">
                  <option
                    v-for="opt in maxElementsOptions"
                    :key="opt"
                    :value="opt">
                    {{ opt }}
                  </option>
                </select>
              </div> <!-- /maxElements select -->

              <!-- connections controls anchor -->
              <div
                v-if="spiGraphType === 'connections'"
                id="connections-controls-anchor"
                class="connections-controls-anchor d-flex flex-wrap align-center gap-1 mt-1 mb-1" />

              <!-- intensity select (heatmap only) -->
              <div
                class="arkime-input-group"
                v-if="spiGraphType === 'heatmap'">
                <span class="arkime-input-label">
                  {{ $t('spigraph.intensity') }}:
                </span>
                <select
                  class="arkime-input-control"
                  :value="intensity"
                  @change="changeIntensity($event.target.value)">
                  <option
                    v-for="opt in intensityOptions"
                    :key="opt.value"
                    :value="opt.value">
                    {{ opt.label }}
                  </option>
                </select>
              </div> <!-- /intensity select -->

              <!-- sort select (not shown for the pie graph) -->
              <div
                class="arkime-input-group"
                v-if="spiGraphType === 'default' || spiGraphType === 'heatmap'">
                <span class="arkime-input-label">
                  {{ $t('spigraph.sortBy') }}:
                </span>
                <select
                  class="arkime-input-control"
                  :value="sortBy"
                  @change="changeSortBy($event.target.value)">
                  <option value="name">
                    {{ $t('spigraph.sortBy-name') }}
                  </option>
                  <option value="graph">
                    {{ $t('spigraph.sortBy-graph') }}
                  </option>
                </select>
              </div> <!-- /sort select -->

              <!-- refresh input (not shown for pie) -->
              <div
                class="arkime-input-group"
                v-if="spiGraphType === 'default' || spiGraphType === 'heatmap'">
                <span class="arkime-input-label">
                  {{ $t('spigraph.refreshEvery') }}:
                </span>
                <select
                  class="arkime-input-control"
                  :value="refresh"
                  @change="changeRefreshInterval(Number($event.target.value))">
                  <option
                    v-for="opt in refreshOptions"
                    :key="opt"
                    :value="opt">
                    {{ opt }}
                  </option>
                </select>
                <span class="arkime-input-label">
                  {{ $t('common.seconds') }}
                </span>
              </div> <!-- /refresh input-->

              <!-- page info -->
              <div
                class="records-display align-self-center"
                v-if="spiGraphType === 'default' || spiGraphType === 'heatmap'">
                <strong
                  class="text-theme-accent"
                  v-if="!error && recordsFiltered !== undefined">
                  {{ $t('common.showingAllTip', { count: commaString(recordsFiltered), total: commaString(recordsTotal) }) }}
                </strong>
              </div> <!-- /page info -->

              <!-- export button-->
              <v-btn
                v-if="spiGraphType !== 'default' && spiGraphType !== 'sankey' && spiGraphType !== 'connections'"
                variant="outlined"
                size="small"
                density="comfortable"
                class="ms-1"
                prepend-icon="mdi-download"
                @click.stop.prevent="exportCSV">
                {{ $t('spigraph.exportCSV') }}
                <v-tooltip activator="parent">
                  {{ $t('spigraph.exportCSVSPIGraphTip') }}
                </v-tooltip>
              </v-btn> <!-- /export button-->

              <!-- hierarchy "add another field" anchor (own full-width row) -->
              <div
                v-if="['pie', 'table', 'treemap', 'sankey'].includes(spiGraphType)"
                id="spigraph-subfield-anchor"
                class="spigraph-subfield-anchor d-flex flex-wrap align-center gap-1 mt-1" />
            </div>
          </div>
        </div>
      </ArkimeCollapsible>
    </template>

    <!-- main visualization (timeline/map): always inline, scrolls with content -->
    <div v-if="spiGraphType === 'default' && mapData && graphData && fieldObj && showToolBars">
      <arkime-visualizations
        id="primary"
        :graph-data="graphData"
        :map-data="mapData"
        :primary="true"
        :timeline-data-filters="timelineDataFilters"
        @fetch-map-data="cancelAndLoad(true)"
        @spanning-change="cancelAndLoad(true)" />
    </div> <!-- /main visualization -->

    <div
      class="spigraph-content"
      v-if="spiGraphType !== 'connections'">
      <!-- heatmap graph type -->
      <div v-if="spiGraphType === 'heatmap'">
        <arkime-heatmap
          v-if="items && items.length && fieldObj && graphData"
          :items="items"
          :graph="graphData"
          :field-obj="fieldObj"
          :metric="intensity"
          :timeline-data-filters="timelineDataFilters"
          :sort-by="sortBy" />
      </div> <!-- /heatmap graph type -->

      <!-- pie graph type -->
      <div v-else-if="spiGraphType !== 'default'">
        <arkime-pie
          v-if="items && items.length"
          :base-field="baseField"
          :graph-data="items"
          :fields="fields"
          :query="query"
          :spi-graph-type="spiGraphType"
          @toggle-load="toggleLoad"
          @toggle-error="toggleError"
          @fetched-results="fetchedResults" />
      </div> <!-- /pie graph type -->

      <!-- default graph type -->
      <div v-else>
        <!-- values -->
        <template v-if="fieldObj">
          <div
            v-for="(item, index) in items"
            :key="item.name"
            class="spi-graph-item ps-1 pe-1 pt-1">
            <!-- field value -->
            <v-row>
              <v-col
                cols="12"
                md="12">
                <div class="spi-bucket">
                  <strong>
                    <arkime-session-field
                      :field="fieldObj"
                      :value="item.name"
                      :expr="fieldObj.exp"
                      :parse="true"
                      :pull-left="true"
                      :session-btn="true" />
                  </strong>
                  <sup>({{ commaString(item[graphType]) }})</sup>
                </div>
              </v-col>
            </v-row> <!-- /field value -->
            <!-- field visualization -->
            <v-row>
              <v-col
                cols="12"
                md="12">
                <arkime-visualizations
                  :id="(index + 1).toString()"
                  :graph-data="item.graph"
                  :map-data="item.map"
                  :primary="false"
                  :timeline-data-filters="timelineDataFilters" />
              </v-col>
            </v-row> <!-- /field visualization -->
          </div>
        </template> <!-- /values -->
      </div> <!-- /default graph type -->

      <!-- loading overlay -->
      <arkime-loading
        :can-cancel="true"
        v-if="loading && !error"
        @cancel="cancelAndLoad" /> <!-- /loading overlay -->

      <!-- page error -->
      <arkime-error
        v-if="error"
        :message="error"
        class="mt-5 mb-5" /> <!-- /page error -->

      <!-- no results -->
      <arkime-no-results
        v-if="!error && !loading && !items.length"
        class="mt-5 mb-5"
        :records-total="recordsTotal"
        :view="query.view" /> <!-- /no results -->
    </div>

    <!-- connections network graph type (self-contained: owns its own
         /api/connections fetch, controls, and overlay buttons) -->
    <arkime-connections-graph
      v-else
      ref="connectionsGraph" /> <!-- /connections graph type -->

    <!-- overlay anchor: connections floating buttons teleport here -->
    <template
      v-if="spiGraphType === 'connections'"
      #overlay>
      <div id="connections-overlay-anchor" />
    </template>
  </page-layout>
</template>

<script>
// import services
import SpigraphService from './SpigraphService';
import FieldService from '../search/FieldService';
import ConfigService from '../utils/ConfigService';
// import internal
import ArkimeError from '../utils/Error.vue';
import ArkimeSearch from '../search/Search.vue';
import ArkimeLoading from '../utils/Loading.vue';
import ArkimeNoResults from '../utils/NoResults.vue';
import ArkimeFieldTypeahead from '../utils/FieldTypeahead.vue';
import ArkimeVisualizations from '../visualizations/Visualizations.vue';
import ArkimeCollapsible from '../utils/CollapsibleWrapper.vue';
import PageLayout from '../utils/PageLayout.vue';
import ArkimePie from './Hierarchy.vue';
import ArkimeHeatmap from './Heatmap.vue';
import ArkimeConnectionsGraph from '../connections/ConnectionsGraph.vue';
import { commaString } from '@common/vueFilters.js';
import { resolveMessage } from '@common/resolveI18nMessage';
// import utils
import Utils from '../utils/utils';

let refreshInterval;
let respondedAt; // the time that the last data load successfully responded
let pendingPromise; // save a pending promise to be able to cancel it

export default {
  name: 'Spigraph',
  components: {
    ArkimeError,
    ArkimeSearch,
    ArkimeLoading,
    ArkimeNoResults,
    ArkimeFieldTypeahead,
    ArkimeVisualizations,
    ArkimeCollapsible,
    PageLayout,
    ArkimePie,
    ArkimeHeatmap,
    ArkimeConnectionsGraph
  },
  data: function () {
    return {
      error: '',
      loading: true,
      filtered: 0,
      graphData: undefined,
      mapData: undefined,
      refresh: 0,
      recordsTotal: 0,
      recordsFiltered: 0,
      items: [],
      showDropdown: false,
      fieldTypeahead: 'node',
      baseField: this.$route.query.exp || this.$route.query.field || this.$store.state.user.settings.spiGraph || 'node',
      sortBy: this.$route.query.sort || 'graph',
      spiGraphType: this.$route.query.spiGraphType || 'default',
      tableResults: [],
      fieldTypeaheadList: [],
      baseFieldObj: {},
      // Subnav option lists -- extracted from inline literals so the
      // template stays readable and v-for collapses the repeated
      // <option> blocks into one.
      maxElementsOptions: [5, 10, 15, 20, 30, 50, 100, 200, 500],
      refreshOptions: [0, 5, 10, 15, 30, 45, 60],
      graphTypeOptions: ['default', 'heatmap', 'pie', 'table', 'treemap', 'sankey', 'connections']
    };
  },
  computed: {
    user: function () {
      return this.$store.state.user;
    },
    timelineDataFilters: function () {
      const filters = this.user.settings.timelineDataFilters;
      return filters.map(i => FieldService.getField(i));
    },
    graphType: function () {
      return this.$store.state.graphType;
    },
    // heatmap intensity choices: sessions + each timeline metric
    intensityOptions: function () {
      const opts = [{ value: 'sessionsHisto', label: this.$t('spigraph.sessions') }];
      for (const f of this.timelineDataFilters) {
        opts.push({ value: f.dbField + 'Histo', label: f.friendlyName });
      }
      return opts;
    },
    intensity: function () {
      // graphType may name a metric whose timeline filter was since removed
      if (this.graphType && this.intensityOptions.some(o => o.value === this.graphType)) {
        return this.graphType;
      }
      return 'sessionsHisto';
    },
    query: function () {
      let sort = 'name';
      if (!this.$route.query.sort || this.$route.query.sort === 'graph') {
        sort = this.$route.query.graphType || this.$store.state.graphType || 'sessionsHisto';
      }
      return {
        sort,
        date: this.$store.state.timeRange,
        exp: this.$route.query.exp || this.$route.query.field || this.user.settings.spiGraph || 'node',
        size: this.$route.query.size || 20,
        startTime: this.$store.state.time.startTime,
        stopTime: this.$store.state.time.stopTime,
        bounding: this.$route.query.bounding || 'last',
        interval: this.$route.query.interval || 'auto',
        spanning: this.$route.query.spanning === 'true' ? true : undefined,
        view: this.$route.query.view || undefined,
        expression: this.$store.state.expression || undefined,
        cluster: this.$route.query.cluster || undefined
      };
    },
    fieldObj: function () {
      return FieldService.getField(this.query.exp);
    },
    showToolBars: function () {
      return this.$store.state.showToolBars;
    },
    fields: function () {
      return FieldService.addIpDstPortField(this.$store.state.fieldsArr);
    }
  },
  watch: {
    '$route.query' (newVal, oldVal) {
      if (newVal.size !== this.query.size) {
        this.query.size = newVal.size || 20;
      }
      if (newVal.sort !== this.query.sort) {
        this.query.sort = newVal.sort || 'graph';
      }
      if (newVal.spiGraphType !== this.spiGraphType) {
        this.spiGraphType = newVal.spiGraphType || 'default';
        if (this.spiGraphType === 'connections') {
          // connections owns its data; stop spigraph refresh
          this.refresh = 0;
          this.changeRefreshInterval(0);
        }
      }
    },
    // watch graph type and update sort
    graphType: function (newVal, oldVal) {
      if (newVal && this.sortBy === 'graph') {
        this.query.sort = newVal;
      }
    }
  },
  mounted: function () {
    const field = FieldService.getField(this.query.exp, true);
    if (field) {
      this.fieldTypeahead = field.friendlyName;
      this.baseField = field.exp;
      this.query.exp = field.exp;
    }

    if (this.spiGraphType === 'connections') {
      // the connections child runs its own /api/connections load on mount
      this.loading = false;
    } else if (field) {
      this.cancelAndLoad(true);
      this.changeRefreshInterval(0);
    }
  },
  methods: {
    commaString,
    /* Search bar change: connections reloads via its child, others here. */
    onSearch: function () {
      if (this.spiGraphType === 'connections') {
        this.$refs.connectionsGraph?.cancelAndLoad(true);
        return;
      }
      this.cancelAndLoad(true);
    },
    /* exposed page functions ---------------------------------------------- */
    /**
     * Cancels the pending session query (if it's still pending) and runs a new
     * query if requested
     * @param {bool} runNewQuery  Whether to run a new spigraph query after
     *                            canceling the request
     */
    cancelAndLoad: function (runNewQuery) {
      const clientCancel = () => {
        if (pendingPromise) {
          pendingPromise.controller.abort('You canceled the search');
          pendingPromise = null;
        }

        if (!runNewQuery) {
          this.loading = false;
          if (!this.items.length) {
            // show a page error if there is no data on the page
            this.error = 'You canceled the search';
          }
          return;
        }

        this.loadData();
      };

      if (pendingPromise) {
        ConfigService.cancelEsTask(pendingPromise.cancelId).then((response) => {
          clientCancel();
        }).catch((error) => {
          clientCancel();
        });
      } else if (runNewQuery) {
        this.loadData();
      }
    },
    changeMaxElements: function (size) {
      this.query.size = size;
      this.$router.push({
        query: {
          ...this.$route.query,
          size
        }
      });
    },
    changeIntensity: function (value) {
      // recolors + re-sorts client-side (all metrics are already fetched); the
      // backend top-N ranking only switches to this metric on the next fetch
      this.$store.commit('updateGraphType', value);
    },
    changeSortBy: function (sortBy) {
      this.sortBy = sortBy;
      if (this.sortBy === 'graph') {
        this.query.sort = this.graphType;
      } else {
        this.query.sort = this.sortBy;
      }
      this.$router.push({
        query: {
          ...this.$route.query,
          sort: this.sortBy
        }
      });
    },
    changeRefreshInterval: function (interval) {
      this.refresh = interval;
      if (refreshInterval) { clearInterval(refreshInterval); }

      if (this.refresh && this.refresh > 0) {
        this.$store.commit('setIssueSearch', true);
        refreshInterval = setInterval(() => {
          if (respondedAt && Date.now() - respondedAt >= parseInt(this.refresh * 1000)) {
            this.$store.commit('setIssueSearch', true);
          }
        }, 500);
      }
    },
    changeSpiGraphType: function (spiGraphType) {
      const leavingConnections = this.spiGraphType === 'connections';
      this.spiGraphType = spiGraphType;
      if (this.spiGraphType === 'pie' ||
        this.spiGraphType === 'table' || this.spiGraphType === 'treemap' || this.spiGraphType === 'sankey') {
        if (!this.$route.query.size) {
          this.query.size = 5; // set default size to 5
        }
        this.sortBy = 'graph'; // set default sort to count (graph)
        this.query.sort = this.graphType;
        this.refresh = 0;
        this.changeRefreshInterval(0);
      } else if (this.spiGraphType === 'heatmap') {
        if (!this.graphType) { this.$store.commit('updateGraphType', 'sessionsHisto'); }
      } else if (this.spiGraphType === 'connections') {
        // connections owns its data; stop spigraph refresh
        this.refresh = 0;
        this.changeRefreshInterval(0);
      }
      this.$router.push({
        query: {
          ...this.$route.query,
          size: this.query.size,
          sort: this.sortBy,
          spiGraphType: this.spiGraphType
        }
      });
      if (leavingConnections && this.spiGraphType !== 'connections') {
        // load spigraph data skipped while connections was active
        this.cancelAndLoad(true);
      }
    },
    fetchedResults: function (tableResults, fieldTypeaheadList, baseFieldObj) {
      this.fieldTypeaheadList = fieldTypeaheadList;
      this.tableResults = tableResults;
      this.baseFieldObj = baseFieldObj;
    },
    exportCSV () {
      let fieldList = [this.baseFieldObj.friendlyName];
      fieldList = fieldList.concat(this.fieldTypeaheadList.map(f => f.friendlyName));

      let csv = '';
      if (this.tableResults.length) {
        const keys = ['name', 'size'];

        // add the headers
        fieldList.forEach((field) => {
          keys.forEach((key) => {
            csv += `${field} ${key === 'name' ? 'value' : 'count'},`;
          });
        });

        csv = csv.slice(0, -1); // remove trailing comma
        csv += '\n'; // newline for next row

        // add the values
        this.tableResults.forEach((row) => {
          if (row.parents) {
            csv += row.parents.map(p => {
              return keys.map(key => p[key]).join(',');
            }).join(',');
            if (row.parents.length) {
              csv += ',';
            }
          }
          csv += keys.map(key => row[key]).join(',') + '\n';
        });
      }

      const blob = new Blob([csv], { type: 'text/csv' });
      const url = window.URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'spigraph.csv';
      a.click();
      window.URL.revokeObjectURL(url);
    },
    /* event functions ----------------------------------------------------- */
    changeField: function (field) {
      this.fieldTypeahead = field.friendlyName;
      this.query.exp = field.exp;
      this.baseField = field.exp;
      this.$router.push({
        query: {
          ...this.$route.query,
          exp: this.query.exp
        }
      });
      this.cancelAndLoad(true);
    },
    toggleLoad: function (loading) {
      this.loading = loading;
    },
    toggleError: function (message) {
      this.error = message;
    },
    /* helper functions ---------------------------------------------------- */
    async loadData () {
      respondedAt = undefined;

      // connections loads via its own child
      if (this.spiGraphType === 'connections') {
        this.loading = false;
        return;
      }

      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        this.items = [];
        pendingPromise = null;
        this.recordsTotal = 0;
        this.recordsFiltered = 0;
        this.mapData = undefined;
        this.graphData = undefined;
        return;
      }

      this.loading = true;
      this.error = false;

      // set whether map is open on the sessions page
      if (localStorage.getItem('spigraph-open-map') === 'true') {
        this.query.map = true;
      }

      // create unique cancel id to make cancel req for corresponding es task
      const cancelId = Utils.createRandomString();
      this.query.cancelId = cancelId;

      try {
        const { controller, fetcher } = await SpigraphService.get(this.query);
        pendingPromise = { controller, cancelId };

        const response = await fetcher;
        if (response.error) {
          throw new Error(response.error);
        }

        pendingPromise = null;
        respondedAt = Date.now();
        this.error = '';
        this.loading = false;
        this.items = response.items;
        this.mapData = response.map;
        this.graphData = response.graph;
        this.recordsTotal = response.recordsTotal;
        this.recordsFiltered = response.recordsFiltered;
      } catch (error) {
        pendingPromise = null;
        respondedAt = undefined;
        this.loading = false;
        this.error = resolveMessage(error, this.$t);
      }
    }
  },
  beforeUnmount () {
    if (pendingPromise) {
      pendingPromise.controller.abort('Closing the SPIGraph page canceled the search');
      pendingPromise = null;
    }
  }
};
</script>

<style scoped>
.spigraph-page .spigraph-form {
  z-index: 4;
  background-color: rgb(var(--v-theme-quaternary-lightest));
}
.spigraph-page .spigraph-form .records-display  {
  font-size: 0.85rem;
  font-weight: 400;
}

/* primary view switcher: a prominent segmented toggle (not a buried
   dropdown) sized to the 32px subnav band */
.spigraph-page .spigraph-type-toggle {
  height: 32px;
}
.spigraph-page .spigraph-type-toggle :deep(.v-btn) {
  height: 32px;
  text-transform: none;
  letter-spacing: normal;
}
/* symmetric band padding: even spacing for single-row vs multi-row subnavs */
.spigraph-page .page-subnav {
  padding-block: 6px;
}
/* "add another field" on its own row: full-width forces a flex-wrap break */
.spigraph-page .spigraph-subfield-anchor {
  flex: 1 0 100%;
  padding-bottom: 6px;
}

/* field typeahead */
.spigraph-page .spigraph-form .field-typeahead {
  max-height: 300px;
  overflow-y: auto;
}

/* spigraph content styles ------------------- */
.spigraph-page .spigraph-content {
  padding-top: 10px;
}

.spigraph-page .spi-graph-item .spi-bucket sup {
  margin-left: -8px;
}

/* main graph/map */
.spigraph-page .spigraph-content .well {
  -webkit-box-shadow: 0 4px 16px -2px black;
     -moz-box-shadow: 0 4px 16px -2px black;
          box-shadow: 0 4px 16px -2px black;
}

/* stripes */
.spigraph-page .spigraph-content .spi-graph-item:nth-child(odd) {
  background-color: rgb(var(--v-theme-quaternary-lightest));
}
</style>
