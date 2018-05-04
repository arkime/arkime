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
          placeholder="Begin typing to search for ES indices (hint: this input accepts regex)">
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
          <template v-if="stats && averageValues && totalValues && stats.data.length > 9">
            <tr class="bold average-row">
              <td class="text-left">Average</td>
              <td>{{ averageValues['docs.count'] | round(0) | commaString }}</td>
              <td>{{ averageValues['store.size'] | humanReadableBytes }}</td>
              <td>{{ averageValues.pri | round(0) | commaString }}</td>
              <td>{{ averageValues.segmentsCount | round(0) | commaString }}</td>
              <td>{{ averageValues.rep| round(0) | commaString }}</td>
              <td>{{ averageValues.memoryTotal | humanReadableBytes }}</td>
              <td class="text-left">-</td>
              <td class="text-left">-</td>
            </tr>
            <tr class="border-bottom-bold bold total-row">
              <td class="text-left">Total</td>
              <td>{{ totalValues['docs.count'] | round(0) | commaString }}</td>
              <td>{{ totalValues['store.size'] | humanReadableBytes }}</td>
              <td>{{ totalValues.pri | round(0) | commaString }}</td>
              <td>{{ totalValues.segmentsCount | round(0) | commaString }}</td>
              <td>{{ totalValues.rep| round(0) | commaString }}</td>
              <td>{{ totalValues.memoryTotal | humanReadableBytes }}</td>
              <td class="text-left">-</td>
              <td class="text-left">-</td>
            </tr>
          </template>
          <tr v-for="(stat, i) of stats.data"
            :key="stat.name">
            <td class="text-left">
              {{ stat.index }}
              <a class="btn btn-xs btn-danger ml-2"
                v-has-permission="'createEnabled'"
                @click="deleteIndex(i, stat.index)">
                <span class="fa fa-trash-o"></span>
              </a>
            </td>
            <td>{{ stat['docs.count'] | round(0) | commaString }}</td>
            <td>{{ stat['store.size'] | humanReadableBytes }}</td>
            <td>{{ stat.pri | round(0) | commaString }}</td>
            <td>{{ stat.segmentsCount | round(0) | commaString }}</td>
            <td>{{ stat.rep| round(0) | commaString }}</td>
            <td>{{ stat.memoryTotal | humanReadableBytes }}</td>
            <td class="text-left">{{ stat.health }}</td>
            <td class="text-left">{{ stat.status }}</td>
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
          <tr class="bold border-top-bold average-row">
            <td class="text-left">Average</td>
            <td>{{ averageValues['docs.count'] | round(0) | commaString }}</td>
            <td>{{ averageValues['store.size'] | humanReadableBytes }}</td>
            <td>{{ averageValues.pri | round(0) | commaString }}</td>
            <td>{{ averageValues.segmentsCount | round(0) | commaString }}</td>
            <td>{{ averageValues.rep| round(0) | commaString }}</td>
            <td>{{ averageValues.memoryTotal | humanReadableBytes }}</td>
            <td class="text-left">-</td>
            <td class="text-left">-</td>
          </tr>
          <tr class="border-bottom-bold bold total-row">
            <td class="text-left">Total</td>
            <td>{{ totalValues['docs.count'] | round(0) | commaString }}</td>
            <td>{{ totalValues['store.size'] | humanReadableBytes }}</td>
            <td>{{ totalValues.pri | round(0) | commaString }}</td>
            <td>{{ totalValues.segmentsCount | round(0) | commaString }}</td>
            <td>{{ totalValues.rep| round(0) | commaString }}</td>
            <td>{{ totalValues.memoryTotal | humanReadableBytes }}</td>
            <td class="text-left">-</td>
            <td class="text-left">-</td>
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
  name: 'EsIndices',
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
        sortField: 'index',
        desc: false
      },
      columns: [ // es indices table columns
        { name: 'Name', sort: 'index', doStats: false },
        { name: 'Documents', sort: 'docs.count', doStats: true },
        { name: 'Disk Size', sort: 'store.size', doStats: true },
        { name: 'Shards', sort: 'pri', doStats: true },
        { name: 'Segments', sort: 'segmentsCount', doStats: true },
        { name: 'Replicas', sort: 'rep', doStats: true },
        { name: 'Memory', sort: 'memoryTotal', doStats: true },
        { name: 'Health', sort: 'health', doStats: false },
        { name: 'Status', sort: 'status', doStats: false }
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
    deleteIndex (i, indexName) {
      this.$http.delete(`esindices/${indexName}`)
        .then((response) => {
          this.stats.data.splice(i, 1);
        }, (error) => {
          this.$emit('errored', error.text || error);
        });
    },
    /* helper functions ------------------------------------------ */
    setRequestInterval: function () {
      reqPromise = setInterval(() => {
        this.loadData();
      }, parseInt(this.dataInterval, 10));
    },
    loadData: function () {
      this.$http.get('esindices/list', { params: this.query })
        .then((response) => {
          this.error = '';
          this.loading = false;
          this.stats = response;

          this.averageValues = {};
          this.totalValues = {};
          let stats = this.stats.data;

          let columnNames = this.columns.map(function (item) {
            if (!item.doStats) { return; }
            return item.field || item.sort;
          });

          for (let i = 1; i < columnNames.length; i++) {
            const columnName = columnNames[i];
            if (columnName) {
              this.totalValues[columnName] = 0;
              for (let s = 0; s < stats.length; s++) {
                this.totalValues[columnName] += parseInt(stats[s][columnName], 10);
              }
              this.averageValues[columnName] = this.totalValues[columnName] / stats.length;
            }
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

<style scoped>
tr.bold {
  font-weight: bold;
}
table.table tr.border-bottom-bold > td {
  border-bottom: 2px solid #dee2e6;
}
table.table tr.border-top-bold > td {
  border-top: 2px solid #dee2e6;
}

table.table td .btn {
  visibility: hidden;
}
table.table td:hover .btn {
  visibility: visible;
}
</style>
