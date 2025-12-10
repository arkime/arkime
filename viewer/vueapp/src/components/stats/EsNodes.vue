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
        :records-filtered="filteredStats.length" />

      <arkime-table
        id="esNodesTable"
        @toggle-data-node-only="showOnlyDataNodes = !showOnlyDataNodes"
        :data="filteredStats"
        :load-data="loadData"
        :columns="columns"
        :no-results="true"
        :show-avg-tot="true"
        :action-column="true"
        :desc="query.desc"
        :sort-field="query.sortField"
        :no-results-msg="$t( cluster ? 'stats.noResultsCluster' : 'stats.noResults' )"
        page="esNodes"
        table-animation="list"
        table-state-name="esNodesCols"
        table-widths-state-name="esNodesColWidths"
        table-classes="table-sm table-hover text-end small mt-2">
        <template #actions="item">
          <span class="no-wrap">
            <b-dropdown
              size="xs"
              class="row-actions-btn d-inline"
              v-has-role="{user:user,roles:'arkimeAdmin'}">
              <b-dropdown-item
                v-if="!item.item.nodeExcluded"
                @click="exclude('name', item.item)">
                {{ $t('stats.excludeNode') }}: {{ item.item.name }}
              </b-dropdown-item>
              <b-dropdown-item
                v-else
                @click="include('name', item.item)">
                {{ $t('stats.includeNode') }}: {{ item.item.name }}
              </b-dropdown-item>
              <b-dropdown-item
                v-if="!item.item.ipExcluded"
                @click="exclude('ip', item.item)">
                {{ $t('stats.excludeIp') }}: {{ item.item.ip }}
              </b-dropdown-item>
              <b-dropdown-item
                v-else
                @click="include('ip', item.item)">
                {{ $t('stats.includeIp') }}: {{ item.item.ip }}
              </b-dropdown-item>
            </b-dropdown>
            <span
              class="node-badge badge bg-primary badge-pill ms-1"
              :class="{'show-badge cursor-help': item.item.roles.indexOf('master') > -1, 'badge-master':item.item.isMaster}">
              <template v-if="item.item.isMaster">
                <span :id="'mainMasterBadge' + item.item.name">
                  M
                </span>
                <BTooltip :target="'mainMasterBadge' + item.item.name">{{ $t('stats.esNodes.mainManaging') }}</BTooltip>
              </template>
              <template v-else>
                <span :id="'masterBadge' + item.item.name">
                  m
                </span>
                <BTooltip :target="'masterBadge' + item.item.name">{{ $t('stats.esNodes.managing') }}</BTooltip>
              </template>
            </span>
            <span
              class="node-badge badge bg-primary badge-pill ms-1"
              style="padding-left:.5rem;"
              :class="{'show-badge cursor-help': item.item.roles.some(role => role.startsWith('data'))}">
              <span
                v-if="item.item.roles.some(role => role.startsWith('data'))"
                :id="'dataBadge' + item.item.name">
                D
                <BTooltip :target="'dataBadge' + item.item.name">{{ $t('stats.esNodes.data') }}</BTooltip>
              </span>
              <span v-else>&nbsp;</span>
            </span>
          </span>
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
import { roundCommaString, humanReadableBytes, readableTimeCompact } from '@common/vueFilters.js';

let reqPromise; // promise returned from setInterval for recurring requests
let respondedAt; // the time that the last data load successfully responded

export default {
  name: 'EsStats',
  props: {
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
      error: '',
      showOnlyDataNodes: false,
      initialLoading: true,
      stats: null,
      recordsTotal: null,
      query: {
        filter: this.searchTerm || undefined,
        sortField: 'nodeName',
        desc: false,
        cluster: this.cluster || undefined
      }
    };
  },
  computed: {
    columns: function () {
      const $t = this.$t.bind(this);
      function intl(obj) {
        obj.name = $t('stats.esNodes.' + obj.id.replace(/\./g, '-'));
        return obj;
      }

      return [ // es stats table columns
        // default columns
        intl({ id: 'name', classes: 'text-start', sort: 'nodeName', doStats: false, default: true, width: 120 }),
        intl({ id: 'docs', sort: 'docs', doStats: true, default: true, width: 120, dataFunction: (item) => { return roundCommaString(item.docs); } }),
        intl({ id: 'storeSize', sort: 'storeSize', doStats: true, default: true, width: 105, dataFunction: (item) => { return humanReadableBytes(item.storeSize); } }),
        intl({ id: 'freeSize', sort: 'freeSize', doStats: true, default: true, width: 100, dataFunction: (item) => { return humanReadableBytes(item.freeSize); } }),
        intl({ id: 'heapSize', sort: 'heapSize', doStats: true, default: true, width: 105, dataFunction: (item) => { return humanReadableBytes(item.heapSize); } }),
        intl({ id: 'load', sort: 'load', doStats: true, default: true, width: 100, dataFunction: (item) => { return roundCommaString(item.load, 2); } }),
        intl({ id: 'cpu', sort: 'cpu', doStats: true, default: true, width: 80, dataFunction: (item) => { return roundCommaString(item.cpu, 1) + '%'; } }),
        intl({ id: 'read', sort: 'read', doStats: true, default: true, width: 90, dataFunction: (item) => { return humanReadableBytes(item.read); } }),
        intl({ id: 'write', sort: 'write', doStats: true, default: true, width: 90, dataFunction: (item) => { return humanReadableBytes(item.write); } }),
        intl({ id: 'searches', sort: 'searches', doStats: true, width: 100, default: true, dataFunction: (item) => { return roundCommaString(item.searches); } }),
        // all the rest of the available stats
        intl({ id: 'scrolls', sort: 'scrolls', doStats: true, width: 100, dataFunction: (item) => { return roundCommaString(item.scrolls); } }),
        intl({ id: 'ip', sort: 'ip', doStats: false, width: 100 }),
        intl({ id: 'ipExcluded', sort: 'ipExcluded', doStats: false, width: 100 }),
        intl({ id: 'nodeExcluded', classes: 'text-start', sort: 'nodeExcluded', doStats: false, width: 125 }),
        intl({ id: 'nonHeapSize', sort: 'nonHeapSize', doStats: false, width: 100, dataFunction: (item) => { return humanReadableBytes(item.nonHeapSize); } }),
        intl({ id: 'searchesTime', sort: 'searchesTime', doStats: true, width: 100, dataFunction: (item) => { return roundCommaString(item.searchesTime); } }),
        intl({ id: 'writesRejectedDelta', sort: 'writesRejectedDelta', doStats: true, width: 100, dataFunction: (item) => { return roundCommaString(item.writesRejectedDelta); } }),
        intl({ id: 'writesCompleted', sort: 'writesCompleted', doStats: true, width: 100, canClear: true, dataFunction: (item) => { return roundCommaString(item.writesCompleted); } }),
        intl({ id: 'writesCompletedDelta', sort: 'writesCompletedDelta', doStats: true, width: 100, dataFunction: (item) => { return roundCommaString(item.writesCompletedDelta); } }),
        intl({ id: 'writesQueueSize', sort: 'writesQueueSize', doStats: true, width: 100, dataFunction: (item) => { return roundCommaString(item.writesQueueSize); } }),
        intl({ id: 'molochtype', sort: 'molochtype', doStats: false, width: 100 }),
        intl({ id: 'molochzone', sort: 'molochzone', doStats: false, width: 100 }),
        intl({ id: 'shards', sort: 'shards', doStats: true, default: false, width: 80, dataFunction: (item) => { return roundCommaString(item.shards); } }),
        intl({ id: 'segments', sort: 'segments', doStats: true, default: false, width: 100, dataFunction: (item) => { return roundCommaString(item.segments); } }),
        intl({ id: 'uptime', sort: 'uptime', doStats: true, width: 100, dataFunction: (item) => { return readableTimeCompact(item.uptime * 60 * 1000); } }),
        intl({ id: 'version', sort: 'version', doStats: false, width: 100 }),
        intl({ id: 'writesRejected', sort: 'writesRejected', doStats: true, width: 100, default: false, canClear: true, dataFunction: (item) => { return roundCommaString(item.writesRejected); } })
      ];
    },
    loading: {
      get: function () {
        return this.$store.state.loadingData;
      },
      set: function (newValue) {
        this.$store.commit('setLoadingData', newValue);
      }
    },
    filteredStats: function () {
      if (this.showOnlyDataNodes) {
        return this.stats.filter(s => s.roles.some(role => role.startsWith('data')));
      }
      return this.stats;
    },
    user () {
      return this.$store.state.user;
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
    },
    cluster: function (newValue) {
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
    async exclude (type, column) {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      try {
        await StatsService.excludeShard(type, column[type], { cluster: this.query.cluster });
        if (type === 'name') {
          column.nodeExcluded = true;
        } else {
          column.ipExcluded = true;
        }
      } catch (error) {
        this.error = error.text || String(error);
      }
    },
    async include (type, column) {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      try {
        await StatsService.includeShard(type, column[type], { cluster: this.query.cluster });
        if (type === 'name') {
          column.nodeExcluded = false;
        } else {
          column.ipExcluded = false;
        }
      } catch (error) {
        this.error = error.text || String(error);
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

      if (desc !== undefined) { this.query.desc = desc; }
      if (sortField) { this.query.sortField = sortField; }

      try {
        const response = await StatsService.getDataNodes(this.query);
        respondedAt = Date.now();
        this.error = '';
        this.loading = false;
        this.stats = response.data;
        this.recordsTotal = response.recordsTotal;
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

.node-badge {
  opacity: 0;
  width: 1.6rem;
  line-height: 1.2;
  font-size: 0.75rem;
  background-color: var(--color-primary);

}
.node-badge.show-badge {
  opacity: 1;
}
.badge-master {
  background-color: var(--color-quaternary) !important;
}
</style>
