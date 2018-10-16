<template>

  <div class="container-fluid">

    <moloch-loading v-if="loading && !error">
    </moloch-loading>

    <moloch-error v-if="error"
      :message="error">
    </moloch-error>

    <div v-show="!error">

      <div class="input-group input-group-sm mt-1 mb-1">
        <div class="input-group-prepend">
          <span class="input-group-text input-group-text-fw">
            <span v-if="!shiftKeyHold"
              class="fa fa-search fa-fw">
            </span>
            <span v-else
              class="query-shortcut">
              Q
            </span>
          </span>
        </div>
        <input type="text"
          class="form-control"
          v-model="query.filter"
          v-focus-input="focusInput"
          @blur="onOffFocus"
          @input="searchForES"
          placeholder="Begin typing to search for ES nodes (hint: this input accepts regex)"
        />
        <span class="input-group-append">
          <button type="button"
            @click="clear"
            :disabled="!query.filter"
            class="btn btn-outline-secondary btn-clear-input">
            <span class="fa fa-close">
            </span>
          </button>
        </span>
      </div>

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
import FocusInput from '../utils/FocusInput';

let reqPromise; // promise returned from setInterval for recurring requests
let searchInputTimeout; // timeout to debounce the search input
let respondedAt; // the time that the last data load succesfully responded

function roundCommaString (val) {
  let result = Vue.options.filters.commaString(Vue.options.filters.round(val, 0));
  return result;
};

export default {
  name: 'EsStats',
  props: [ 'dataInterval', 'refreshData' ],
  components: { MolochError, MolochLoading, MolochTable },
  directives: { FocusInput },
  data: function () {
    return {
      error: '',
      loading: true,
      stats: null,
      query: {
        filter: null,
        sortField: 'nodeName',
        desc: false
      },
      columns: [ // es stats table columns
        // default columns
        { id: 'nodeName', name: 'Name', sort: 'nodeName', dataField: 'name', doStats: false, default: true, width: 80 },
        { id: 'docs', name: 'Documents', sort: 'docs', dataField: 'docs', doStats: true, default: true, width: 100, dataFunction: roundCommaString },
        { id: 'storeSize', name: 'Disk Storage', sort: 'storeSize', dataField: 'storeSize', doStats: true, default: true, width: 120, dataFunction: (val) => { return this.$options.filters.humanReadableBytes(val); } },
        { id: 'heapSize', name: 'Heap Size', sort: 'heapSize', dataField: 'heapSize', doStats: true, default: true, width: 120, dataFunction: (val) => { return this.$options.filters.humanReadableBytes(val); } },
        { id: 'load', name: 'OS Load', sort: 'load', dataField: 'load', doStats: true, default: true, width: 120, dataFunction: (val) => { return this.$options.filters.commaString(this.$options.filters.round(val, 2)); } },
        { id: 'cpu', name: 'CPU', sort: 'cpu', dataField: 'cpu', doStats: true, default: true, width: 80, dataFunction: (val) => { return this.$options.filters.commaString(this.$options.filters.round(val, 1)) + '%'; } },
        { id: 'read', name: 'Read/s', sort: 'read', dataField: 'read', doStats: true, default: true, width: 120, dataFunction: (val) => { return this.$options.filters.humanReadableBytes(val); } },
        { id: 'write', name: 'Write/s', sort: 'write', dataField: 'write', doStats: true, default: true, width: 120, dataFunction: (val) => { return this.$options.filters.humanReadableBytes(val); } },
        { id: 'searches', name: 'Searches/s', sort: 'searches', dataField: 'searches', doStats: true, width: 120, default: true, dataFunction: roundCommaString },
        // all the rest of the available stats
        { id: 'ip', name: 'IP', sort: 'ip', dataField: 'ip', doStats: false, width: 120 },
        { id: 'ipExcluded', name: 'IP Excluded', sort: 'ipExcluded', dataField: 'ipExcluded', doStats: false, width: 120 },
        { id: 'nodeExcluded', name: 'Node Excluded', sort: 'nodeExcluded', dataField: 'nodeExcluded', doStats: false, width: 120 },
        { id: 'nonHeapSize', name: 'Non Heap Size', sort: 'nonHeapSize', dataField: 'nonHeapSize', doStats: false, width: 120, dataFunction: (val) => { return this.$options.filters.humanReadableBytes(val); } },
        { id: 'searchesTime', name: 'Searches timeout', sort: 'searchesTime', dataField: 'searchesTime', doStats: true, width: 120, dataFunction: roundCommaString }
      ]
    };
  },
  computed: {
    focusInput: {
      get: function () {
        return this.$store.state.focusSearch;
      },
      set: function (newValue) {
        this.$store.commit('setFocusSearch', newValue);
      }
    },
    shiftKeyHold: function () {
      return this.$store.state.shiftKeyHold;
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
    searchForES () {
      if (searchInputTimeout) { clearTimeout(searchInputTimeout); }
      // debounce the input so it only issues a request after keyups cease for 400ms
      searchInputTimeout = setTimeout(() => {
        searchInputTimeout = null;
        this.loadData();
      }, 400);
    },
    clear () {
      this.query.filter = undefined;
      this.loadData();
    },
    onOffFocus: function () {
      this.focusInput = false;
    },
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
      respondedAt = undefined;

      if (desc !== undefined) { this.query.desc = desc; }
      if (sortField) { this.query.sortField = sortField; }

      this.$http.get('esstats.json', { params: this.query })
        .then((response) => {
          respondedAt = Date.now();
          this.error = '';
          this.loading = false;
          this.stats = response.data.data;
        }, (error) => {
          respondedAt = undefined;
          this.loading = false;
          this.error = error;
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
