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
          placeholder="Begin typing to search for ES tasks (hint: this input accepts regex)">
      </div>

      <table class="table table-sm text-right small mt-3">
        <thead>
          <tr>
            <th v-for="column of columns"
              :key="column.name"
              class="cursor-pointer"
              @click="columnClick(column.sort)">
              {{ column.name }}
              <span v-if="column.sort !== undefined">
                <span v-show="query.sortField === column.sort && !query.desc" class="fa fa-sort-asc"></span>
                <span v-show="query.sortField === column.sort && query.desc" class="fa fa-sort-desc"></span>
                <span v-show="query.sortField !== column.sort" class="fa fa-sort"></span>
              </span>
            </th>
            <th>
              <input type="checkbox"
                v-model="query.cancellable"
                v-has-permission="'createEnabled'">
            </th>
          </tr>
        </thead>
        <tbody v-if="stats">
          <tr v-for="stat of stats.data"
            :key="stat.name">
            <td>{{ stat.action }}</td>
            <td>{{ stat.description }}</td>
            <td>{{ stat.start_time_in_millis/1000 | timezoneDateString(user.settings.timezone, 'YYYY/MM/DD HH:mm:ss z')  }}%</td>
            <td>{{ stat.running_time_in_nanos/1000000 | round(1) | commaString }}</td>
            <td>{{ stat.childrenCount | round(0) | commaString }}</td>
            <td>
              <a v-if="stat.cancellable"
                class="btn btn-xs btn-danger"
                @click="cancelTask(stat.taskId)"
                v-b-tooltip.hover
                title="Cancel task"
                v-has-permission="'createEnabled'">
                <span class="fa fa-trash-o"></span>
              </a>
            </td>
          </tr>
          <tr v-if="stats.data && !stats.data.length">
            <td :colspan="columns.length"
              class="text-danger text-center">
              <span class="fa fa-warning"></span>&nbsp;
              No results match your search
            </td>
          </tr>
        </tbody>
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
  name: 'EsTasks',
  props: [ 'user', 'dataInterval' ],
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
        sortField: 'action',
        desc: false,
        cancellable: false
      },
      columns: [ // es indices table columns
        { name: 'Action', sort: 'action', doStats: false },
        { name: 'Description', sort: 'description', doStats: true },
        { name: 'Start Time', sort: 'start_time_in_millis', doStats: true },
        { name: 'Running Time', sort: 'running_time_in_nanos', doStats: true },
        { name: 'Children', sort: 'childrenCount', doStats: false }
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
    },
    'query.cancellable': function () {
      this.loadData();
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
    cancelTask (taskId) {
      this.$http.post('estask/cancel', { taskId: taskId });
    },
    /* helper functions ------------------------------------------ */
    setRequestInterval: function () {
      reqPromise = setInterval(() => {
        this.loadData();
      }, parseInt(this.dataInterval, 10));
    },
    loadData: function () {
      this.$http.get('estask/list', { params: this.query })
        .then((response) => {
          this.error = '';
          this.loading = false;
          this.stats = response;

          this.averageValues = {};
          this.totalValues = {};
          let stats = this.stats;

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
