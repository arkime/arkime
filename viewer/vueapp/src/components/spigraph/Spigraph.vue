<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>

  <div class="spigraph-page">
    <ArkimeCollapsible>
      <span class="fixed-header">
        <!-- search navbar -->
        <arkime-search
          :num-matching-sessions="filtered"
          @changeSearch="cancelAndLoad(true)">
        </arkime-search> <!-- /search navbar -->

        <!-- spigraph sub navbar -->
        <div class="spigraph-form m-1">
          <BRow gutter-x="1" align-h="start">
            <!-- field select -->
            <BCol cols="auto" v-if="fields && fields.length && fieldTypeahead">
              <BInputGroup size="sm">
                <BInputGroupText class="cursor-help">
                  SPI Graph:
                </BInputGroupText>
                <arkime-field-typeahead
                  :fields="fields"
                  query-param="exp"
                  :initial-value="fieldTypeahead"
                  @fieldSelected="changeField"
                  page="Spigraph">
                </arkime-field-typeahead>
              </BInputGroup>
            </BCol> <!-- /field select -->

            <!-- maxElements select -->
            <BCol cols="auto">
              <BInputGroup size="sm">
                <BInputGroupText class="cursor-help" id="maxElementsTooltip">
                    Max Elements:
                  <BTooltip target="maxElementsTooltip">Maximum number of elements returned (for the first field selected)</BTooltip>
                </BInputGroupText>
                <BFormSelect
                  :model-value="query.size"
                  @update:model-value="val => changeMaxElements(val)"
                  :options="[5,10,15,20,30,50,100,200,500]">
                </BFormSelect>
              </BInputGroup>
            </BCol> <!-- /maxElements select -->

            <!-- main graph type select -->
            <BCol cols="auto">
              <BInputGroup size="sm">
                <BInputGroupText class="cursor-help">
                  Graph Type:
                </BInputGroupText>
                <BFormSelect
                  :model-value="spiGraphType"
                  @update:model-value="(val) => changeSpiGraphType(val)">
                  <option value="default">timeline/map</option>
                  <option value="pie">donut</option>
                  <option value="table">table</option>
                  <option value="treemap">treemap</option>
                </BFormSelect>
              </BInputGroup>
            </BCol> <!-- /main graph type select -->

            <!-- sort select (not shown for the pie graph) -->
            <BCol cols="auto" v-if="spiGraphType === 'default'">
              <BInputGroup size="sm">
                <BInputGroupText class="cursor-help">
                  Sort by:
                </BInputGroupText>
                <BFormSelect
                  :model-value="sortBy"
                  @update:model-value="(val) => changeSortBy(val)"
                  :options="[
                    { value: 'name', text: 'Alphabetically' },
                    { value: 'graph', text: 'Count' }
                  ]">
                </BFormSelect>
              </BInputGroup>
            </BCol> <!-- /sort select -->

            <!-- refresh input (not shown for pie) -->
            <BCol cols="auto" v-if="spiGraphType === 'default'">
              <BInputGroup size="sm">
                <BInputGroupText class="cursor-help">
                  Refresh every:
                </BInputGroupText>
                <BFormSelect
                  :model-value="refresh"
                  @update:model-value="(val) => changeRefreshInterval(val)"
                  :options="[0,5,10,15,30,45,60]">
                </BFormSelect>
                <BInputGroupText >
                  seconds
                </BInputGroupText>
              </BInputGroup>
            </BCol> <!-- /refresh input-->

            <!-- page info -->
            <BCol cols="auto"
              align-self="center"
              class="records-display"
              v-if="spiGraphType === 'default'">
              <strong class="text-theme-accent"
                v-if="!error && recordsFiltered !== undefined" >
                Showing {{ commaString(recordsFiltered) }} entries filtered from
                {{ commaString(recordsTotal) }} total entries
              </strong>
            </BCol> <!-- /page info -->

            <!-- export button-->
            <BCol cols="auto">
              <button
                v-if="spiGraphType !== 'default'"
                class="btn btn-default btn-sm ms-1"
                id="exportCSVSPIGraph"
                @click.stop.prevent="exportCSV">
                <span class="fa fa-download"></span>
                <BTooltip target="exportCSVSPIGraph">Export this data as a CSV file</BTooltip>
              </button> <!-- /export button-->
            </BCol>
          </BRow>
        </div>
      </span>
    </ArkimeCollapsible>

    <!-- main visualization -->
    <div v-if="spiGraphType === 'default' && mapData && graphData && fieldObj && showToolBars">
      <arkime-visualizations
        id="primary"
        :graph-data="graphData"
        :map-data="mapData"
        :primary="true"
        :timelineDataFilters="timelineDataFilters"
        @fetchMapData="cancelAndLoad(true)">
      </arkime-visualizations>
    </div> <!-- /main visualization -->

    <div class="spigraph-content">

      <!-- pie graph type -->
      <div v-if="spiGraphType === 'pie' || spiGraphType === 'table' || spiGraphType === 'treemap'">

        <arkime-pie v-if="items && items.length"
          :base-field="baseField"
          :graph-data="items"
          :fields="fields"
          :query="query"
          :spiGraphType="spiGraphType"
          @toggleLoad="toggleLoad"
          @toggleError="toggleError"
          @fetchedResults="fetchedResults">
        </arkime-pie>

      </div> <!-- /pie graph type -->

      <!-- default graph type -->
      <div v-else>

        <!-- values -->
        <template v-if="fieldObj">
          <div v-for="(item, index) in items"
            :key="item.name"
            class="spi-graph-item ps-1 pe-1 pt-1">
            <!-- field value -->
            <div class="row">
              <div class="col-md-12">
                <div class="spi-bucket">
                  <strong>
                    <arkime-session-field
                      :field="fieldObj"
                      :value="item.name"
                      :expr="fieldObj.exp"
                      :parse="true"
                      :pull-left="true"
                      :session-btn="true">
                    </arkime-session-field>
                  </strong>
                  <sup>({{ commaString(item[graphType]) }})</sup>
                </div>
              </div>
            </div> <!-- /field value -->
            <!-- field visualization -->
            <div class="row">
              <div class="col-md-12">
                <arkime-visualizations
                  :id="(index + 1).toString()"
                  :graph-data="item.graph"
                  :map-data="item.map"
                  :primary="false"
                  :timelineDataFilters="timelineDataFilters">
                </arkime-visualizations>
              </div>
            </div> <!-- /field visualization -->
          </div>
        </template> <!-- /values -->

      </div> <!-- /default graph type -->

      <!-- loading overlay -->
      <arkime-loading
        :can-cancel="true"
        v-if="loading && !error"
        @cancel="cancelAndLoad">
      </arkime-loading> <!-- /loading overlay -->

      <!-- page error -->
      <arkime-error
        v-if="error"
        :message="error"
        class="mt-5 mb-5">
      </arkime-error> <!-- /page error -->

      <!-- no results -->
      <arkime-no-results
        v-if="!error && !loading && !items.length"
        class="mt-5 mb-5"
        :records-total="recordsTotal"
        :view="query.view">
      </arkime-no-results> <!-- /no results -->

    </div>

  </div>

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
import ArkimePie from './Hierarchy.vue';
import { commaString } from '@real_common/vueFilters.js';
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
    ArkimePie
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
      baseFieldObj: {}
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
    '$route.query.size': function (newVal, oldVal) {
      this.cancelAndLoad(true);
    },
    '$route.query.sort': function (newVal, oldVal) {
      this.cancelAndLoad(true);
    },
    '$route.query.exp': function (newVal, oldVal) {
      this.cancelAndLoad(true);
    },
    '$route.query.spiGraphType': function (newVal, oldVal) {
      this.spiGraphType = newVal;
    },
    // watch graph type and update sort
    graphType: function (newVal, oldVal) {
      if (newVal && this.sortBy === 'graph') {
        this.query.sort = newVal;
        if (oldVal) { this.cancelAndLoad(true); }
      }
    }
  },
  mounted: function () {
    const field = FieldService.getField(this.query.exp, true);
    if (field) {
      this.fieldTypeahead = field.friendlyName;
      this.baseField = field.exp;
      this.query.exp = field.exp;

      this.cancelAndLoad(true);
      this.changeRefreshInterval();
    }
  },
  methods: {
    commaString,
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
          pendingPromise.controller.abort();
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
      this.spiGraphType = spiGraphType;
      if (this.spiGraphType === 'pie' ||
        this.spiGraphType === 'table' || this.spiGraphType === 'treemap') {
        if (!this.$route.query.size) {
          this.query.size = 5; // set default size to 5
        }
        this.sortBy = 'graph'; // set default sort to count (graph)
        this.query.sort = this.graphType;
        this.refresh = 0;
        this.changeRefreshInterval();
      }
      this.$router.push({
        query: {
          ...this.$route.query,
          size: this.query.size,
          sort: this.sortBy,
          spiGraphType: this.spiGraphType
        }
      });
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
          csv += row.parents.map(p => {
            return keys.map(key => p[key]).join(',');
          }).join(',');
          if (row.parents.length) {
            csv += ',';
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
        this.error = error.text || error;
      }
    }
  },
  beforeUnmount () {
    if (pendingPromise) {
      pendingPromise.controller.abort();
      pendingPromise = null;
    }
  }
};
</script>

<style scoped>
.spigraph-page .spigraph-form {
  z-index: 4;
  background-color: var(--color-quaternary-lightest);
}
.spigraph-page .spigraph-form .form-inline {
  flex-flow: row nowrap;
}
.spigraph-page .spigraph-form .records-display  {
  font-size: 0.85rem;
  font-weight: 400;
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
  background-color: var(--color-quaternary-lightest);
}
</style>
