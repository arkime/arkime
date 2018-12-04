<template>

  <div class="container-fluid mt-2">

    <moloch-loading v-if="initialLoading && !error">
    </moloch-loading>

    <moloch-error v-if="error"
      :message="error">
    </moloch-error>

    <div v-show="!error">

      <moloch-table
        id="esTasksTable"
        :data="stats"
        :loadData="loadData"
        :columns="columns"
        :no-results="true"
        :action-column="true"
        :desc="query.desc"
        :sortField="query.sortField"
        table-classes="table-sm text-right small"
        table-state-name="esTasksCols"
        table-widths-state-name="esTasksColWidths">
        <template slot="actions"
          slot-scope="{ item }">
          <a v-if="item.cancellable"
            class="btn btn-xs btn-danger"
            @click="cancelTask(item.taskId)"
            v-b-tooltip.hover
            title="Cancel task"
            v-has-permission="'createEnabled'">
            <span class="fa fa-trash-o">
            </span>
          </a>
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
  name: 'EsTasks',
  props: [
    'user',
    'dataInterval',
    'refreshData',
    'searchTerm'
  ],
  components: { MolochError, MolochLoading, MolochTable },
  data: function () {
    return {
      stats: null,
      error: '',
      initialLoading: true,
      totalValues: null,
      averageValues: null,
      query: {
        filter: this.searchTerm || undefined,
        sortField: 'action',
        desc: false,
        cancellable: false
      },
      columns: [ // es indices table columns
        // default columns
        { id: 'action', name: 'Action', sort: 'action', dataField: 'action', default: true, width: 200 },
        { id: 'description', name: 'Description', sort: 'description', dataField: 'description', default: true, width: 300 },
        { id: 'start_time_in_millis', name: 'Start Time', sort: 'start_time_in_millis', dataField: 'start_time_in_millis', width: 180, default: true, dataFunction: (val) => { return this.$options.filters.timezoneDateString(Math.floor(val / 1000), this.user.settings.timezone, 'YYYY/MM/DD HH:mm:ss z'); } },
        { id: 'running_time_in_nanos', name: 'Running Time', sort: 'running_time_in_nanos', dataField: 'running_time_in_nanos', width: 120, default: true, dataFunction: (val) => { return this.$options.filters.commaString(this.$options.filters.round(val / 1000000, 1)); } },
        { id: 'childrenCount', name: 'Children', sort: 'childrenCount', dataField: 'childrenCount', default: true, width: 100, dataFunction: roundCommaString },
        // all the rest of the available stats
        { id: 'cancellable', name: 'Cancellable', sort: 'cancellable', dataField: 'cancellable', width: 100 },
        { id: 'id', name: 'ID', sort: 'id', dataField: 'id', width: 80 },
        { id: 'node', name: 'Node', sort: 'node', dataField: 'node', width: 180 },
        { id: 'taskid', name: 'Task ID', sort: 'taskid', dataField: 'taskid', width: 150 },
        { id: 'type', name: 'Type', sort: 'type', dataField: 'type', width: 100 }
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
    'query.cancellable': function () {
      this.loadData();
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
    cancelTask (taskId) {
      this.$http.post('estask/cancel', { taskId: taskId });
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

      this.$http.get('estask/list', { params: this.query })
        .then((response) => {
          respondedAt = Date.now();
          this.error = '';
          this.loading = false;
          this.initialLoading = false;
          this.stats = response.data;
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
