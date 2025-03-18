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

      <arkime-paging v-if="stats"
        class="mt-2"
        :info-only="true"
        :records-total="recordsTotal"
        :records-filtered="recordsFiltered">
      </arkime-paging>

      <arkime-table
        id="esIndicesTable"
        :data="stats"
        :loadData="loadData"
        :columns="columns"
        :no-results="true"
        :show-avg-tot="true"
        :action-column="true"
        :desc="query.desc"
        :sortField="query.sortField"
        :no-results-msg="`No results match your search.${cluster ? 'Try selecting a different cluster.' : ''}`"
        page="esIndices"
        table-animation="list"
        table-state-name="esIndicesCols"
        table-widths-state-name="esIndicesColWidths"
        table-classes="table-sm table-hover text-end small mt-2">
        <template v-slot:actions="item">
          <b-dropdown size="xs"
            class="row-actions-btn"
            v-has-role="{user:user,roles:'arkimeAdmin'}"
            v-has-permission="'removeEnabled'">
            <b-dropdown-item
              @click.stop.prevent="confirmDeleteIndex(item.item.index)">
              Delete Index {{ item.item.index }}
            </b-dropdown-item>
            <b-dropdown-item
              @click="optimizeIndex(item.item.index)">
              Optimize Index {{ item.item.index }}
            </b-dropdown-item>
            <b-dropdown-item
              v-if="item.item.status === 'open'"
              @click="closeIndex(item.item)">
              Close Index {{ item.item.index }}
            </b-dropdown-item>
            <b-dropdown-item
              v-if="item.item.status === 'close'"
              @click="openIndex(item.item)">
              Open Index {{ item.item.index }}
            </b-dropdown-item>
            <b-dropdown-item
              v-if="item.item.pri > 1"
              @click="openShrinkIndexForm(item.item)">
              Shrink Index {{ item.item.index }}
            </b-dropdown-item>
          </b-dropdown>
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
import { roundCommaString, humanReadableBytes, timezoneDateString } from '@real_common/vueFilters.js';

let reqPromise; // promise returned from setInterval for recurring requests
let respondedAt; // the time that the last data load successfully responded

export default {
  name: 'EsIndices',
  props: [
    'user',
    'dataInterval',
    'refreshData',
    'confirm',
    'issueConfirmation',
    'searchTerm',
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
        sortField: 'index',
        desc: false,
        cluster: this.cluster || undefined
      },
      columns: [ // es indices table columns
        // default columns
        { id: 'index', name: 'Name', classes: 'text-start', sort: 'index', doStats: false, default: true, width: 200 },
        { id: 'docs.count', name: 'Documents', sort: 'docs.count', doStats: true, default: true, width: 105, dataFunction: (item) => { return roundCommaString(item['docs.count']); } },
        { id: 'store.size', name: 'Disk Size', sort: 'store.size', doStats: true, default: true, width: 100, dataFunction: (item) => { return humanReadableBytes(item['store.size']); } },
        { id: 'pri', name: 'Shards', sort: 'pri', doStats: true, default: true, width: 100, dataFunction: (item) => { return roundCommaString(item.pri); } },
        { id: 'segmentsCount', name: 'Segments', sort: 'segmentsCount', doStats: true, default: true, width: 100, dataFunction: (item) => { return roundCommaString(item.segmentsCount); } },
        { id: 'rep', name: 'Replicas', sort: 'rep', doStats: true, default: true, width: 100, dataFunction: (item) => { return roundCommaString(item.rep); } },
        { id: 'memoryTotal', name: 'Memory', sort: 'memoryTotal', doStats: true, default: true, width: 100, dataFunction: (item) => { return humanReadableBytes(item.memoryTotal); } },
        { id: 'health', name: 'Health', sort: 'health', doStats: false, default: true, width: 100 },
        { id: 'status', name: 'Status', sort: 'status', doStats: false, default: true, width: 100 },
        // all the rest of the available stats
        { id: 'cd', name: 'Created Date', sort: 'cd', doStats: false, width: 150, dataFunction: (item) => { return timezoneDateString(parseInt(item.cd), this.user.settings.timezone, this.user.settings.ms); } },
        { id: 'pri.search.query_current', name: 'Current Query Phase Ops', dataField: 'pri.search.query_current', doStats: false, width: 100, dataFunction: (item) => { return roundCommaString(item['pri.search.query_current']); } },
        { id: 'uuid', name: 'UUID', sort: 'uuid', doStats: false, width: 100 },
        { id: 'molochtype', name: 'Hot/Warm', sort: 'molochtype', doStats: false, width: 100 },
        { id: 'shardsPerNode', name: 'Shards/Node', sort: 'shardsPerNode', doStats: false, width: 100 },
        { id: 'versionCreated', name: 'ES Version', sort: 'versionCreated', doStats: false, width: 100 },
        { id: 'docSize', name: 'Disk Per Doc', sort: 'docSize', doStats: true, width: 100, dataFunction: (item) => { return roundCommaString(item.docSize); } }
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
    },
    issueConfirmation: function () {
      if (this.issueConfirmation) {
        this.deleteIndex(this.issueConfirmation);
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
    /* exposed page functions ------------------------------------ */
    confirmDeleteIndex: function (indexName) {
      this.$emit('confirm', `Delete ${indexName}`, indexName);
    },
    async deleteIndex (indexName) {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      try {
        await StatsService.deleteIndex(indexName, this.query);
        for (let i = 0; i < this.stats.length; i++) {
          if (this.stats[i].index === indexName) {
            this.stats.splice(i, 1);
            return;
          }
        }
      } catch (error) {
        this.$emit('errored', error.text || error);
      }
    },
    async optimizeIndex (indexName) {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      try {
        await StatsService.optimizeIndex(indexName, this.query);
      } catch (error) {
        this.$emit('errored', error.text || error);
      }
    },
    async closeIndex (index) {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      try {
        const response = await StatsService.closeIndex(index.index, this.query);
        if (response.success) {
          this.$set(index, 'status', 'close');
        }
      } catch (error) {
        this.$emit('errored', error.text || error);
      }
    },
    async openIndex (index) {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      try {
        const response = await StatsService.openIndex(index.index, this.query);
        if (response.success) {
          this.$set(index, 'status', 'open');
        }
      } catch (error) {
        this.$emit('errored', error.text || error);
      }
    },
    openShrinkIndexForm (index) {
      this.$emit('shrink', index);
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
        const response = await StatsService.getIndices(this.query);
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

<style>
/* remove any space between dropdown button and menu to make
   sure the menu doesn't get hidden */
.hover-menu .dropdown-menu {
  margin-top: 0;
}
/* widen the button to make sure the user has enough space to
   move their mouse to the menu so that it doesn't get hidden */
.hover-menu .btn-sm {
  padding: 1px 8px !important;
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
