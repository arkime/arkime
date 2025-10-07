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
      <arkime-paging
        v-if="stats"
        class="mt-2"
        :info-only="true"
        :records-total="recordsTotal"
        :records-filtered="recordsFiltered" />

      <arkime-table
        id="esRecoveryTable"
        :data="stats"
        :load-data="loadData"
        :columns="columns"
        :no-results="true"
        :action-column="true"
        :desc="query.desc"
        :sort-field="query.sortField"
        :no-results-msg="$t( cluster ? 'stats.noResultsCluster' : 'stats.noResults' )"
        page="esRecovery"
        table-state-name="esRecoveryCols"
        table-widths-state-name="esRecoveryColWidths"
        table-classes="table-sm table-hover text-end small mt-2" />
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

let reqPromise; // promise returned from setInterval for recurring requests
let respondedAt; // the time that the last data load successfully responded

export default {
  name: 'EsRecovery',
  props: {
    user: {
      type: Object,
      default: () => ({})
    },
    dataInterval: {
      type: Number,
      default: 5000
    },
    recoveryShow: {
      type: String,
      default: 'notdone'
    },
    refreshData: {
      type: Boolean,
      default: false
    },
    searchTerm: {
      type: String,
      default: ''
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
        sortField: 'index',
        desc: false,
        show: this.recoveryShow || 'notdone',
        cluster: this.cluster || undefined
      }
    };
  },
  computed: {
    columns: function () {
      const $t = this.$t.bind(this);
      function intl(obj) {
        obj.name = $t('stats.esRecovery.' + obj.id.replace(/\./g, '-'));
        return obj;
      }

      return [ // es recovery table columns
        // default columns
        intl({ id: 'index', classes: 'text-start', sort: 'index', default: true, width: 200 }),
        intl({ id: 'shard', sort: 'shard', default: true, width: 80 }),
        intl({ id: 'time', sort: 'time', default: true, width: 80 }),
        intl({ id: 'type', sort: 'type', default: true, width: 100 }),
        intl({ id: 'stage', sort: 'stage', default: true, width: 100 }),
        intl({ id: 'source_host', classes: 'text-start', sort: 'source_host', default: false, width: 200 }),
        intl({ id: 'source_node', classes: 'text-start', sort: 'source_node', default: true, width: 120 }),
        intl({ id: 'target_host', classes: 'text-start', sort: 'target_host', default: false, width: 200 }),
        intl({ id: 'target_node', classes: 'text-start', sort: 'target_node', default: true, width: 120 }),
        intl({ id: 'files', sort: 'files', default: false, width: 100 }),
        intl({ id: 'files_recovered', sort: 'files_recovered', default: false, width: 100 }),
        intl({ id: 'files_percent', sort: 'files_percent', default: true, width: 80 }),
        intl({ id: 'files_total', sort: 'files_total', default: false, width: 100 }),
        intl({ id: 'bytes', sort: 'bytes', default: false, width: 100 }),
        intl({ id: 'bytes_recovered', sort: 'bytes_recovered', default: false, width: 100 }),
        intl({ id: 'bytes_percent', sort: 'bytes_percent', default: true, width: 80 }),
        intl({ id: 'bytes_total', sort: 'bytes_total', default: false, width: 100 }),
        intl({ id: 'translog_ops', sort: 'translog_ops', default: false, width: 100 }),
        intl({ id: 'translog_ops_recovered', sort: 'translog_ops_recovered', default: false, width: 100 }),
        intl({ id: 'translog_ops_percent', sort: 'translog_ops_percent', default: true, width: 100 })
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
    recoveryShow: function () {
      this.query.show = this.recoveryShow;
      this.loadData();
    },
    refreshData: function () {
      if (this.refreshData) {
        this.loadData();
      }
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

      // this.query.all = 'true';
      this.query.filter = this.searchTerm;

      if (desc !== undefined) { this.query.desc = desc; }
      if (sortField) { this.query.sortField = sortField; }

      try {
        const response = await StatsService.getRecovery(this.query);
        respondedAt = Date.now();
        this.error = '';
        this.loading = false;
        this.initialLoading = false;
        this.stats = response.data;
        this.recordsTotal = response.recordsTotal;
        this.recordsFiltered = response.recordsFiltered;
      } catch (error) {
        respondedAt = undefined;
        this.loading = false;
        this.initialLoading = false;
        this.error = error.text || error;
      }
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
