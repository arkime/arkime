<template>

  <div>

    <div class="files-search">
      <div class="mr-1 ml-1 mt-1 mb-1">
        <div class="input-group input-group-sm pull-right" style="max-width:50%;">
          <div class="input-group-prepend">
            <span class="input-group-text">
              <span class="fa fa-search"></span>
            </span>
          </div>
          <input type="text"
            class="form-control"
            v-model="query.filter"
            @keyup="searchForFiles()"
            placeholder="Begin typing to search for files by name"
          />
        </div>
        <moloch-paging v-if="files"
          :records-total="files.recordsTotal"
          :records-filtered="files.recordsFiltered"
          v-on:changePaging="changePaging">
        </moloch-paging>
      </div>
    </div>

    <div class="files-content">

      <moloch-loading v-if="loading && !error">
      </moloch-loading>

      <moloch-error v-if="error"
        :message="error">
      </moloch-error>

      <div v-show="!error">
        <table class="table table-sm table-striped">
          <thead>
            <tr>
              <th v-for="column of columns"
                :key="column.name"
                class="cursor-pointer"
                @click="columnClick(column.sort)">
                {{ column.name }}
                <span v-if="column.sort !== undefined">
                  <span v-show="query.sortField === column.sort && !query.desc" class="fa fa-sort-asc"></span>
                  <span v-show="query.sortField === column.sort && query.desc" class="fa fa-sort-desc"></span>
                  <span v-show="query.sortField !== column.sort" class="fa fa-sort"></span>
                </span>
              </th>
            </tr>
          </thead>
          <tbody v-if="files">
            <template v-for="file of files.data">
              <tr :key="file.id">
                <td class="no-wrap">
                  {{ file.num }}
                </td>
                <td class="no-wrap">
                  {{ file.node }}
                </td>
                <td class="no-wrap">
                  {{ file.name }}
                </td>
                <td class="no-wrap">
                  {{ file.locked === 1 ? 'True' : 'False' }}
                </td>
                <td class="no-wrap">
                  <span v-if="settings">
                    {{ file.first | timezoneDateString(settings.timezone, 'YYYY/MM/DD HH:mm:ss z') }}
                  </span>
                </td>
                <td class="no-wrap">
                  {{ file.filesize }}
                </td>
              </tr>
            </template>
            <tr v-if="!files.data.length">
              <td colspan="6"
                class="text-danger text-center">
                <span class="fa fa-warning">
                </span>&nbsp;
                No results match your search
              </td>
            </tr>
          </tbody>
        </table>
      </div>

    </div>

  </div> <!-- /files content -->

</template>

<script>
import UserService from '../users/UserService';
import MolochPaging from '../utils/Pagination';
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';

let searchInputTimeout; // timeout to debounce the search input

export default {
  name: 'Files',
  components: { MolochPaging, MolochError, MolochLoading },
  data: function () {
    return {
      error: '',
      loading: true,
      settings: null,
      files: null,
      query: {
        length: parseInt(this.$route.query.length) || 50,
        start: 0,
        filter: null,
        sortField: 'num',
        desc: false
      },
      columns: [ // node stats table columns
        { name: 'File Number', sort: 'num' },
        { name: 'Node', sort: 'node' },
        { name: 'Name', sort: 'name' },
        { name: 'Locked', sort: 'locked' },
        { name: 'First Date', sort: 'first' },
        { name: 'File Size', sort: 'filesize' }
      ]
    };
  },
  created: function () {
    this.loadUser();
    this.loadData();
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
    columnClick (name) {
      this.query.sortField = name;
      this.query.desc = !this.query.desc;
      this.loadData();
    },
    loadUser: function () {
      UserService.getCurrent()
        .then((response) => {
          this.settings = response.settings;
        }, (error) => {
          this.settings = { timezone: 'local' };
        });
    },
    loadData: function () {
      this.$http.get('file/list', { params: this.query })
        .then((response) => {
          this.error = '';
          this.loading = false;
          this.files = response.data;
        }, (error) => {
          this.loading = false;
          this.error = error;
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
