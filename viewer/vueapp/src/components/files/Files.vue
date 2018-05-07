<template>

  <div class="files-content">
    <div class="sub-navbar">
      <span class="sub-navbar-title">
        <span class="fa-stack">
          <span class="fa fa-files-o fa-stack-1x"></span>
          <span class="fa fa-square-o fa-stack-2x"></span>
        </span>&nbsp;
        Files
      </span>
    </div>

    <moloch-loading v-if="loading && !error">
    </moloch-loading>

    <moloch-error v-if="error"
      :message="error">
    </moloch-error>

    <div v-show="!error">

      <div class="input-group input-group-sm node-search pull-right mt-1">
        <div class="input-group-prepend">
          <span class="input-group-text">
            <span class="fa fa-search"></span>
          </span>
        </div>
        <input type="text"
          class="form-control"
          v-model="query.filter"
          @keyup="searchForFiles()"
          placeholder="Begin typing to search for files by name">
      </div>

      <moloch-paging v-if="files"
        class="mt-1"
        :records-total="files.recordsTotal"
        :records-filtered="files.recordsFiltered"
        v-on:changePaging="changePaging">
      </moloch-paging>

      <table class="table table-sm text-right small">
        <thead>
          <tr>
            <th v-for="column of columns"
              :key="column.name"
              class="cursor-pointer"
              :class="{'text-left':!column.doStats}"
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
              <td class="no-wrap">{{file.num}}</td>
              <td class="no-wrap">{{file.node}}</td>
              <td class="no-wrap">{{file.name}}</td>
              <td class="no-wrap">{{file.locked === 1 ? 'True' : 'False'}}</td>
              <td class="no-wrap"><span ng-if="::$ctrl.settings">{{file.first | timezoneDateString(user.settings.timezone, 'YYYY/MM/DD HH:mm:ss z') }}</span></td>
              <td class="no-wrap">{{file.filesize}}</td>
            </tr>
          </template>
        </tbody>
      </table>
    </div>
  </div> <!-- /files content -->

</template>

<script>
import UserService from '../UserService';
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
      user: null,
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
          this.user = response;
        }, (error) => {
          this.user = { settings: { timezone: 'local' } };
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
<style scoped>
.node-search {
  max-width: 50%;
}
</style>
