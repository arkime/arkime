<template>

  <div>
    <MolochCollapsible>
      <span class="fixed-header">
        <div class="files-search">
          <div class="p-1">
            <div class="input-group input-group-sm pull-right" style="max-width:50%;">
              <div class="input-group-prepend">
                <span class="input-group-text input-group-text-fw">
                  <span v-if="!shiftKeyHold"
                    class="fa fa-search fa-fw">
                  </span>
                  <span v-else
                    class="query-shortcut">
                    Q
                  </span>
                </span>
              </div>
              <input type="text"
                class="form-control"
                v-model="query.filter"
                v-focus="focusInput"
                @blur="onOffFocus"
                @input="searchForFiles"
                @keydown.enter="searchForFiles"
                placeholder="Begin typing to search for files by name"
              />
              <span class="input-group-append">
                <button type="button"
                  @click="clear"
                  :disabled="!query.filter"
                  class="btn btn-outline-secondary btn-clear-input">
                  <span class="fa fa-close">
                  </span>
                </button>
              </span>
            </div>
            <moloch-paging v-if="files"
              :records-total="recordsTotal"
              :records-filtered="recordsFiltered"
              v-on:changePaging="changePaging"
              length-default=500 >
            </moloch-paging>
          </div>
        </div>
      </span>
    </MolochCollapsible>

    <div class="files-content container-fluid">

      <moloch-loading v-if="loading && !error">
      </moloch-loading>

      <moloch-error v-if="error"
        :message="error">
      </moloch-error>

      <div v-if="!error"
        class="ml-2 mr-2">
        <moloch-table
          id="fieldTable"
          :data="files"
          :load-data="loadData"
          :columns="columns"
          :no-results="true"
          :desc="query.desc"
          :sort-field="query.sortField"
          :action-column="true"
          page="files"
          table-animation="list"
          table-classes="table-sm"
          table-state-name="fieldsCols"
          table-widths-state-name="filesColWidths">
        </moloch-table>
      </div>

    </div>

  </div> <!-- /files content -->

</template>

<script>
import MolochPaging from '../utils/Pagination';
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';
import MolochTable from '../utils/Table';
import MolochCollapsible from '../utils/CollapsibleWrapper';
import FileService from './FileService';
import Focus from '../../../../../common/vueapp/Focus';

let searchInputTimeout; // timeout to debounce the search input

export default {
  name: 'Files',
  components: {
    MolochPaging,
    MolochError,
    MolochLoading,
    MolochTable,
    MolochCollapsible
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
        filter: null,
        sortField: 'num',
        desc: false
      },
      columns: [ // node stats table columns
        { id: 'num', name: 'File #', classes: 'text-right', sort: 'num', help: 'Internal file number, unique per node', width: 140, default: true },
        { id: 'node', name: 'Node', sort: 'node', help: 'What Arkime capture node this file lives on', width: 120, default: true },
        { id: 'name', name: 'Name', sort: 'name', help: 'The complete file path', width: 500, default: true },
        { id: 'locked', name: 'Locked', sort: 'locked', dataFunction: (item) => { return item.locked === 1 ? 'True' : 'False'; }, help: 'If locked Arkime viewer won\'t delete this file to free space', width: 100, default: true },
        { id: 'first', name: 'First Date', sort: 'first', dataFunction: (item) => { return this.$options.filters.timezoneDateString(item.first * 1000, this.user.settings.timezone); }, help: 'Timestamp of the first packet in the file', width: 220, default: true },
        { id: 'filesize', name: 'File Size', sort: 'filesize', classes: 'text-right', help: 'Size of the file in bytes, blank if the file is still being written to', width: 100, default: true, dataFunction: (item) => { return this.$options.filters.commaString(item.filesize); } },
        { id: 'encoding', name: 'Encoding', help: 'How the packets are encoded/encrypted', width: 140 },
        { id: 'packetPosEncoding', name: 'Packet Pos Encoding', help: 'How the packet position is encoded', width: 140 },
        { id: 'packets', sort: 'packets', name: 'Packets', classes: 'text-right', help: 'Number of packets in file', width: 130 },
        { id: 'packetsSize', sort: 'packetsSize', name: 'Packets Bytes', classes: 'text-right', help: 'Size of packets before compression', width: 150, dataFunction: (item) => { return this.$options.filters.commaString(item.packetsSize); } },
        { id: 'uncompressedBits', sort: 'uncompressedBits', name: 'UC Bits', classes: 'text-right', help: 'Number of bits used to store uncompressed position', width: 100 },
        { id: 'cratio', name: 'C Ratio', classes: 'text-right', help: '1 - compressed/uncompressed in bytes', width: 100, dataFunction: (item) => { return item.cratio + '%'; } },
        { id: 'compression', name: 'Compression', help: 'Compression Algorithm', width: 100 }
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
  methods: {
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
      this.loading = true;

      if (desc !== undefined) { this.query.desc = desc; }
      if (sortField) { this.query.sortField = sortField; }

      FileService.get(this.query).then((response) => {
        this.error = '';
        this.loading = false;
        this.files = response.data.data;
        this.recordsTotal = response.data.recordsTotal;
        this.recordsFiltered = response.data.recordsFiltered;
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

/* page content */
.files-content {
  margin-top: 10px;
}
</style>
