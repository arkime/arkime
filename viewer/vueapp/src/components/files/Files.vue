<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>

  <div>
    <ArkimeCollapsible>
      <span class="fixed-header">
        <div class="files-search p-1">
          <BRow gutter-x="1" align-h="start">
            <BCol cols="auto">
              <arkime-paging v-if="files"
                :records-total="recordsTotal"
                :records-filtered="recordsFiltered"
                v-on:changePaging="changePaging"
                :length-default=500 >
              </arkime-paging>
            </BCol>
            <BCol cols="auto">
              <BInputGroup size="sm">
                <BInputGroupText class="input-group-text-fw">
                  <span v-if="!shiftKeyHold"
                    class="fa fa-search fa-fw">
                  </span>
                  <span v-else
                    class="query-shortcut">
                    Q
                  </span>
                </BInputGroupText>
                <input type="text"
                  class="form-control"
                  v-model="query.filter"
                  v-focus="focusInput"
                  @blur="onOffFocus"
                  @input="searchForFiles"
                  @keydown.enter="searchForFiles"
                  placeholder="Begin typing to search for files by name"
                />
                <BButton
                  variant="outline-secondary"
                  @click="clear"
                  :disabled="!query.filter"
                  class="btn-clear-input">
                  <span class="fa fa-close"></span>
                </BButton>
              </BInputGroup>
            </BCol>
            <BCol cols="auto">
              <Clusters />
            </BCol>
          </BRow>
        </div>
      </span>
    </ArkimeCollapsible>

    <div class="mt-4 container-fluid">

      <arkime-loading v-if="loading && !error">
      </arkime-loading>

      <arkime-error v-if="error"
        :message="error">
      </arkime-error>

      <div v-if="!error"
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
          :no-results-msg="`No results match your search.${query.cluster ? 'Try selecting a different cluster.' : ''}`"
          page="files"
          table-animation="list"
          table-state-name="fieldsCols"
          table-widths-state-name="filesColWidths">
        </arkime-table>
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
import { commaString, timezoneDateString } from '@real_common/vueFilters.js';

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
      },
      columns: [ // node stats table columns
        { id: 'num', name: 'File #', classes: 'text-end', sort: 'num', help: 'Internal file number, unique per node', width: 140, default: true },
        { id: 'node', name: 'Node', sort: 'node', help: 'What Arkime capture node this file lives on', width: 120, default: true },
        { id: 'name', name: 'Name', sort: 'name', help: 'The complete file path', width: 500, default: true },
        { id: 'locked', name: 'Locked', sort: 'locked', dataFunction: (item) => { return item.locked === 1 ? 'True' : 'False'; }, help: 'If locked Arkime viewer won\'t delete this file to free space', width: 100, default: true },
        { id: 'first', name: 'First Date', sort: 'first', dataFunction: (item) => { return timezoneDateString(item.firstTimestamp === undefined ? item.first * 1000 : item.firstTimestamp, this.user.settings.timezone, this.user.settings.ms); }, help: 'Timestamp of the first packet in the file', width: 220, default: true },
        { id: 'lastTimestamp', name: 'Last Date', sort: 'lastTimestamp', dataFunction: (item) => { return timezoneDateString(item.lastTimestamp, this.user.settings.timezone, this.user.settings.ms); }, help: 'Last Packet Timestamp', width: 220 },
        { id: 'filesize', name: 'File Size', sort: 'filesize', classes: 'text-end', help: 'Size of the file in bytes, blank if the file is still being written to', width: 100, default: true, dataFunction: (item) => { return this.commaString(item.filesize); } },
        { id: 'encoding', name: 'Encoding', help: 'How the packets are encoded/encrypted', width: 140 },
        { id: 'packetPosEncoding', name: 'Packet Pos Encoding', help: 'How the packet position is encoded', width: 140 },
        { id: 'packets', sort: 'packets', name: 'Packets', classes: 'text-end', help: 'Number of packets in file', width: 130 },
        { id: 'packetsSize', sort: 'packetsSize', name: 'Packets Bytes', classes: 'text-end', help: 'Size of packets before compression', width: 150, dataFunction: (item) => { return this.commaString(item.packetsSize); } },
        { id: 'uncompressedBits', sort: 'uncompressedBits', name: 'UC Bits', classes: 'text-end', help: 'Number of bits used to store uncompressed position', width: 100 },
        { id: 'cratio', name: 'C Ratio', classes: 'text-end', help: '1 - compressed/uncompressed in bytes', width: 100, dataFunction: (item) => { return item.cratio + '%'; } },
        { id: 'compression', name: 'Compression', help: 'Compression Algorithm', width: 100 },
        { id: 'startTimestamp', name: 'Start Date', sort: 'startTimestamp', dataFunction: (item) => { return timezoneDateString(item.startTimestamp, this.user.settings.timezone, this.user.settings.ms); }, help: 'Start Processing Timestamp', width: 220 },
        { id: 'finishTimestamp', name: 'Finish Date', sort: 'finishTimestamp', dataFunction: (item) => { return timezoneDateString(item.finishTimestamp, this.user.settings.timezone, this.user.settings.ms); }, help: 'Finish Processing Timestamp', width: 220 },
        { id: 'sessionsStarted', sort: 'sessionsStarted', name: 'Sessions Started', classes: 'text-right', help: 'Sessions started in file', width: 130 },
        { id: 'sessionsPresent', sort: 'sessionsPresent', name: 'Sessions Present', classes: 'text-right', help: 'Sessions present in file but started in previous file', width: 130 }
      ]
    };
  },
  computed: {
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
        this.error = error.text || error;
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
</style>
