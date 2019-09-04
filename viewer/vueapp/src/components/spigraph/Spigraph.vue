<template>

  <div class="spigraph-page">

    <!-- search navbar -->
    <moloch-search
      :num-matching-sessions="filtered"
      :timezone="user.settings.timezone"
      @changeSearch="cancelAndLoad(true)">
    </moloch-search> <!-- /search navbar -->

    <!-- spigraph sub navbar -->
    <form class="spigraph-form"
      @submit.prevent>
      <div class="form-inline pr-1 pl-1 pt-1 pb-1">
        <!-- field select -->
        <div class="form-group"
          v-if="fields && fields.length && fieldTypeahead">
          <div class="input-group input-group-sm">
            <span class="input-group-prepend cursor-help"
              v-b-tooltip.hover
              title="SPI Graph Field">
              <span class="input-group-text">
                SPI Graph:
              </span>
            </span>
            <moloch-field-typeahead
              :fields="fields"
              query-param="field"
              :initial-value="fieldTypeahead"
              @fieldSelected="changeField"
              page="Spigraph">
            </moloch-field-typeahead>
          </div>
        </div> <!-- /field select -->

        <!-- maxElements select -->
        <div class="form-group ml-1">
          <div class="input-group input-group-sm">
            <span class="input-group-prepend cursor-help"
              v-b-tooltip.hover
              title="Max Elements Shown">
              <span class="input-group-text">
                Max Elements:
              </span>
            </span>
            <select class="form-control"
              v-model="query.size"
              @change="changeMaxElements">
              <option value="10">10</option>
              <option value="20">20</option>
              <option value="50">50</option>
              <option value="100">100</option>
              <option value="200">200</option>
              <option value="500">500</option>
            </select>
          </div>
        </div> <!-- /maxElements select -->

        <!-- sort select -->
        <div class="form-group ml-1">
          <div class="input-group input-group-sm">
            <span class="input-group-prepend cursor-help"
              v-b-tooltip.hover
              title="Sort results by">
              <span class="input-group-text">
                Sort by:
              </span>
            </span>
            <select class="form-control"
              v-model="sortBy"
              @change="changeSortBy">
              <option value="name">name</option>
              <option value="graph">graph</option>
            </select>
          </div>
        </div> <!-- /sort select -->

        <!-- refresh input -->
        <div class="form-group ml-1">
          <div class="input-group input-group-sm">
            <span class="input-group-prepend cursor-help"
              v-b-tooltip.hover
              title="Refresh page every X seconds">
              <span class="input-group-text">
                Refresh every:
              </span>
            </span>
            <select class="form-control"
              v-model="refresh"
              @change="changeRefreshInterval">
              <option value="0">0</option>
              <option value="5">5</option>
              <option value="10">10</option>
              <option value="15">15</option>
              <option value="30">30</option>
              <option value="45">45</option>
              <option value="60">60</option>
            </select>
            <span class="input-group-append">
              <span class="input-group-text">
                seconds
              </span>
            </span>
          </div>
        </div> <!-- /refresh input-->

        <div class="ml-1 records-display">
          <strong class="text-theme-accent"
            v-if="!error && recordsFiltered !== undefined">
            Showing {{ recordsFiltered | commaString }} entries filtered from
            {{ recordsTotal | commaString }} total entries
          </strong>
        </div>
      </div>
    </form>

    <div class="spigraph-content">

      <!-- main visualization -->
      <div v-if="mapData && graphData"
        class="well well-sm mb-3 ml-2 mr-2">
        <moloch-visualizations
          id="primary"
          :graph-data="graphData"
          :map-data="mapData"
          :primary="true"
          :timezone="user.settings.timezone"
          @fetchMapData="cancelAndLoad(true)">
        </moloch-visualizations>
      </div> <!-- /main visualization -->

      <!-- values -->
      <template v-if="fieldObj">
        <div v-for="(item, index) in items"
          :key="item.name"
          class="spi-graph-item pl-3 pr-3 pt-1">
          <!-- field value -->
          <div class="row">
            <div class="col-md-12">
              <div class="spi-bucket">
                <strong>
                  <moloch-session-field
                    :field="fieldObj"
                    :value="item.name"
                    :expr="fieldObj.exp"
                    :parse="true"
                    :pull-left="true"
                    :session-btn="true">
                  </moloch-session-field>
                </strong>
                <sup>({{ item.count | commaString }})</sup>
              </div>
            </div>
          </div> <!-- /field value -->
          <!-- field visualization -->
          <div class="row">
            <div class="col-md-12">
              <moloch-visualizations
                :id="index.toString()"
                :graph-data="item.graph"
                :map-data="item.map"
                :primary="false"
                :timezone="user.settings.timezone">
              </moloch-visualizations>
            </div>
          </div> <!-- /field visualization -->
        </div>
      </template> <!-- /values -->

      <!-- loading overlay -->
      <moloch-loading
        :can-cancel="true"
        v-if="loading && !error"
        @cancel="cancelAndLoad">
      </moloch-loading> <!-- /loading overlay -->

      <!-- page error -->
      <moloch-error
        v-if="error"
        :message="error"
        class="mt-5 mb-5">
      </moloch-error> <!-- /page error -->

      <!-- no results -->
      <moloch-no-results
        v-if="!error && !loading && !items.length"
        class="mt-5 mb-5"
        :records-total="recordsTotal"
        :view="query.view">
      </moloch-no-results> <!-- /no results -->

    </div>

  </div>

</template>

<script>
// import external
import Vue from 'vue';
// import services
import SpigraphService from './SpigraphService';
import FieldService from '../search/FieldService';
import ConfigService from '../utils/ConfigService';
// import external
import MolochError from '../utils/Error';
import MolochSearch from '../search/Search';
import MolochLoading from '../utils/Loading';
import MolochNoResults from '../utils/NoResults';
import MolochFieldTypeahead from '../utils/FieldTypeahead';
import MolochVisualizations from '../visualizations/Visualizations';
// import utils
import Utils from '../utils/utils';

let oldFieldObj;
let refreshInterval;
let respondedAt; // the time that the last data load succesfully responded
let pendingPromise; // save a pending promise to be able to cancel it

export default {
  name: 'Spigraph',
  components: {
    MolochError,
    MolochSearch,
    MolochLoading,
    MolochNoResults,
    MolochFieldTypeahead,
    MolochVisualizations
  },
  data: function () {
    return {
      error: '',
      fields: [],
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
      sortBy: this.$route.query.sort || 'graph'
    };
  },
  computed: {
    user: function () {
      return this.$store.state.user;
    },
    graphType: function () {
      return this.$store.state.graphType;
    },
    query: function () {
      let sort = 'name';
      if (!this.$route.query.sort || this.$route.query.sort === 'graph') {
        sort = this.$route.query.graphType || 'lpHisto';
      }
      return {
        sort: sort,
        date: this.$store.state.timeRange,
        exp: this.$route.query.field || this.user.settings.spiGraph || 'node',
        size: this.$route.query.size || 20,
        startTime: this.$store.state.time.startTime,
        stopTime: this.$store.state.time.stopTime,
        bounding: this.$route.query.bounding || 'last',
        interval: this.$route.query.interval || 'auto',
        view: this.$route.query.view || undefined,
        expression: this.$store.state.expression || undefined
      };
    },
    fieldObj: function () {
      for (let field of this.fields) {
        if (field.dbField === this.query.exp ||
          field.exp === this.query.exp) {
          oldFieldObj = field;
          return field;
        }
      }
      return oldFieldObj;
    }
  },
  watch: {
    '$route.query.size': function (newVal, oldVal) {
      this.cancelAndLoad(true);
    },
    '$route.query.sort': function (newVal, oldVal) {
      this.cancelAndLoad(true);
    },
    '$route.query.field': function (newVal, oldVal) {
      this.cancelAndLoad(true);
    },
    // watch graph type and update sort
    'graphType': function (newVal, oldVal) {
      if (newVal && this.sortBy === 'graph') {
        this.query.sort = newVal;
        if (oldVal) { this.cancelAndLoad(true); }
      }
    }
  },
  created: function () {
    setTimeout(() => {
      // wait for query to be computed
      this.cancelAndLoad(true);
      this.changeRefreshInterval();
    });

    FieldService.get(true)
      .then((result) => {
        this.fields = result;
        this.fields.push({
          dbField: 'ip.dst:port',
          exp: 'ip.dst:port',
          help: 'Destination IP:Destination Port',
          group: 'general',
          friendlyName: 'Dst IP:Dst Port'
        });
        for (let field of this.fields) {
          if (field.dbField === this.query.exp ||
            field.exp === this.query.exp) {
            this.fieldTypeahead = field.friendlyName;
          }
        }
      }).catch((error) => {
        this.loading = false;
        this.error = error.text || error;
      });
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    /**
     * Cancels the pending session query (if it's still pending) and runs a new
     * query if requested
     * @param {bool} runNewQuery  Whether to run a new spigraph query after
     *                            canceling the request
     */
    cancelAndLoad: function (runNewQuery) {
      if (pendingPromise) {
        ConfigService.cancelEsTask(pendingPromise.cancelId)
          .then((response) => {
            pendingPromise.source.cancel();
            pendingPromise = null;

            if (!runNewQuery) {
              this.loading = false;
              if (!this.items.length) {
                // show a page error if there is no data on the page
                this.error = 'You canceled the search';
              }
              return;
            }

            this.loadData();
          });
      } else if (runNewQuery) {
        this.loadData();
      }
    },
    changeMaxElements: function () {
      this.$router.push({
        query: {
          ...this.$route.query,
          size: this.query.size
        }
      });
    },
    changeSortBy: function () {
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
    changeRefreshInterval: function () {
      if (refreshInterval) { clearInterval(refreshInterval); }

      if (this.refresh && this.refresh > 0) {
        this.cancelAndLoad(true);
        refreshInterval = setInterval(() => {
          if (respondedAt && Date.now() - respondedAt >= parseInt(this.refresh * 1000)) {
            this.cancelAndLoad(true);
          }
        }, 500);
      }
    },
    /* event functions ----------------------------------------------------- */
    changeField: function (field) {
      this.fieldTypeahead = field.friendlyName;
      this.query.exp = field.dbField;
      this.$router.push({
        query: {
          ...this.$route.query,
          field: this.query.exp
        }
      });
    },
    /* helper functions ---------------------------------------------------- */
    loadData: function () {
      respondedAt = undefined;
      this.loading = true;
      this.error = false;

      // set whether map is open on the sessions page
      if (localStorage.getItem('spigraph-open-map') === 'true') {
        this.query.map = true;
      }

      // create unique cancel id to make canel req for corresponding es task
      const cancelId = Utils.createRandomString();
      this.query.cancelId = cancelId;

      const source = Vue.axios.CancelToken.source();
      const cancellablePromise = SpigraphService.get(this.query, source.token);

      // set pending promise info so it can be cancelled
      pendingPromise = { cancellablePromise, source, cancelId };

      cancellablePromise.then((response) => {
        pendingPromise = null;
        respondedAt = Date.now();
        this.error = '';
        this.loading = false;
        this.items = []; // clear items
        this.processData(response.data);
        this.recordsTotal = response.data.recordsTotal;
        this.recordsFiltered = response.data.recordsFiltered;
      }).catch((error) => {
        pendingPromise = null;
        respondedAt = undefined;
        this.loading = false;
        this.error = error.text || error;
      });
    },
    processData: function (json) {
      this.mapData = json.map;
      this.graphData = json.graph;

      let finfo = this.db2Field(this.filed);

      for (let i = 0, len = json.items.length; i < len; i++) {
        json.items[i].type = finfo.type;
      }

      this.items = json.items;
    },
    db2Field: function (dbField) {
      for (let k in this.fields) {
        if (dbField === this.fields[k].dbField ||
            dbField === this.fields[k].rawField) {
          return this.fields[k];
        }
      }

      return undefined;
    }
  },
  beforeDestroy: function () {
    if (pendingPromise) {
      pendingPromise.source.cancel();
      pendingPromise = null;
    }
  }
};
</script>

<style scoped>
/* spigraph page and navbar styles ---------- */
.spigraph-page {
  margin-top: 36px;
}
.spigraph-page form.spigraph-form {
  z-index: 4;
  position: fixed;
  top: 110px;
  left: 0;
  right: 0;
  background-color: var(--color-quaternary-lightest);

  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}
.spigraph-page form.spigraph-form .form-inline {
  flex-flow: row nowrap;
}
.spigraph-page form.spigraph-form select.form-control {
  -webkit-appearance: none;
}
.spigraph-page form.spigraph-form .form-inline .records-display  {
  line-height: 0.95;
  font-size: 12px;
  font-weight: 400;
}

/* field typeahead */
.spigraph-page form.spigraph-form .field-typeahead {
  max-height: 300px;
  overflow-y: auto;
}

/* spigraph content styles ------------------- */
.spigraph-page .spigraph-content {
  padding-top: 120px;
}

.spigraph-page .spi-graph-item .spi-bucket sup {
  margin-left: -12px;
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
</style> -->
