<template>
  <div class="history-page">
    <div class="input-group input-group-sm mt-1">
      <div class="input-group-prepend">
        <span class="input-group-text">
          <span class="fa fa-search"></span>
        </span>
      </div>
      <input type="text"
        class="form-control"
        v-model="query.filter"
        @keyup="searchForHistory()"
        placeholder="Search for history in the table below">
    </div>

    <!-- paging navbar -->
    <form class="history-paging">
      <div class="form-inline">
        <moloch-paging v-if="history"
          class="mt-1 ml-1"
          :records-total="history.recordsTotal"
          :records-filtered="history.recordsFiltered"
          @changePaging="changePaging">
        </moloch-paging>
      </div>
    </form> <!-- /paging navbar -->

    <table class="table table-sm text-right small">
      <thead>
        <tr>
          <th width="90px;">
            <a class="btn btn-xs btn-primary margined-bottom-sm"
              v-b-tooltip.hover title="Toggle column filters" tooltip-placement="bottom-left"
              @click="showColFilters = !showColFilters">
              <span class="fa fa-filter"></span>
            </a>
          </th>
          <th v-for="column of columns"
            :key="column.name"
            class="cursor-pointer"
            :style="{'width': `${column.width}%`}"
            @click="columnClick(column.sort)">

            <input type="text"
              v-if="column.filter && showColFilters"
              v-has-permission=column.permission
              v-model="query[column.sort]"
              :placeholder="`Filter by ${column.name}`"
              class="form-control input-sm input-filter"
              @change="loadData()">

            {{ column.name }}
            <span v-if="column.sort !== undefined">
              <span v-show="query.sortField === column.sort && !query.desc" class="fa fa-sort-asc"></span>
              <span v-show="query.sortField === column.sort && query.desc" class="fa fa-sort-desc"></span>
              <span v-show="query.sortField !== column.sort" class="fa fa-sort"></span>
            </span>
          </th>
        </tr>
      </thead>
      <tbody v-if="history">
        <!-- TODO: debounce:500 input fields -->
        <template v-for="item of history.data">
          <tr :key="item.id">
            <td>
              <a class="btn btn-xs btn-toggle"
                :class="{'collapsed':!item.expanded, 'btn-theme-tertiary':!item.expanded, 'btn-danger':item.expanded}"
                @click="toggleLogDetail(item)">
                <span class="fa fa-close"></span>
              </a>
              <a class="btn btn-xs btn-warning"
                v-has-permission="'createEnabled,removeEnabled'"
                @click="deleteLog(item, $index)">
                <span class="fa fa-trash-o"></span>
              </a>
              <a class="btn btn-xs btn-info"
                v-if="item.uiPage"
                tooltip-placement="right"
                v-b-tooltip.hover :title="`Open this query on the ${item.uiPage} page`"
                :href="`${item.uiPage}?${item.query}`">
                <span class="fa fa-folder-open"></span>
              </a>
            </td>
            <td class="no-wrap">{{item.timestamp | timezoneDateString(user.settings.timezone, 'YYYY/MM/DD HH:mm:ss z') }}</td>
            <td class="no-wrap">{{item.range*1000 | readableTime}}</td>
            <td v-has-permission="'createEnabled'" class="no-wrap">{{item.userId}}</td>
            <td class="no-wrap">{{item.queryTime}}ms</td>
            <td class="no-wrap">{{item.api}}</td>
            <td class="no-wrap">{{item.expression}}</td>
            <td class="no-wrap">
              <span v-if=item.view>
                <strong>{{item.view.name}}</strong> {{item.view.expression}}
              </span>
            </td>
          </tr>
        </template>
      </tbody>
    </table>
  </div>
</template>
<script>
import UserService from '../UserService';
import MolochPaging from '../utils/Pagination';
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';

let searchInputTimeout; // timeout to debounce the search input

export default {
  name: 'History',
  components: {MolochPaging, MolochError, MolochLoading},
  data: function () {
    return {
      error: '',
      loading: true,
      user: null,
      history: null,
      showColFilters: false,
      colSpan: 7,
      query: {
        length: parseInt(this.$route.query.length) || 50,
        start: 0,
        filter: null,
        sortField: 'timestamp',
        desc: false
      },
      columns: [
        { name: 'Time', sort: 'timestamp', nowrap: true, width: 10, help: 'The time of the request' },
        { name: 'Time Range', sort: 'range', nowrap: true, width: 8, help: 'The time range of the request' },
        { name: 'User ID', sort: 'userId', nowrap: true, width: 11, filter: true, permission: 'createEnabled', help: 'The id of the user that initiated the request' },
        { name: 'Query Time', sort: 'queryTime', nowrap: true, width: 8, help: 'Execution time in MS' },
        { name: 'API', sort: 'api', nowrap: true, width: 13, filter: true, help: 'The API endpoint of the request' },
        { name: 'Expression', sort: 'expression', nowrap: true, width: 27, exists: false, help: 'The query expression issued with the request' },
        { name: 'View', sort: 'view.name', nowrap: true, width: 23, exists: false, help: 'The view expression applied to the request' }
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
    searchForHistory () {
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
      this.$http.get('history/list', { params: this.query })
        .then((response) => {
          this.error = '';
          this.loading = false;
          this.history = response.data;
        }, (error) => {
          this.loading = false;
          this.error = error;
        });
    },
    toggleLogDetail(item) {
      item.expanded = !item.expanded;

      /* TODO
      if (log.query) {
        log.queryObj = this.$filter('parseParamString')(log.query);
      }
      */
    },
    deleteLog(item, index) {
      this.HistoryService.delete(log.id, log.index)
        .then((response) => {
          this.msg = response.text || 'Successfully deleted history item';
          this.msgType = 'success';
          this.history.data.splice(index, 1);
        })
        .catch((error) => {
          this.msg = error.data.text || 'Error deleting history item';
          this.msgType = 'danger';
        });
    },
    onError: function (message) {
      this.childError = message;
    }
  }
};
</script>
<style scoped>
</style>
