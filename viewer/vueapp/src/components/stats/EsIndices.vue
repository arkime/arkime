<template>

  <div class="container-fluid mt-2">

    <moloch-loading v-if="initialLoading && !error">
    </moloch-loading>

    <moloch-error v-if="error"
      :message="error">
    </moloch-error>

    <div v-show="!error">

      <moloch-paging v-if="stats"
        class="mt-1 ml-2"
        :info-only="true"
        :records-total="recordsTotal"
        :records-filtered="recordsFiltered">
      </moloch-paging>

      <moloch-table
        id="esIndicesTable"
        :data="stats"
        :loadData="loadData"
        :columns="columns"
        :no-results="true"
        :show-avg-tot="true"
        :action-column="true"
        :desc="query.desc"
        :sortField="query.sortField"
        table-animation="list"
        table-classes="table-sm text-right small mt-2"
        table-state-name="esIndicesCols"
        table-widths-state-name="esIndicesColWidths">
        <template slot="actions"
          slot-scope="{ item }">
          <b-dropdown size="sm"
            class="row-actions-btn"
            v-has-permission="'createEnabled'">
            <b-dropdown-item
              @click.stop.prevent="confirmDeleteIndex(item.index)">
              Delete Index {{ item.index }}
            </b-dropdown-item>
            <b-dropdown-item
              @click="optimizeIndex(item.index)">
              Optimize Index {{ item.index }}
            </b-dropdown-item>
            <b-dropdown-item
              v-if="item.status === 'open'"
              @click="closeIndex(item.index)">
              Close Index {{ item.index }}
            </b-dropdown-item>
            <b-dropdown-item
              v-if="item.status === 'close'"
              @click="openIndex(item.index)">
              Open Index {{ item.index }}
            </b-dropdown-item>
          </b-dropdown>
        </template>
      </moloch-table>

    </div>

  </div>

</template>

<script>
import MolochTable from '../utils/Table';
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';
import MolochPaging from '../utils/Pagination';

let reqPromise; // promise returned from setInterval for recurring requests
let respondedAt; // the time that the last data load succesfully responded

export default {
  name: 'EsIndices',
  props: [
    'user',
    'dataInterval',
    'refreshData',
    'confirm',
    'issueConfirmation',
    'searchTerm'
  ],
  components: {
    MolochTable,
    MolochError,
    MolochPaging,
    MolochLoading
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
        desc: false
      },
      columns: [ // es indices table columns
        // default columns
        { id: 'index', name: 'Name', sort: 'index', doStats: false, default: true, width: 200 },
        { id: 'docs.count', name: 'Documents', sort: 'docs.count', doStats: true, default: true, width: 100, dataFunction: (item) => { return this.$options.filters.roundCommaString(item['docs.count']); } },
        { id: 'store.size', name: 'Disk Size', sort: 'store.size', doStats: true, default: true, width: 100, dataFunction: (item) => { return this.$options.filters.humanReadableBytes(item['store.size']); } },
        { id: 'pri', name: 'Shards', sort: 'pri', doStats: true, default: true, width: 100, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.pri); } },
        { id: 'segmentsCount', name: 'Segments', sort: 'segmentsCount', doStats: true, default: true, width: 100, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.segmentsCount); } },
        { id: 'rep', name: 'Replicas', sort: 'rep', doStats: true, default: true, width: 100, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.rep); } },
        { id: 'memoryTotal', name: 'Memory', sort: 'memoryTotal', doStats: true, default: true, width: 100, dataFunction: (item) => { return this.$options.filters.humanReadableBytes(item.memoryTotal); } },
        { id: 'health', name: 'Health', sort: 'health', doStats: false, default: true, width: 100 },
        { id: 'status', name: 'Status', sort: 'status', doStats: false, default: true, width: 100 },
        // all the rest of the available stats
        { id: 'cd', name: 'Created Date', sort: 'cd', doStats: false, width: 150, dataFunction: (item) => { return this.$options.filters.timezoneDateString(parseInt(item.cd), this.user.settings.timezone, this.user.settings.ms); } },
        { id: 'pri.search.query_current', name: 'Current Query Phase Ops', dataField: 'pri.search.query_current', doStats: false, width: 100, dataFunction: (item) => { return this.$options.filters.roundCommaString(item['pri.search.query_current']); } },
        { id: 'uuid', name: 'UUID', sort: 'uuid', doStats: false, width: 100 },
        { id: 'molochtype', name: 'Hot/Warm', sort: 'molochtype', doStats: false, width: 100 },
        { id: 'shardsPerNode', name: 'Shards/Node', sort: 'shardsPerNode', doStats: false, width: 100 },
        { id: 'versionCreated', name: 'ES Version', sort: 'versionCreated', doStats: false, width: 100 }
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
    deleteIndex (indexName) {
      this.$http.delete(`esindices/${indexName}`)
        .then((response) => {
          for (let i = 0; i < this.stats.length; i++) {
            if (this.stats[i].index === indexName) {
              this.stats.splice(i, 1);
              return;
            }
          }
        }, (error) => {
          this.$emit('errored', error.text || error);
        });
    },
    optimizeIndex (indexName) {
      this.$http.post(`esindices/${indexName}/optimize`)
        .then((response) => {
        }, (error) => {
          this.$emit('errored', error.text || error);
        });
    },
    closeIndex (indexName) {
      this.$http.post(`esindices/${indexName}/close`)
        .then((response) => {
        }, (error) => {
          this.$emit('errored', error.text || error);
        });
    },
    openIndex (indexName) {
      this.$http.post(`esindices/${indexName}/open`)
        .then((response) => {
        }, (error) => {
          this.$emit('errored', error.text || error);
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
      this.loading = true;
      respondedAt = undefined;

      this.query.filter = this.searchTerm;

      if (desc !== undefined) { this.query.desc = desc; }
      if (sortField) { this.query.sortField = sortField; }

      this.$http.get('esindices/list', { params: this.query })
        .then((response) => {
          respondedAt = Date.now();
          this.error = '';
          this.loading = false;
          this.initialLoading = false;
          this.stats = response.data.data;
          this.recordsTotal = response.data.recordsTotal;
          this.recordsFiltered = response.data.recordsFiltered;
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
