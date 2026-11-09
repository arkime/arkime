<!--
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="arkime-container-fluid mt-2">
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
        id="dbQueriesTable"
        :data="stats"
        :load-data="loadData"
        :columns="columns"
        :no-results="true"
        :action-column="true"
        :desc="query.desc"
        :sort-field="query.sortField"
        :no-results-msg="$t('stats.dbQueries.noResults')"
        page="dbQueries"
        table-state-name="dbQueriesCols"
        table-widths-state-name="dbQueriesColWidths"
        table-classes="text-end small mt-2">
        <template #actions="item">
          <v-btn
            v-if="!item.item.cancelled"
            color="error"
            variant="flat"
            size="small"
            density="comfortable"
            icon
            :title="$t('stats.dbQueries.kill')"
            v-has-role="{user:user,roles:'dbAdmin'}"
            @click="killQuery(item.item.id)">
            <v-icon icon="mdi-cancel" />
          </v-btn>
        </template>
      </arkime-table>
    </div>
  </div>
</template>

<script>
import ArkimeTable from '../utils/Table.vue';
import ArkimeError from '../utils/Error.vue';
import ArkimeLoading from '../utils/Loading.vue';
import ArkimePaging from '@common/Pagination.vue';
import StatsService from './StatsService.js';
import { roundCommaString, humanReadableBytes } from '@common/vueFilters.js';
import { resolveMessage } from '@common/resolveI18nMessage';

let reqPromise; // promise returned from setInterval for recurring requests
let respondedAt; // the time that the last data load successfully responded

export default {
  name: 'DbQueries',
  emits: ['errored'],
  props: {
    db: {
      type: String,
      required: true
    },
    // the columns this db's backend fills
    fields: {
      type: Array,
      default: () => []
    },
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
      recordsTotal: null,
      recordsFiltered: null,
      query: {
        filter: this.searchTerm || undefined,
        sortField: 'elapsed',
        desc: true
      }
    };
  },
  computed: {
    columns: function () {
      const $t = this.$t.bind(this);
      function intl (obj) {
        obj.name = $t('stats.dbQueries.' + obj.id);
        return obj;
      }

      return [
        intl({ id: 'user', classes: 'text-start', sort: 'user', default: true, width: 120 }),
        intl({ id: 'elapsed', sort: 'elapsed', default: true, width: 100, dataFunction: (item) => roundCommaString(item.elapsed, 1) }),
        intl({ id: 'rowsRead', sort: 'rowsRead', default: true, width: 120, dataFunction: (item) => roundCommaString(item.rowsRead) }),
        intl({ id: 'bytesRead', sort: 'bytesRead', default: true, width: 110, dataFunction: (item) => humanReadableBytes(item.bytesRead) }),
        intl({ id: 'memory', sort: 'memory', default: true, width: 110, dataFunction: (item) => humanReadableBytes(item.memory) }),
        intl({ id: 'state', classes: 'text-start', sort: 'state', default: true, width: 140 }),
        intl({ id: 'query', classes: 'text-start break-all', default: true, width: 500 }),
        intl({ id: 'dbUser', classes: 'text-start', sort: 'dbUser', width: 120 }),
        intl({ id: 'app', classes: 'text-start', sort: 'app', width: 120 }),
        intl({ id: 'cancelled', width: 100 }),
        intl({ id: 'id', classes: 'text-start break-all', sort: 'id', width: 300 })
      ].filter(c => !this.fields.length || this.fields.includes(c.id));
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
      if (reqPromise) {
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
    pageSize: function () {
      this.loadData();
    }
  },
  created: function () {
    if (this.dataInterval !== '0') {
      this.setRequestInterval();
    }
  },
  methods: {
    async killQuery (id) {
      try {
        await StatsService.killDbQuery(this.db, id);
        const query = this.stats.find(q => q.id === id);
        if (query) { query.cancelled = true; }
      } catch (error) {
        this.$emit('errored', resolveMessage(error, this.$t));
      }
    },
    setRequestInterval: function () {
      reqPromise = setInterval(() => {
        if (respondedAt && Date.now() - respondedAt >= parseInt(this.dataInterval)) {
          this.loadData();
        }
      }, 500);
    },
    async loadData (sortField, desc) {
      this.loading = true;
      respondedAt = undefined;

      this.query.filter = this.searchTerm || undefined;
      this.query.size = this.pageSize || 500;
      if (desc !== undefined) { this.query.desc = desc; }
      if (sortField) { this.query.sortField = sortField; }

      try {
        const response = await StatsService.getDbQueries(this.db, this.query);
        respondedAt = Date.now();
        this.error = '';
        this.stats = response.data;
        this.recordsTotal = response.recordsTotal;
        this.recordsFiltered = response.recordsFiltered;
      } catch (error) {
        respondedAt = undefined;
        this.error = resolveMessage(error, this.$t);
      }

      this.loading = false;
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
#dbQueriesTable td {
  vertical-align: top;
}
</style>
