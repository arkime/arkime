<template>

  <div class="ml-1 mr-1">

    <moloch-loading v-if="loading && !error">
    </moloch-loading>

    <moloch-error v-if="error"
      :message="error">
    </moloch-error>

    <div v-show="!error">

      <div class="input-group input-group-sm mt-1">
        <div class="input-group-prepend">
          <span class="input-group-text">
            <span class="fa fa-search"></span>
          </span>
        </div>
        <input type="text"
          class="form-control"
          v-model="query.filter"
          @keyup="searchForES()"
          placeholder="Begin typing to search for ES nodes (hint: this input accepts regex)">
      </div>

      <table class="table table-sm table-striped text-right small mt-3">
        <thead>
          <tr>
            <th v-for="column of columns"
              :key="column.name"
              class="cursor-pointer"
              :class="{'text-left':!column.doStats}"
              @click="columnClick(column.sort)">
              {{ column.name }}
              <span v-if="column.sort !== undefined">
                <span v-show="query.sortField === column.sort && !query.desc" class="fa fa-sort-asc"></span>
                <span v-show="query.sortField === column.sort && query.desc" class="fa fa-sort-desc"></span>
                <span v-show="query.sortField !== column.sort" class="fa fa-sort"></span>
              </span>
            </th>
          </tr>
        </thead>
        <tbody v-if="stats">
          <template v-if="averageValues && totalValues && stats.data.length > 9">
            <tr class="bold average-row">
              <td class="text-left">Average</td>
              <td>{{ averageValues.docs | round(0) | commaString }}</td>
              <td>{{ averageValues.storeSize | humanReadableBytes }}</td>
              <td>{{ averageValues.heapSize | humanReadableBytes }}</td>
              <td>{{ averageValues.load | round(2) | commaString }}</td>
              <td>{{ averageValues.cpu | round(1) | commaString }}%</td>
              <td>{{ averageValues.read | humanReadableBytes }}</td>
              <td>{{ averageValues.write | humanReadableBytes }}</td>
              <td>{{ averageValues.searches | round(0) | commaString }}</td>
            </tr>
            <tr class="border-bottom-bold bold total-row">
              <td class="text-left">Total</td>
              <td>{{ totalValues.docs | round(0) | commaString }}</td>
              <td>{{ totalValues.storeSize | humanReadableBytes }}</td>
              <td>{{ totalValues.heapSize | humanReadableBytes }}</td>
              <td>{{ totalValues.load | round(2) | commaString }}</td>
              <td>{{ totalValues.cpu | round(1) | commaString }}%</td>
              <td>{{ totalValues.read | humanReadableBytes }}</td>
              <td>{{ totalValues.write | humanReadableBytes }}</td>
              <td>{{ totalValues.searches | round(0) | commaString }}</td>
            </tr>
          </template>
          <tr v-for="stat of stats.data"
            :key="stat.name">
            <td class="hover-menu text-left">
              {{ stat.name }}
              <!-- column dropdown menu -->
              <b-dropdown size="sm"
                class="row-actions-btn ml-1"
                v-has-permission="'createEnabled'">
                <b-dropdown-item v-if="!stat.nodeExcluded"
                  @click="exclude('name', stat)">
                  Exclude node {{ stat.name }}
                </b-dropdown-item>
                <b-dropdown-item v-if="stat.nodeExcluded"
                  @click="include('name', stat)">
                  Include node {{ stat.name }}
                </b-dropdown-item>
                <b-dropdown-item v-if="!stat.ipExcluded"
                  @click="exclude('ip', stat)">
                  Exclude IP {{ stat.ip }}
                </b-dropdown-item>
                <b-dropdown-item v-if="stat.ipExcluded"
                  @click="include('ip', stat)">
                  Include IP {{ stat.ip }}
                </b-dropdown-item>
              </b-dropdown> <!-- /column dropdown menu -->
            </td>
            <td>{{ stat.docs | round(0) | commaString }}</td>
            <td>{{ stat.storeSize | humanReadableBytes }}</td>
            <td>{{ stat.heapSize | humanReadableBytes }}</td>
            <td>{{ stat.load | round(2) | commaString }}</td>
            <td>{{ stat.cpu | round(1) | commaString }}%</td>
            <td>{{ stat.read | humanReadableBytes }}</td>
            <td>{{ stat.write | humanReadableBytes }}</td>
            <td>{{ stat.searches | round(0) | commaString }}</td>
          </tr>
          <tr v-if="stats.data && !stats.data.length">
            <td :colspan="columns.length"
              class="text-danger text-center">
              <span class="fa fa-warning"></span>&nbsp;
              No results match your search
            </td>
          </tr>
        </tbody>
        <tfoot v-if="stats && averageValues && totalValues && stats.data.length > 1">
          <tr class="bold average-row">
            <td class="text-left">Average</td>
            <td>{{ averageValues.docs | round(0) | commaString }}</td>
            <td>{{ averageValues.storeSize | humanReadableBytes }}</td>
            <td>{{ averageValues.heapSize | humanReadableBytes }}</td>
            <td>{{ averageValues.load | round(2) | commaString }}</td>
            <td>{{ averageValues.cpu | round(1) | commaString }}%</td>
            <td>{{ averageValues.read | humanReadableBytes }}</td>
            <td>{{ averageValues.write | humanReadableBytes }}</td>
            <td>{{ averageValues.searches | round(0) | commaString }}</td>
          </tr>
          <tr class="border-bottom-bold bold total-row">
            <td class="text-left">Total</td>
            <td>{{ totalValues.docs | round(0) | commaString }}</td>
            <td>{{ totalValues.storeSize | humanReadableBytes }}</td>
            <td>{{ totalValues.heapSize | humanReadableBytes }}</td>
            <td>{{ totalValues.load | round(2) | commaString }}</td>
            <td>{{ totalValues.cpu | round(1) | commaString }}%</td>
            <td>{{ totalValues.read | humanReadableBytes }}</td>
            <td>{{ totalValues.write | humanReadableBytes }}</td>
            <td>{{ totalValues.searches | round(0) | commaString }}</td>
          </tr>
        </tfoot>
      </table>

    </div>

  </div>

</template>

<script>
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';

let reqPromise; // promise returned from setInterval for recurring requests
let searchInputTimeout; // timeout to debounce the search input

export default {
  name: 'EsStats',
  props: [ 'dataInterval' ],
  components: { MolochError, MolochLoading },
  data: function () {
    return {
      stats: {},
      error: '',
      loading: true,
      totalValues: null,
      averageValues: null,
      query: {
        filter: null,
        sortField: 'nodeName',
        desc: false
      },
      columns: [ // es stats table columns
        { name: 'Name', sort: 'nodeName', doStats: false },
        { name: 'Documents', sort: 'docs', doStats: true },
        { name: 'Disk Storage', sort: 'storeSize', doStats: true },
        { name: 'Heap Size', sort: 'heapSize', doStats: true },
        { name: 'OS Load', sort: 'load', doStats: true },
        { name: 'CPU', sort: 'cpu', doStats: true },
        { name: 'Read/s', sort: 'read', doStats: true },
        { name: 'Write/s', sort: 'write', doStats: true },
        { name: 'Searches/s', sort: 'searches', doStats: true }
      ]
    };
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
    }
  },
  created: function () {
    this.loadData();
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
    columnClick (name) {
      this.query.sortField = name;
      this.query.desc = !this.query.desc;
      this.loadData();
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
        this.loadData();
      }, parseInt(this.dataInterval, 10));
    },
    loadData: function () {
      this.$http.get('esstats.json', { params: this.query })
        .then((response) => {
          this.error = '';
          this.loading = false;
          this.stats = response.data;

          this.totalValues = {};
          this.averageValues = {};
          let stats = this.stats.data;

          let columnNames = this.columns.map(function (item) {
            return item.field || item.sort;
          });

          for (let i = 1; i < columnNames.length; i++) {
            const columnName = columnNames[i];
            this.totalValues[columnName] = 0;
            for (let s = 0; s < stats.length; s++) {
              this.totalValues[columnName] += stats[s][columnName];
            }
            this.averageValues[columnName] = this.totalValues[columnName] / stats.length;
          }
        }, (error) => {
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

<style>
table .btn-group.row-actions-btn > .btn-sm {
  padding: 1px 4px;
  font-size: 13px;
  line-height: 1.2;
}
</style>

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
