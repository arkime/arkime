<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="container-fluid mt-2">
    <arkime-loading v-if="initialLoading && !error" />

    <arkime-error
      v-if="error"
      :message="error" />

    <div v-show="!error">
      <button
        type="button"
        id="cancelAllTasks"
        @click="cancelTasks"
        v-has-role="{user:user,roles:'arkimeAdmin'}"
        class="pull-right btn btn-sm btn-warning">
        <span class="fa fa-ban" />&nbsp;
        {{ $t('stats.esTasks.cancelAll') }}
        <BTooltip target="cancelAllTasks">
          {{ $t('stats.esTasks.cancelAllTip') }}
        </BTooltip>
      </button>

      <arkime-paging
        v-if="stats"
        class="mt-2"
        :info-only="true"
        :records-total="recordsTotal"
        :records-filtered="recordsFiltered" />

      <arkime-table
        id="esTasksTable"
        :data="stats"
        :load-data="loadData"
        :columns="columns"
        :no-results="true"
        :action-column="true"
        :desc="query.desc"
        :sort-field="query.sortField"
        :no-results-msg="$t( cluster ? 'stats.noResultsCluster' : 'stats.noResults' )"
        page="esTasks"
        table-state-name="esTasksCols"
        table-widths-state-name="esTasksColWidths"
        table-classes="table-sm table-hover text-end small mt-2">
        <template #actions="item">
          <a
            v-if="item.item.cancellable"
            class="btn btn-xs btn-danger"
            @click="cancelTask(item.item.taskId)"
            v-has-role="{user:user,roles:'arkimeAdmin'}">
            <span class="fa fa-trash-o" />
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
import { roundCommaString, timezoneDateString } from '@common/vueFilters.js';

let reqPromise; // promise returned from setInterval for recurring requests
let respondedAt; // the time that the last data load successfully responded

export default {
  name: 'EsTasks',
  emits: ['errored'],
  props: {
    user: {
      type: Object,
      default: () => ({})
    },
    dataInterval: {
      type: Number,
      default: 5000
    },
    refreshData: {
      type: Boolean,
      default: false
    },
    searchTerm: {
      type: String,
      default: ''
    },
    pageSize: {
      type: Number,
      default: 50
    },
    cluster: {
      type: String,
      default: ''
    }
  },
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
      }
    };
  },
  computed: {
    columns: function () {
      const $t = this.$t.bind(this);
      function intl(obj) {
        obj.name = $t('stats.esTasks.' + obj.id.replace(/\./g, '-'));
        return obj;
      }

      return [ // es tasks table columns
        // default columns
        intl({ id: 'action', classes: 'text-start', sort: 'action', default: true, width: 200 }),
        intl({ id: 'description', classes: 'text-start break-all', sort: 'description', default: true, width: 300 }),
        intl({ id: 'start_time_in_millis', classes: 'text-start', sort: 'start_time_in_millis', width: 180, default: true, dataFunction: (item) => { return timezoneDateString(item.start_time_in_millis, this.user.settings.timezone, this.user.settings.ms); } }),
        intl({ id: 'running_time_in_nanos', sort: 'running_time_in_nanos', width: 120, default: true, dataFunction: (item) => { return roundCommaString(item.running_time_in_nanos / 1000000, 1); } }),
        intl({ id: 'childrenCount', sort: 'childrenCount', default: true, width: 100, dataFunction: (item) => { return roundCommaString(item.childrenCount); } }),
        intl({ id: 'user', classes: 'text-start', sort: 'user', default: true, width: 100 }),
        // all the rest of the available stats
        intl({ id: 'cancellable', sort: 'cancellable', width: 100 }),
        intl({ id: 'id', sort: 'id', width: 80 }),
        intl({ id: 'node', sort: 'node', width: 180 }),
        intl({ id: 'taskId', sort: 'taskId', width: 150 }),
        intl({ id: 'type', sort: 'type', width: 100 })
      ];
    },
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
        this.$emit('errored', error.text || String(error));
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
        this.$emit('errored', error.text || String(error));
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
        this.stats = response.data;
        this.recordsTotal = response.recordsTotal;
        this.recordsFiltered = Math.min(response.recordsFiltered, this.pageSize);
      } catch (error) {
        respondedAt = undefined;
        this.loading = false;
        this.error = error.text || String(error);
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
