<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <page-layout>
    <template #chrome>
      <ArkimeCollapsible>
        <div class="files-search px-1">
          <v-row
            dense
            align="center"
            justify="start"
            class="page-subnav">
            <v-col
              cols="auto"
              align-self="start"
              class="mt-2">
              <arkime-paging
                v-if="files"
                :records-total="recordsTotal"
                :records-filtered="recordsFiltered"
                @change-paging="changePaging"
                :length-default="500" />
            </v-col>
            <v-col>
              <div class="arkime-input-group arkime-input-group--fluid">
                <span class="arkime-input-label arkime-input-label-fw">
                  <v-icon
                    icon="mdi-magnify"
                    v-if="!shiftKeyHold" />
                  <span
                    v-else
                    class="query-shortcut">
                    Q
                  </span>
                </span>
                <input
                  type="text"
                  class="arkime-input-control"
                  v-model="query.filter"
                  v-focus="focusInput"
                  @blur="onOffFocus"
                  @input="searchForFiles"
                  @keydown.enter="searchForFiles"
                  :placeholder="$t('files.searchPlaceholder')">
                <v-btn
                  variant="text"
                  size="small"
                  density="comfortable"
                  icon
                  class="arkime-input-append-btn"
                  :disabled="!query.filter"
                  :aria-label="$t('common.clear')"
                  @click="clear">
                  <v-icon icon="mdi-close" />
                </v-btn>
              </div>
            </v-col>
            <v-col cols="auto">
              <Clusters />
            </v-col>
          </v-row>
        </div>
      </ArkimeCollapsible>
    </template>

    <div class="mt-4 px-3">
      <arkime-loading v-if="loading && !error" />

      <arkime-error
        v-if="error"
        :message="error" />

      <div
        v-if="!error"
        class="ms-2 me-2">
        <arkime-table
          id="fieldTable"
          :data="files"
          :load-data="loadData"
          :columns="columns"
          :no-results="true"
          :desc="query.desc"
          :sort-field="query.sortField"
          :action-column="true"
          :no-results-msg="$t(query.cluster ? 'files.noResultsCluster' : 'files.noResults')"
          page="files"
          table-animation="list"
          table-state-name="fieldsCols"
          table-widths-state-name="filesColWidths" />
      </div>
    </div>
  </page-layout> <!-- /files content -->
</template>

<script>
import Utils from '../utils/utils';
import FileService from './FileService';
import ArkimeError from '../utils/Error.vue';
import ArkimeTable from '../utils/Table.vue';
import Clusters from '../utils/Clusters.vue';
import ArkimeLoading from '../utils/Loading.vue';
import ArkimePaging from '@common/Pagination.vue';
import ArkimeCollapsible from '../utils/CollapsibleWrapper.vue';
import PageLayout from '../utils/PageLayout.vue';
import Focus from '@common/Focus.vue';
import { commaString, timezoneDateString } from '@common/vueFilters.js';
import { resolveMessage } from '@common/resolveI18nMessage';

let searchInputTimeout; // timeout to debounce the search input

export default {
  name: 'Files',
  components: {
    ArkimePaging,
    ArkimeError,
    ArkimeLoading,
    ArkimeTable,
    ArkimeCollapsible,
    PageLayout,
    Clusters
  },
  directives: { Focus },
  data: function () {
    return {
      error: '',
      loading: true,
      files: null,
      recordsTotal: undefined,
      recordsFiltered: undefined,
      query: {
        length: parseInt(this.$route.query.length) || 500,
        start: 0,
        filter: '',
        sortField: 'num',
        desc: false,
        cluster: this.$route.query.cluster || undefined
      }
    };
  },
  computed: {
    columns: function() {
      const $t = this.$t.bind(this);
      function intl(opt) {
        opt.name = $t(`files.${opt.id}Name`);
        opt.help = $t(`files.${opt.id}Help`);
        return opt;
      }
      return [
        intl({ id: 'num', classes: 'text-end', sort: 'num', width: 140, default: true }),
        intl({ id: 'node', sort: 'node', width: 120, default: true }),
        intl({ id: 'name', sort: 'name', width: 500, default: true }),
        intl({ id: 'locked', sort: 'locked', dataFunction: (item) => { return item.locked === 1 ? 'True' : 'False'; }, width: 130, default: true }),
        intl({ id: 'first', sort: 'first', dataFunction: (item) => { return timezoneDateString(item.firstTimestamp === undefined ? item.first * 1000 : item.firstTimestamp, this.user.settings.timezone, this.user.settings.ms); }, width: 220, default: true }),
        intl({ id: 'lastTimestamp', sort: 'lastTimestamp', dataFunction: (item) => { return timezoneDateString(item.lastTimestamp, this.user.settings.timezone, this.user.settings.ms); }, width: 220 }),
        intl({ id: 'filesize', sort: 'filesize', classes: 'text-end', width: 140, default: true, dataFunction: (item) => { return this.commaString(item.filesize); } }),
        intl({ id: 'encoding', width: 140 }),
        intl({ id: 'packetPosEncoding', width: 140 }),
        intl({ id: 'packets', sort: 'packets', classes: 'text-end', width: 130 }),
        intl({ id: 'packetsSize', sort: 'packetsSize', classes: 'text-end', width: 150, dataFunction: (item) => { return this.commaString(item.packetsSize); } }),
        intl({ id: 'uncompressedBits', sort: 'uncompressedBits', classes: 'text-end', width: 100 }),
        intl({ id: 'cratio', classes: 'text-end', width: 100, dataFunction: (item) => { return item.cratio + '%'; } }),
        intl({ id: 'compression', width: 100 }),
        intl({ id: 'startTimestamp', sort: 'startTimestamp', dataFunction: (item) => { return timezoneDateString(item.startTimestamp, this.user.settings.timezone, this.user.settings.ms); }, width: 220 }),
        intl({ id: 'finishTimestamp', sort: 'finishTimestamp', dataFunction: (item) => { return timezoneDateString(item.finishTimestamp, this.user.settings.timezone, this.user.settings.ms); }, width: 220 }),
        intl({ id: 'sessionsStarted', sort: 'sessionsStarted', classes: 'text-right', width: 130 }),
        intl({ id: 'sessionsPresent', sort: 'sessionsPresent', classes: 'text-right', width: 130 })
      ];
    },
    user: function () {
      return this.$store.state.user;
    },
    focusInput: {
      get: function () {
        return this.$store.state.focusSearch;
      },
      set: function (newValue) {
        this.$store.commit('setFocusSearch', newValue);
      }
    },
    shiftKeyHold: function () {
      return this.$store.state.shiftKeyHold;
    }
  },
  watch: {
    '$route.query.cluster': {
      handler: function (newVal, oldVal) {
        if (newVal !== oldVal) {
          this.query.cluster = newVal;
          this.loadData();
        }
      }
    }
  },
  methods: {
    commaString,
    timezoneDateString,
    /* exposed page functions ------------------------------------ */
    changePaging (pagingValues) {
      this.query.length = pagingValues.length;
      this.query.start = pagingValues.start;

      this.loadData();
    },
    searchForFiles () {
      if (searchInputTimeout) { clearTimeout(searchInputTimeout); }
      // debounce the input so it only issues a request after keyups cease for 400ms
      searchInputTimeout = setTimeout(() => {
        searchInputTimeout = null;
        this.loadData();
      }, 400);
    },
    clear () {
      this.query.filter = undefined;
      this.loadData();
    },
    onOffFocus: function () {
      this.focusInput = false;
    },
    /* helper functions ---------------------------------------------------- */
    loadData: function (sortField, desc) {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      this.loading = true;

      if (desc !== undefined) { this.query.desc = desc; }
      if (sortField) { this.query.sortField = sortField; }

      FileService.get(this.query).then((response) => {
        this.error = '';
        this.loading = false;
        this.files = response.data;
        this.recordsTotal = response.recordsTotal;
        this.recordsFiltered = response.recordsFiltered;
      }).catch((error) => {
        this.loading = false;
        this.error = resolveMessage(error, this.$t);
      });
    },
    onError: function (message) {
      this.childError = message;
    }
  }
};
</script>

<style>
.files-search ul.pagination {
  margin-bottom: 0;
}
</style>

<style scoped>
/* search navbar */
.files-search {
  z-index: 5;
  border: none;
  background-color: rgb(var(--v-theme-secondary-lightest));

  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}
</style>
