<template>

  <div class="history-page">

    <!-- search navbar -->
    <form class="history-search">
      <div class="mr-1 ml-1 mt-1 mb-1">
        <span class="pull-right fa fa-lg fa-question-circle text-theme-primary help-cursor mt-2"
          title="Tip: use ? to replace a single character and * to replace zero or more characters in your query"
          v-b-tooltip.hover>
        </span>
        <div class="input-group input-group-sm input-search">
          <div class="input-group-prepend">
            <span class="input-group-text">
              <span class="fa fa-search">
              </span>
            </span>
          </div>
          <input type="text"
            class="form-control"
            v-model="query.searchTerm"
            @keyup="searchForHistory()"
            placeholder="Search for history in the table below"
          />
        </div>
        <div class="form-inline mt-1">
          <moloch-time :timezone="settings.timezone"
            @timeChange="loadData"
            :hide-bounding="true"
            :hide-interval="true">
          </moloch-time>
        </div>
      </div>
    </form> <!-- /search navbar -->

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

    <table class="table table-sm small">
      <thead>
        <tr>
          <th width="90px;">
            <button type="button"
              class="btn btn-xs btn-primary margined-bottom-sm"
              v-b-tooltip.hover
              title="Toggle column filters"
              @click="showColFilters = !showColFilters">
              <span class="fa fa-filter"></span>
            </button>
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
        <template v-for="(item, index) of history.data">
          <tr :key="item.id">
            <td class="nowrap">
              <!-- TODO toggle btn -->
              <button type="button"
                class="btn btn-xs btn-toggle"
                :class="{'collapsed':!item.expanded, 'btn-theme-tertiary':!item.expanded, 'btn-danger':item.expanded}"
                @click="toggleLogDetail(item)">
                <span class="fa fa-close">
                </span>
              </button>
              <button type="button"
                class="btn btn-xs btn-warning"
                v-has-permission="'createEnabled,removeEnabled'"
                @click="deleteLog(item, index)">
                <span class="fa fa-trash-o">
                </span>
              </button>
              <button type="button"
                class="btn btn-xs btn-info"
                v-if="item.uiPage"
                tooltip-placement="right"
                v-b-tooltip.hover :title="`Open this query on the ${item.uiPage} page`"
                :href="`${item.uiPage}?${item.query}`">
                <span class="fa fa-folder-open">
                </span>
              </button>
            </td>
            <td class="no-wrap">
              {{ item.timestamp | timezoneDateString(settings.timezone, 'YYYY/MM/DD HH:mm:ss z') }}
            </td>
            <td class="no-wrap">
              {{ item.range*1000 | readableTime }}
            </td>
            <td v-has-permission="'createEnabled'"
              class="no-wrap">
              {{ item.userId }}
            </td>
            <td class="no-wrap">
              {{ item.queryTime }}ms
            </td>
            <td class="no-wrap">
              {{ item.api }}
            </td>
            <td class="no-wrap">
              {{ item.expression }}
            </td>
            <td class="no-wrap">
              <span v-if="item.view">
                <strong>
                  {{ item.view.name }}
                </strong>
                {{ item.view.expression }}
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
import MolochTime from '../search/Time';

let searchInputTimeout; // timeout to debounce the search input

export default {
  name: 'History',
  components: { MolochPaging, MolochError, MolochLoading, MolochTime },
  data: function () {
    return {
      error: '',
      loading: true,
      user: null,
      history: null,
      showColFilters: false,
      colSpan: 7,
      filters: {},
      settings: { timezone: 'local' },
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
  computed: {
    query: function () {
      return { // query defaults
        length: parseInt(this.$route.query.length) || 50,
        start: 0,
        searchTerm: null,
        sortField: 'timestamp',
        desc: false,
        date: this.$store.state.timeRange,
        startTime: this.$store.state.time.startTime,
        stopTime: this.$store.state.time.stopTime
      };
    }
  },
  created: function () {
    this.loadUser();
  },
  methods: {
    /* exposed page functions ------------------------------------ */
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
    toggleLogDetail: function (item) {
      item.expanded = !item.expanded;

      /* TODO
      if (log.query) {
        log.queryObj = this.$filter('parseParamString')(log.query);
      }
      */
    },
    deleteLog: function (log, index) {
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
    /* helper functions ------------------------------------------ */
    loadUser: function () {
      UserService.getCurrent()
        .then((response) => {
          this.settings = response.settings;
          if (response.createEnabled) { this.colSpan = 8; }
          this.filters.userId = this.$route.query.userId || response.userId;
          this.loadData();
        }, (error) => {
          this.loadData();
        });
    },
    loadData: function () {
      let exists = [];
      for (let i = 0, len = this.columns.length; i < len; ++i) {
        let col = this.columns[i];
        if (col.exists) { exists.push(col.sort); }
      }
      if (exists.length) {
        this.query.exists = exists.join();
      }

      if (this.filters && Object.keys(this.filters).length) {
        for (let key in this.filters) {
          if (this.filters.hasOwnProperty(key)) {
            this.query[key] = this.filters[key];
          }
        }
      }

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
    /* event functions ------------------------------------------- */
    changePaging (pagingValues) {
      this.query.length = pagingValues.length;
      this.query.start = pagingValues.start;

      this.loadData();
    }
  }
};
</script>

<style scoped>
/* navbar styles ------------------- */
.history-page form.history-search {
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

.history-page form .time-range-control {
  -webkit-appearance: none;
}

.history-page form .input-search {
  width: 97%
}

/* navbar with pagination */
.history-page form.history-paging {
  z-index: 4;
  position: fixed;
  top: 110px;
  left: 0;
  right: 0;
  height: 40px;
  background-color: var(--color-quaternary-lightest);

  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

/* table styles */
.history-page table {
  margin-top: 160px;
}
.history-page table .nowrap {
  white-space: nowrap;
}
</style>
