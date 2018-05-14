<template>

  <div class="spigraph-page">

    <!-- search navbar -->
    <moloch-search
      :num-matching-sessions="filtered"
      :timezone="settings.timezone"
      @changeSearch="loadData">
    </moloch-search> <!-- /search navbar -->

    <!-- info navbar -->
    <form class="info-nav">
      <div v-if="dataLoading">
        <!-- TODO config save btn -->
        <div v-if="dataLoading"
          class="info-nav-loading">
          <span class="fa fa-spinner fa-lg fa-spin">
          </span>&nbsp;
          <em>
            Loading SPI data
          </em>
          <button type="button"
            class="btn btn-warning btn-sm pull-right"
            @click="cancelLoading()">
            <span class="fa fa-ban">
            </span>&nbsp;
            cancel
          </button>
        </div>
      </div>
    </form> <!-- /info navbar -->

    <!-- warning navbar -->
    <form v-if="staleData && !dataLoading"
      class="loading-nav">
      <div class="form-inline text-theme-accent">
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        <strong>Warning:</strong>
        much of the data below does not match your query
        because the request was canceled.
        <em>
          Click search to reissue your query.
        </em>
        <span class="fa fa-close pull-right cursor-pointer"
          @click="staleData = false">
        </span>
      </div>
    </form> <!-- /warning navbar -->

    <!-- spigraph sub navbar -->
    <form class="spigraph-form">
      <div class="form-inline mr-1 ml-1 mt-1 mb-1">
        <!-- field select -->
        <div class="form-group margined-bottom">
          <div class="input-group input-group-sm" ng-if="$ctrl.fields">
            <span class="input-group-addon cursor-help"
              tooltip-placement="bottom-left"
              title="SPI Graph Field">
              SPI Graph:
            </span>
            <input type="text" v-model="query.field"
              class="form-control field-typeahead"
              typeahead-min-length="0"
              typeahead-on-select="$ctrl.changeField()"
              typeahead-input-formatter="$ctrl.formatField($model)"
              uib-typeahead="field.exp for field in $ctrl.fields | filter:{exp:$viewValue}">
          </div>
        </div> <!-- /field select -->

        <!-- maxElements select -->
        <div class="form-group margined-bottom">
          <div class="input-group input-group-sm">
            <span class="input-group-addon cursor-help"
              tooltip-placement="bottom"
              title="Max Elements Shown">
              Max Elements:
            </span>
            <select class="form-control" convert-to-number
              v-model="query.size" @change="changeMaxElements()">
              <option value="10">10</option>
              <option value="20">20</option>
              <option value="50">50</option>
              <option value="100">100</option>
              <option value="200">200</option>
              <option value="500">500</option>
            </select>
          </div>
        </div> <!-- /min connections select -->

        <!-- sort select -->
        <div class="form-group margined-bottom">
          <div class="input-group input-group-sm">
            <span class="input-group-addon cursor-help"
              tooltip-placement="bottom"
              title="Sort results by">
              Sort by:
            </span>
            <select class="form-control"
              v-model="query.sortBy" @change="changeSortBy()">
              <option value="name">name</option>
              <option value="graph">graph</option>
            </select>
          </div>
        </div> <!-- /sort select -->

        <!-- refresh input -->
        <div class="form-group margined-bottom">
          <div class="input-group input-group-sm">
            <span class="input-group-addon cursor-help"
              tooltip-placement="bottom"
              title="Refresh page every X seconds">
              Refresh every:
            </span>
            <select class="form-control" convert-to-number
              v-model="refresh" @change="changeRefreshInterval()">
              <option value="0">0</option>
              <option value="5">5</option>
              <option value="10">10</option>
              <option value="15">15</option>
              <option value="30">30</option>
              <option value="45">45</option>
              <option value="60">60</option>
            </select>
            <span class="input-group-addon">
              seconds
            </span>
          </div>
        </div> <!-- /refresh input-->

        <strong class="small margined-top margined-left-lg text-theme-accent"
          v-if="!error && recordsFiltered !== undefined">
          Showing {{recordsFiltered}} entries filtered from
          {{recordsTotal}} total entries
        </strong>
      </div>
    </form>

    <div class="spigraph-content ml-2 mr-2">

    <!-- session visualizations -->
      <moloch-visualizations
        v-if="mapData && graphData"
        :graph-data="graphData"
        :map-data="mapData"
        :primary="true"
        :timezone="settings.timezone">
      </moloch-visualizations>

      <div v-for="item in items" :key="item.name">
        class="margined-top-lg margined-bottom spi-graph-item padded-top">
        <div class="row">
          <div class="col-md-12">
            <div class="spi-bucket margined-left-xxlg">
              <moloch-session-field
                :value="item.name"
                :expr="exp"
                parse="true"
                pull-left="true"
                session-btn="true">
              </moloch-session-field>
              <sup>({{item.count}})</sup>
            </div>
          </div>
        </div>
        <div class="row padded-bottom-lg">
          <div class="col-md-12">
            <moloch-visualizations
              v-if="mapData && graphData"
              :graph-data="item.graph"
              :map-data="item.map"
              :primary="false"
              :timezone="settings.timezone">
            </moloch-visualizations>
          </div>
        </div>
      </div>

      <!-- page error -->
      <moloch-error
        v-if="error"
        :message="error"
        class="mt-5 mb-5">
      </moloch-error> <!-- /page error -->
    </div>
  </div>

</template>

<script>
// import Vue from 'vue';

import FieldService from '../search/FieldService';
import UserService from '../UserService';

import MolochError from '../utils/Error';
import MolochSearch from '../search/Search';
import MolochVisualizations from '../visualizations/Visualizations';

export default {
  name: 'Spigraph',
  components: {
    MolochError,
    MolochSearch,
    MolochVisualizations
  },
  data: function () {
    return {
      error: '',
      canceled: false,
      loading: true,
      dataLoading: true,
      staleData: undefined, // TODO,
      filtered: 0,
      settings: {
        timezone: 'local'
      },
      fieldConfigs: [],
      graphData: undefined,
      mapData: undefined,
      categoryList: [],
      categoryObjects: {},
      refresh: 0,
      recordsTotal: 0,
      recordsFiltered: 0,
      items: [],
      exp: 'ip.src'
    };
  },
  computed: {
    query: function () {
      return {
        date: this.$store.state.timeRange,
        field: this.$route.query.field || 'srcIp',
        sortBy: this.$route.query.sortBy || 'graph',
        size: this.$route.query.size || 20,
        startTime: this.$store.state.time.startTime,
        stopTime: this.$store.state.time.stopTime,
        bounding: this.$route.query.bounding || 'last',
        interval: this.$route.query.interval || 'auto',
        view: this.$route.query.view || undefined,
        expression: this.$route.query.expression || undefined
      };
    }
  },
  created: function () {
    this.getUserSettings();
    FieldService.get()
      .then((result) => {
        this.fields = result;
      }).catch((error) => {
        this.loading = false;
        this.error = error;
      });
  },
  mounted: function () {
    this.loadData();
  },
  methods: {
    /* event functions ----------------------------------------------------- */
    getUserSettings: function () {
      UserService.getCurrent()
        .then((response) => {
          this.settings = response;
        }, (error) => {
          this.settings = { timezone: 'local' };
          this.error = error;
        });
    },
    loadData: function () {
      this.loading = true;
      this.error = false;

      this.$http.get('spigraph.json', { params: this.query })
        .then((response) => {
          this.error = '';
          this.loading = false;
          this.processData(response.data);
          this.recordsTotal = response.data.recordsTotal;
          this.recordsFiltered = response.data.recordsFiltered;
        }, (error) => {
          this.loading = false;
          this.error = error;
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
    },
    changeField: function () {
      this.$router.push({ query: { ...this.$route.query, field: this.query.field } });
      this.loadData();
    },
    changeMaxElements: function () {
      this.$router.push({ query: { ...this.$route.query, size: this.query.size } });
      this.loadData();
    },
    changeSortBy: function () {
      // ALW - Fix
      this.loadData();
    },
    changeRefreshInterval: function () {
      // ECR - Fix
    }
  }
};
</script>
<style scoped>
/* spigraph page, navbar, and content styles - */
.spigraph-page {
  margin-top: 36px;
}
.spigraph-page form.spigraph-form {
  position: relative;
  top     : 79px;
  left    : 0;
  right   : 0;
  height  : 42px;
  background-color: var(--color-quaternary-lightest);

  -webkit-box-shadow: var(--px-none) var(--px-none) var(--px-xlg) var(--px-sm) #333;
     -moz-box-shadow: var(--px-none) var(--px-none) var(--px-xlg) var(--px-sm) #333;
          box-shadow: var(--px-none) var(--px-none) var(--px-xlg) var(--px-sm) #333;
}
.spigraph-page form.spigraph-form .form-inline {
  margin-top: -3px;
}

.spigraph-page form.spigraph-form .input-group-addon.legend {
  font-weight : 700;
  color       : white;
}

.spigraph-page .dl-horizontal {
  margin-bottom: var(--px-md);
}
.spigraph-page .dl-horizontal dt {
  width     : 80px;
  text-align: left;
}
.spigraph-page .dl-horizontal dd {
  margin-left: 85px;
}
.spigraph-content {
  padding-top: 115px;
  overflow-x: hidden;
}
</style>
