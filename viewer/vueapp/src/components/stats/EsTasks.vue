<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>

  <div class="container-fluid mt-2">

    <arkime-loading v-if="initialLoading && !error">
    </arkime-loading>

    <arkime-error v-if="error"
      :message="error">
    </arkime-error>

    <div v-show="!error">

      <button type="button"
        id="cancelAllTasks"
        @click="cancelTasks"
        v-has-role="{user:user,roles:'arkimeAdmin'}"
        class="pull-right btn btn-sm btn-warning">
        <span class="fa fa-ban"></span>&nbsp;
        Cancel ALL Tasks
        <BTooltip target="cancelAllTasks">Cancel all tasks that can be cancelled</BTooltip>
      </button>

      <arkime-paging v-if="stats"
        class="mt-1 ms-2"
        :info-only="true"
        :records-total="recordsTotal"
        :records-filtered="recordsFiltered">
      </arkime-paging>

      <arkime-table
        id="esTasksTable"
        :data="stats"
        :loadData="loadData"
        :columns="columns"
        :no-results="true"
        :action-column="true"
        :desc="query.desc"
        :sortField="query.sortField"
        :no-results-msg="`No results match your search.${cluster ? 'Try selecting a different cluster.' : ''}`"
        page="esTasks"
        table-state-name="esTasksCols"
        table-widths-state-name="esTasksColWidths"
        table-classes="table-sm table-hover text-end small mt-2">
        <template slot="actions"
          slot-scope="{ item }">
          <a v-if="item.cancellable"
            class="btn btn-xs btn-danger"
            @click="cancelTask(item.taskId)"
            v-has-role="{user:user,roles:'arkimeAdmin'}">
            <span class="fa fa-trash-o">
            </span>
          </a>
        </template>
      </arkime-table>

    </div>

  </div>

</template>

<script>
import Utils from '../utils/utils';
import ArkimeTable from '../utils/Table.vue';
import ArkimeError from '../utils/Error.vue';
import ArkimeLoading from '../utils/Loading.vue';
import ArkimePaging from '../utils/Pagination.vue';
import StatsService from './StatsService.js';
import { roundCommaString, timezoneDateString } from '@real_common/vueFilters.js';

let reqPromise; // promise returned from setInterval for recurring requests
let respondedAt; // the time that the last data load successfully responded

export default {
  name: 'EsTasks',
  props: [
    'user',
    'dataInterval',
    'refreshData',
    'searchTerm',
    'pageSize',
    'cluster'
  ],
  components: {
    ArkimeTable,
    ArkimeError,
    ArkimePaging,
    ArkimeLoading
  },
  data: function () {
    return {
      stats: null,
      error: '',
      initialLoading: true,
      totalValues: null,
      averageValues: null,
      recordsTotal: null,
      recordsFiltered: null,
      query: {
        filter: this.searchTerm || undefined,
        sortField: 'action',
        desc: false,
        cancellable: false,
        cluster: this.cluster || undefined
      },
      columns: [ // es tasks table columns
        // default columns
        { id: 'action', name: 'Action', classes: 'text-start', sort: 'action', default: true, width: 200 },
        { id: 'description', name: 'Description', classes: 'text-start break-all', sort: 'description', default: true, width: 300 },
        { id: 'start_time_in_millis', name: 'Start Time', classes: 'text-start', sort: 'start_time_in_millis', width: 180, default: true, dataFunction: (item) => { return timezoneDateString(item.start_time_in_millis, this.user.settings.timezone, this.user.settings.ms); } },
        { id: 'running_time_in_nanos', name: 'Running Time', sort: 'running_time_in_nanos', width: 120, default: true, dataFunction: (item) => { return roundCommaString(item.running_time_in_nanos / 1000000, 1); } },
        { id: 'childrenCount', name: 'Children', sort: 'childrenCount', default: true, width: 100, dataFunction: (item) => { return roundCommaString(item.childrenCount); } },
        { id: 'user', name: 'User', classes: 'text-start', sort: 'user', default: true, width: 100 },
        // all the rest of the available stats
        { id: 'cancellable', name: 'Cancellable', sort: 'cancellable', width: 100 },
        { id: 'id', name: 'ID', sort: 'id', width: 80 },
        { id: 'node', name: 'Node', sort: 'node', width: 180 },
        { id: 'taskId', name: 'Task ID', sort: 'taskId', width: 150 },
        { id: 'type', name: 'Type', sort: 'type', width: 100 }
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
    },
    pageSize: function () {
      this.loadData();
    },
    cluster: function () {
      this.query.cluster = this.cluster;
      this.loadData();
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
    async cancelTask (taskId) {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      try {
        await StatsService.cancelTask(taskId, this.query);
        // remove the task from the list
        for (let i = 0, len = this.stats.length; i < len; i++) {
          if (this.stats[i].taskId === taskId) {
            this.stats.splice(i, 1);
            return;
          }
        }
      } catch (error) {
        this.$emit('errored', error.text || error);
      }
    },
    async cancelTasks () {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      try {
        await StatsService.cancelAllTasks(this.query);
        // remove cancellable tasks
        for (let i = this.stats.length - 1, len = 0; i >= len; i--) {
          if (this.stats[i].cancellable) {
            this.stats.splice(i, 1);
          }
        }
      } catch (error) {
        this.$emit('errored', error.text || error);
      }
    },
    /* helper functions ------------------------------------------ */
    setRequestInterval: function () {
      reqPromise = setInterval(() => {
        if (respondedAt && Date.now() - respondedAt >= parseInt(this.dataInterval)) {
          this.loadData();
        }
      }, 500);
    },
    async loadData (sortField, desc) {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      this.loading = true;
      respondedAt = undefined;

      this.query.filter = this.searchTerm;

      this.query.size = this.pageSize || 500;

      if (desc !== undefined) { this.query.desc = desc; }
      if (sortField) { this.query.sortField = sortField; }

      try {
        const response = await StatsService.getTasks(this.query);
        respondedAt = Date.now();
        this.error = '';
        this.loading = false;
        this.stats = response.data.data;
        this.recordsTotal = response.data.recordsTotal;
        this.recordsFiltered = Math.min(response.data.recordsFiltered, this.pageSize);
      } catch (error) {
        respondedAt = undefined;
        this.loading = false;
        this.error = error.text || error;
      }

      this.initialLoading = false;
    }
  },
  beforeUnmount () {
    if (reqPromise) {
      clearInterval(reqPromise);
      reqPromise = null;
    }
  }
};
</script>

<style>
#esTasksTable td {
  vertical-align: top;
}
</style>
