<template>

  <div class="container-fluid mt-2">

    <moloch-loading v-if="initialLoading && !error">
    </moloch-loading>

    <moloch-error v-if="error"
      :message="error">
    </moloch-error>

    <div v-show="!error">

      <moloch-table
        id="esNodesTable"
        :data="stats"
        :loadData="loadData"
        :columns="columns"
        :no-results="true"
        :show-avg-tot="true"
        :action-column="true"
        :desc="query.desc"
        :sortField="query.sortField"
        table-animation="list"
        table-classes="table-sm text-right small"
        table-state-name="esNodesCols"
        table-widths-state-name="esNodesColWidths">
        <template slot="actions"
          slot-scope="{ item }">
          <b-dropdown size="sm"
            class="row-actions-btn"
            v-has-permission="'createEnabled'">
            <b-dropdown-item v-if="!item.nodeExcluded"
              @click="exclude('name', item)">
              Exclude node {{ item.name }}
            </b-dropdown-item>
            <b-dropdown-item v-else
              @click="include('name', item)">
              Include node {{ item.name }}
            </b-dropdown-item>
            <b-dropdown-item v-if="!item.ipExcluded"
              @click="exclude('ip', item)">
              Exclude IP {{ item.ip }}
            </b-dropdown-item>
            <b-dropdown-item v-else
              @click="include('ip', item)">
              Include IP {{ item.ip }}
            </b-dropdown-item>
          </b-dropdown>
        </template>
      </moloch-table>

    </div>

  </div>

</template>

<script>
import Vue from 'vue';

import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';
import MolochTable from '../utils/Table';

let reqPromise; // promise returned from setInterval for recurring requests
let respondedAt; // the time that the last data load succesfully responded

function roundCommaString (val) {
  let result = Vue.options.filters.commaString(Vue.options.filters.round(val, 0));
  return result;
};

export default {
  name: 'EsStats',
  props: [ 'dataInterval', 'refreshData', 'searchTerm' ],
  components: { MolochError, MolochLoading, MolochTable },
  data: function () {
    return {
      error: '',
      initialLoading: true,
      stats: null,
      query: {
        filter: this.searchTerm || undefined,
        sortField: 'nodeName',
        desc: false
      },
      columns: [ // es stats table columns
        // default columns
        { id: 'nodeName', name: 'Name', sort: 'nodeName', dataField: 'name', doStats: false, default: true, width: 120 },
        { id: 'docs', name: 'Documents', sort: 'docs', dataField: 'docs', doStats: true, default: true, width: 120, dataFunction: roundCommaString },
        { id: 'storeSize', name: 'Disk Used', sort: 'storeSize', dataField: 'storeSize', doStats: true, default: true, width: 100, dataFunction: (val) => { return this.$options.filters.humanReadableBytes(val); } },
        { id: 'freeSize', name: 'Disk Free', sort: 'freeSize', dataField: 'freeSize', doStats: true, default: true, width: 100, dataFunction: (val) => { return this.$options.filters.humanReadableBytes(val); } },
        { id: 'heapSize', name: 'Heap Size', sort: 'heapSize', dataField: 'heapSize', doStats: true, default: true, width: 100, dataFunction: (val) => { return this.$options.filters.humanReadableBytes(val); } },
        { id: 'load', name: 'OS Load', sort: 'load', dataField: 'load', doStats: true, default: true, width: 100, dataFunction: (val) => { return this.$options.filters.commaString(this.$options.filters.round(val, 2)); } },
        { id: 'cpu', name: 'CPU', sort: 'cpu', dataField: 'cpu', doStats: true, default: true, width: 80, dataFunction: (val) => { return this.$options.filters.commaString(this.$options.filters.round(val, 1)) + '%'; } },
        { id: 'read', name: 'Read/s', sort: 'read', dataField: 'read', doStats: true, default: true, width: 90, dataFunction: (val) => { return this.$options.filters.humanReadableBytes(val); } },
        { id: 'write', name: 'Write/s', sort: 'write', dataField: 'write', doStats: true, default: true, width: 90, dataFunction: (val) => { return this.$options.filters.humanReadableBytes(val); } },
        { id: 'searches', name: 'Searches/s', sort: 'searches', dataField: 'searches', doStats: true, width: 100, default: true, dataFunction: roundCommaString },
        // all the rest of the available stats
        { id: 'ip', name: 'IP', sort: 'ip', dataField: 'ip', doStats: false, width: 100 },
        { id: 'ipExcluded', name: 'IP Excluded', sort: 'ipExcluded', dataField: 'ipExcluded', doStats: false, width: 100 },
        { id: 'nodeExcluded', name: 'Node Excluded', sort: 'nodeExcluded', dataField: 'nodeExcluded', doStats: false, width: 125 },
        { id: 'nonHeapSize', name: 'Non Heap Size', sort: 'nonHeapSize', dataField: 'nonHeapSize', doStats: false, width: 100, dataFunction: (val) => { return this.$options.filters.humanReadableBytes(val); } },
        { id: 'searchesTime', name: 'Search Time', sort: 'searchesTime', dataField: 'searchesTime', doStats: true, width: 100, dataFunction: roundCommaString },
        { id: 'writesRejected', name: 'Write Tasks Rejected', sort: 'writesRejected', dataField: 'writesRejected', doStats: true, width: 100, dataFunction: roundCommaString },
        { id: 'writesRejectedDelta', name: 'Write Tasks Rejected/s', sort: 'writesRejectedDelta', dataField: 'writesRejectedDelta', doStats: true, width: 100, dataFunction: roundCommaString },
        { id: 'writesCompleted', name: 'Write Tasks Completed', sort: 'writesCompleted', dataField: 'writesCompleted', doStats: true, width: 100, dataFunction: roundCommaString },
        { id: 'writesCompletedDelta', name: 'Write Tasks Completed/s', sort: 'writesCompletedDelta', dataField: 'writesCompletedDelta', doStats: true, width: 100, dataFunction: roundCommaString },
        { id: 'writesQueueSize', name: 'Write Tasks Q Limit', sort: 'writesQueueSize', dataField: 'writesQueueSize', doStats: true, width: 100, dataFunction: roundCommaString }
      ]
    };
  },
  computed: {
    loading: {
      get: function () {
        return this.$store.state.loadingData;
      },
      set: function (newValue) {
        this.$store.commit('setLoadingData', newValue);
      }
    }
  },
  watch: {
    dataInterval: function () {
      if (reqPromise) { // cancel the interval and reset it if necessary
        clearInterval(reqPromise);
        reqPromise = null;

        if (this.dataInterval === '0') { return; }

        this.setRequestInterval();
      } else if (this.dataInterval !== '0') {
        this.loadData();
        this.setRequestInterval();
      }
    },
    refreshData: function () {
      if (this.refreshData) {
        this.loadData();
      }
    }
  },
  created: function () {
    // set a recurring server req if necessary
    if (this.dataInterval !== '0') {
      this.setRequestInterval();
    }
  },
  methods: {
    /* exposed page functions ------------------------------------ */
    exclude: function (type, column) {
      this.$http.post(`esshard/exclude/${type}/${column[type]}`)
        .then((response) => {
          if (type === 'name') {
            column.nodeExcluded = true;
          } else {
            column.ipExcluded = true;
          }
        }, (error) => {
          this.error = error.text || error;
        });
    },
    include: function (type, column) {
      this.$http.post(`esshard/include/${type}/${column[type]}`)
        .then((response) => {
          if (type === 'name') {
            column.nodeExcluded = false;
          } else {
            column.ipExcluded = false;
          }
        }, (error) => {
          this.error = error.text || error;
        });
    },
    /* helper functions ------------------------------------------ */
    setRequestInterval: function () {
      reqPromise = setInterval(() => {
        if (respondedAt && Date.now() - respondedAt >= parseInt(this.dataInterval)) {
          this.loadData();
        }
      }, 500);
    },
    loadData: function (sortField, desc) {
      this.loading = true;
      respondedAt = undefined;

      this.query.filter = this.searchTerm;

      if (desc !== undefined) { this.query.desc = desc; }
      if (sortField) { this.query.sortField = sortField; }

      this.$http.get('esstats.json', { params: this.query })
        .then((response) => {
          respondedAt = Date.now();
          this.error = '';
          this.loading = false;
          this.initialLoading = false;
          this.stats = response.data.data;
        }, (error) => {
          respondedAt = undefined;
          this.loading = false;
          this.initialLoading = false;
          this.error = error.text || error;
        });
    }
  },
  beforeDestroy: function () {
    if (reqPromise) {
      clearInterval(reqPromise);
      reqPromise = null;
    }
  }
};
</script>

<style scoped>
td {
  white-space: nowrap;
}
tr.bold {
  font-weight: bold;
}
table.table tr.border-bottom-bold > td {
  border-bottom: 2px solid #dee2e6;
}
table.table tr.border-top-bold > td {
  border-top: 2px solid #dee2e6;
}

.table .hover-menu:hover .btn-group {
  visibility: visible;
}
.table .hover-menu .btn-group {
  visibility: hidden;
}
</style>
