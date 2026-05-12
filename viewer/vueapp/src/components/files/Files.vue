<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <ArkimeCollapsible>
      <span class="fixed-header">
        <div class="files-search p-1">
          <v-row
            dense
            justify="start">
            <v-col cols="auto">
              <arkime-paging
                v-if="files"
                :records-total="recordsTotal"
                :records-filtered="recordsFiltered"
                @change-paging="changePaging"
                :length-default="500" />
            </v-col>
            <v-col cols="auto">
              <div class="arkime-input-group">
                <span class="arkime-input-label arkime-input-label-fw">
                  <span
                    v-if="!shiftKeyHold"
                    class="fa fa-search fa-fw" />
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
                  variant="outlined"
                  size="small"
                  density="comfortable"
                  icon
                  class="arkime-input-append-btn"
                  :disabled="!query.filter"
                  :aria-label="$t('common.clear')"
                  @click="clear">
                  <span class="fa fa-close" />
                </v-btn>
              </div>
            </v-col>
            <v-col cols="auto">
              <Clusters />
            </v-col>
          </v-row>
        </div>
      </span>
    </ArkimeCollapsible>

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
  </div> <!-- /files content -->
</template>

<script>
import Utils from '../utils/utils';
import FileService from './FileService';
import ArkimeError from '../utils/Error.vue';
import ArkimeTable from '../utils/Table.vue';
import Clusters from '../utils/Clusters.vue';
import ArkimeLoading from '../utils/Loading.vue';
import ArkimePaging from '../utils/Pagination.vue';
import ArkimeCollapsible from '../utils/CollapsibleWrapper.vue';
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
        intl({ id: 'locked', sort: 'locked', dataFunction: (item) => { return item.locked === 1 ? 'True' : 'False'; }, width: 100, default: true }),
        intl({ id: 'first', sort: 'first', dataFunction: (item) => { return timezoneDateString(item.firstTimestamp === undefined ? item.first * 1000 : item.firstTimestamp, this.user.settings.timezone, this.user.settings.ms); }, width: 220, default: true }),
        intl({ id: 'lastTimestamp', sort: 'lastTimestamp', dataFunction: (item) => { return timezoneDateString(item.lastTimestamp, this.user.settings.timezone, this.user.settings.ms); }, width: 220 }),
        intl({ id: 'filesize', sort: 'filesize', classes: 'text-end', width: 100, default: true, dataFunction: (item) => { return this.commaString(item.filesize); } }),
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
  background-color: var(--color-secondary-lightest);

  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

/* Input-group bridge -- same shape as Stats.vue/ExpressionTypeahead.vue.
   Phase D candidate for promotion to a shared overrides.css block. */
.arkime-input-group {
  display: inline-flex;
  align-items: stretch;
  width: auto;
  flex-wrap: nowrap;
}
.arkime-input-label {
  display: inline-flex;
  align-items: center;
  padding: 2px 8px;
  background-color: color-mix(in srgb, var(--color-foreground, #000) 8%, var(--color-background, #fff));
  border: 1px solid var(--color-gray);
  border-right: none;
  border-radius: 4px 0 0 4px;
  font-size: 0.85rem;
  color: var(--color-foreground);
  white-space: nowrap;
}
.arkime-input-label-fw {
  width: 36px;
  justify-content: center;
}
.arkime-input-control {
  flex: 1 1 auto;
  min-width: 0;
  padding: 2px 8px;
  background-color: var(--color-background, #fff);
  color: var(--color-foreground, #495057);
  border: 1px solid var(--color-gray);
  font-size: 0.85rem;
  line-height: 1.5;
}
.arkime-input-control:focus {
  outline: none;
  border-color: var(--color-primary, #0d6efd);
}
.arkime-input-group :deep(.arkime-input-append-btn.v-btn) {
  border-radius: 0;
  border-left: none;
  min-width: 0;
}
.arkime-input-group > :last-child {
  border-top-right-radius: 4px;
  border-bottom-right-radius: 4px;
}
</style>
