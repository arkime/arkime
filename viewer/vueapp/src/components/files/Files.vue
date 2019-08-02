<template>

  <div>

    <div class="files-search">
      <div class="mr-1 ml-1 mt-1 mb-1">
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
            v-focus-input="focusInput"
            @blur="onOffFocus"
            @input="searchForFiles"
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
          :loadData="loadData"
          :columns="columns"
          :no-results="true"
          :desc="query.desc"
          :sortField="query.sortField"
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
import FocusInput from '../utils/FocusInput';

let searchInputTimeout; // timeout to debounce the search input

export default {
  name: 'Files',
  components: {
    MolochPaging,
    MolochError,
    MolochLoading,
    MolochTable
  },
  directives: { FocusInput },
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
        { id: 'num', name: 'File #', sort: 'num', help: 'Internal file number, unique per node', width: 140, default: true },
        { id: 'node', name: 'Node', sort: 'node', help: 'What moloch capture node this file lives on', width: 120, default: true },
        { id: 'name', name: 'Name', sort: 'name', help: 'The complete file path', width: 500, default: true },
        { id: 'locked', name: 'Locked', sort: 'locked', dataFunction: (item) => { return item.locked === 1 ? 'True' : 'False'; }, help: 'If locked Moloch viewer won\'t delete this file to free space', width: 100, default: true },
        { id: 'first', name: 'First Date', sort: 'first', dataFunction: (item) => { return this.$options.filters.timezoneDateString(item.first, this.user.settings.timezone, this.user.settings.ms); }, help: 'Timestamp of the first packet in the file', width: 220, default: true },
        { id: 'filesize', name: 'File Size', sort: 'filesize', classes: 'text-right', help: 'Size of the file in bytes, blank if the file is still being written to', width: 100, default: true, dataFunction: (item) => { return this.$options.filters.commaString(item.filesize); } }
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

      this.$http.get('file/list', { params: this.query })
        .then((response) => {
          this.error = '';
          this.loading = false;
          this.files = response.data.data;
          this.recordsTotal = response.data.recordsTotal;
          this.recordsFiltered = response.data.recordsFiltered;
        }, (error) => {
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
  position: fixed;
  right: 0;
  left: 0;
  top: 36px;
  border: none;
  background-color: var(--color-secondary-lightest);

  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

/* page content */
.files-content {
  margin-top: 90px;
}
</style>
