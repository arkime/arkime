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
        id="dbTablesTable"
        :data="stats"
        :load-data="loadData"
        :columns="columns"
        :no-results="true"
        :show-avg-tot="true"
        :action-column="true"
        :desc="query.desc"
        :sort-field="query.sortField"
        :no-results-msg="$t('stats.dbTables.noResults')"
        page="dbTables"
        table-state-name="dbTablesCols"
        table-widths-state-name="dbTablesColWidths"
        table-classes="text-end small mt-2">
        <template #actions="item">
          <span
            v-if="actions.length"
            v-has-role="{user:user,roles:'dbAdmin'}">
            <v-menu>
              <template #activator="{ props: activatorProps }">
                <v-btn
                  v-bind="activatorProps"
                  variant="outlined"
                  size="x-small"
                  density="comfortable"
                  icon
                  class="row-actions-btn">
                  <v-icon icon="mdi-menu-down" />
                </v-btn>
              </template>
              <v-list density="compact">
                <template
                  v-for="action in actions"
                  :key="action">
                  <v-list-item
                    v-if="action !== 'drop' || user?.removeEnabled"
                    @click.stop.prevent="runAction(item.item, action)">
                    {{ $t('stats.dbTables.' + action) }} {{ itemName(item.item) }}
                  </v-list-item>
                </template>
              </v-list>
            </v-menu>
          </span>
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
import { roundCommaString, humanReadableBytes, timezoneDateString } from '@common/vueFilters.js';
import { resolveMessage } from '@common/resolveI18nMessage';

let reqPromise; // promise returned from setInterval for recurring requests
let respondedAt; // the time that the last data load successfully responded

export default {
  name: 'DbTables',
  emits: ['errored', 'confirm'],
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
    // the actions this db's backend supports
    actions: {
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
    issueConfirmation: {
      type: String,
      default: undefined
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
        sortField: 'table',
        desc: false
      }
    };
  },
  computed: {
    columns: function () {
      const $t = this.$t.bind(this);
      function intl (obj) {
        obj.name = $t('stats.dbTables.' + obj.id);
        return obj;
      }
      const count = (id) => (item) => item[id] === undefined || item[id] === null ? '' : roundCommaString(item[id]);
      const bytes = (id) => (item) => item[id] === undefined || item[id] === null ? '' : humanReadableBytes(item[id]);
      const date = (id) => (item) => item[id] ? timezoneDateString(item[id], this.user.settings?.timezone, false) : '';

      return [
        intl({ id: 'table', classes: 'text-start', sort: 'table', default: true, width: 220 }),
        intl({ id: 'partition', classes: 'text-start', sort: 'partition', default: true, width: 110 }),
        intl({ id: 'rows', sort: 'rows', default: true, width: 110, doStats: true, dataFunction: count('rows') }),
        intl({ id: 'dead', sort: 'dead', default: true, width: 100, doStats: true, dataFunction: count('dead') }),
        intl({ id: 'parts', sort: 'parts', default: true, width: 80, doStats: true, dataFunction: count('parts') }),
        intl({ id: 'patchParts', sort: 'patchParts', default: true, width: 100, doStats: true, dataFunction: count('patchParts') }),
        intl({ id: 'bytes', sort: 'bytes', default: true, width: 100, doStats: true, dataFunction: bytes('bytes') }),
        intl({ id: 'tableBytes', sort: 'tableBytes', default: true, width: 110, doStats: true, dataFunction: bytes('tableBytes') }),
        intl({ id: 'indexBytes', sort: 'indexBytes', default: true, width: 110, doStats: true, dataFunction: bytes('indexBytes') }),
        intl({ id: 'memory', sort: 'memory', default: true, width: 100, doStats: true, dataFunction: bytes('memory') }),
        intl({ id: 'cached', sort: 'cached', default: true, width: 100, doStats: true, dataFunction: bytes('cached') }),
        intl({ id: 'ratio', default: true, width: 90, dataFunction: (item) => item.compressed ? roundCommaString(item.uncompressed / item.compressed, 1) + 'x' : '' }),
        intl({ id: 'first', classes: 'text-start', sort: 'first', default: true, width: 170, dataFunction: date('first') }),
        intl({ id: 'last', classes: 'text-start', sort: 'last', default: true, width: 170, dataFunction: date('last') }),
        intl({ id: 'lastVacuum', classes: 'text-start', sort: 'lastVacuum', default: true, width: 170, dataFunction: date('lastVacuum') }),
        intl({ id: 'lastAnalyze', classes: 'text-start', sort: 'lastAnalyze', default: true, width: 170, dataFunction: date('lastAnalyze') }),
        intl({ id: 'compressed', sort: 'compressed', width: 110, doStats: true, dataFunction: bytes('compressed') }),
        intl({ id: 'uncompressed', sort: 'uncompressed', width: 120, doStats: true, dataFunction: bytes('uncompressed') }),
        intl({ id: 'modified', classes: 'text-start', sort: 'modified', width: 170, dataFunction: date('modified') })
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
    issueConfirmation: function () {
      if (this.issueConfirmation) {
        this.doAction(this.issueConfirmation, 'drop');
      }
    }
  },
  created: function () {
    if (this.dataInterval !== '0') {
      this.setRequestInterval();
    }
  },
  methods: {
    itemName (item) {
      return item.partition ? `${item.table} ${item.partition}` : item.table;
    },
    runAction (item, action) {
      if (action === 'drop') {
        this.$emit('confirm', `${this.$t('stats.dbTables.drop')}: ${this.itemName(item)}`, item.id);
        return;
      }
      this.doAction(item.id, action);
    },
    async doAction (id, action) {
      try {
        await StatsService.dbTableAction(this.db, id, action);
        this.loadData();
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
      if (desc !== undefined) { this.query.desc = desc; }
      if (sortField) { this.query.sortField = sortField; }

      try {
        const response = await StatsService.getDbTables(this.db, this.query);
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
