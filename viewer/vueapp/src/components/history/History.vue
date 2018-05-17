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
            @keyup="debounceSearch()"
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

    <table class="table table-sm table-striped small">
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
            v-has-permission="column.permission"
            :style="{'width': `${column.width}%`}">
            <input type="text"
              v-if="column.filter && showColFilters"
              v-has-permission="column.permission"
              v-model="filters[column.sort]"
              :placeholder="`Filter by ${column.name}`"
              class="form-control form-control-sm input-filter"
              @keyup="debounceSearch()"
              @click.stop
            />
            <div v-if="column.hasOwnProperty('exists')"
              class="mr-1 header-div">
              <input type="checkbox"
                class="checkbox"
                v-b-tooltip.hover
                :title="column.name + ' EXISTS!'"
                @change="loadData"
                v-model="column.exists"
              />
            </div>
            <div class="header-div"
              @click="columnClick(column.sort)">
              <span v-if="column.sort !== undefined">
                <span v-show="query.sortField === column.sort && !query.desc" class="fa fa-sort-asc"></span>
                <span v-show="query.sortField === column.sort && query.desc" class="fa fa-sort-desc"></span>
                <span v-show="query.sortField !== column.sort" class="fa fa-sort"></span>
              </span>
              {{ column.name }}
            </div>
            <a v-if="filters[column.sort]"
              v-b-tooltip.hover
              :title="'The history is being filtered by ' + column.name + '. Click to display the filter.'"
              @click="showColFilters = true"
              class="cursor-pointer ml-1">
              <span class="fa fa-filter">
              </span>
            </a>
          </th>
        </tr>
      </thead>
      <tbody>
        <!-- no results -->
        <tr v-if="!history.data.length">
          <td :colspan="colSpan"
            class="text-danger text-center">
            <span class="fa fa-warning">
            </span>&nbsp;
            <strong>
              No history or none that matches your search
            </strong>
          </td>
        </tr> <!-- /no results -->
        <template v-for="(item, index) of history.data">
          <!-- history item -->
          <tr :key="item.id">
            <td class="nowrap">
              <toggle-btn class="mt-1"
                :opened="item.expanded"
                @toggle="toggleLogDetail(item)">
              </toggle-btn>
              <button type="button"
                class="btn btn-xs btn-warning"
                v-has-permission="'createEnabled,removeEnabled'"
                @click="deleteLog(item, index)">
                <span class="fa fa-trash-o">
                </span>
              </button>
              <a class="btn btn-xs btn-info"
                v-if="item.uiPage"
                tooltip-placement="right"
                v-b-tooltip.hover
                :title="`Open this query on the ${item.uiPage} page`"
                :href="`${item.uiPage}?${item.query}`">
                <span class="fa fa-folder-open">
                </span>
              </a>
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
          </tr> <!-- /history item -->
          <!-- history item info -->
          <tr :key="item.id+'-detail'"
            v-if="expandedLogs[item.id]">
            <td :colspan="colSpan">
              <dl class="dl-horizontal">
                <!-- count info -->
                <div v-if="item.recordsReturned !== undefined"
                  class="mt-1">
                  <dt><h5>
                    Counts
                  </h5></dt>
                  <dd><h5>&nbsp;</h5></dd>
                  <span v-if="item.recordsReturned !== undefined">
                    <dt>Records Returned</dt>
                    <dd>{{ item.recordsReturned }}</dd>
                  </span>
                  <span v-if="item.recordsFiltered !== undefined">
                    <dt>Records Filtered</dt>
                    <dd>{{ item.recordsFiltered }}</dd>
                  </span>
                  <span v-if="item.recordsTotal !== undefined">
                    <dt>Records Total</dt>
                    <dd>{{ item.recordsTotal }}</dd>
                  </span>
                </div> <!-- /count info -->
                <!-- req body -->
                <div v-if="item.body"
                  class="mt-1">
                  <dt><h5>
                    Request Body
                  </h5></dt>
                  <dd><h5>&nbsp;</h5></dd>
                  <span v-for="(value, key) in item.body"
                    :key="key">
                    <dt>{{ key }}</dt>
                    <dd>{{ value }}</dd>
                  </span>
                </div> <!-- /req body -->
                <!-- query params -->
                <div v-if="item.query"
                  class="mt-2">
                  <dt><h5>
                    Query parameters
                    <sup>
                      <span class="fa fa-info-circle text-theme-primary">
                      </span>
                    </sup>
                  </h5></dt>
                  <dd><h5>&nbsp;</h5></dd>
                  <span v-for="(value, key) in item.queryObj"
                    :key="key">
                    <dt>{{ key }}</dt>
                    <dd>
                      {{ value }}
                      <span v-if="key === 'view' && item.view && item.view.expression">
                        ({{ item.view.expression }})
                      </span>
                    </dd>
                  </span>
                  <div class="mt-2">
                    <em>
                      <strong>
                        <span class="fa fa-info-circle text-theme-primary">
                        </span>&nbsp;
                        Parsed from:
                      </strong>
                      <span style="word-break:break-all;">
                        {{ item.api }}?{{ item.query }}
                      </span>
                    </em>
                  </div>
                </div> <!-- /query params -->
              </dl>
              <!-- no info -->
              <div v-if="!item.query && !item.body && item.recordsReturned === undefined"
                class="mb-w">
                <span class="fa fa-frown-o fa-lg">
                </span>&nbsp;
                <em>Sorry! There is no more information to show.</em>
              </div> <!-- no info -->
            </td>
          </tr> <!-- /history item info -->
        </template>
      </tbody>
    </table>

    <!-- hack to make vue watch expanded logs -->
    <div style="display:none;">
      {{ expandedLogs }}
    </div>

  </div>

</template>

<script>
import UserService from '../users/UserService';
import MolochPaging from '../utils/Pagination';
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';
import ToggleBtn from '../utils/ToggleBtn';
import MolochTime from '../search/Time';

let searchInputTimeout; // timeout to debounce the search input

export default {
  name: 'History',
  components: {
    MolochPaging,
    MolochError,
    MolochLoading,
    MolochTime,
    ToggleBtn
  },
  data: function () {
    return {
      error: '',
      loading: true,
      user: null,
      history: {
        data: [],
        recordsTotal: undefined,
        recordsFiltered: undefined
      },
      expandedLogs: { change: false },
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
    debounceSearch: function () {
      if (searchInputTimeout) { clearTimeout(searchInputTimeout); }
      // debounce the input so it only issues a request after keyups cease for 400ms
      searchInputTimeout = setTimeout(() => {
        searchInputTimeout = null;
        this.loadData();
      }, 400);
    },
    columnClick: function (name) {
      this.query.sortField = name;
      this.query.desc = !this.query.desc;
      this.loadData();
    },
    toggleLogDetail: function (log) {
      log.expanded = !log.expanded;

      this.expandedLogs.change = !this.expandedLogs.change;
      this.expandedLogs[log.id] = !this.expandedLogs[log.id];

      if (log.query && log.expanded && !log.queryObj) {
        log.queryObj = {};

        let a = (log.query[0] === '?' ? log.query.substr(1) : log.query).split('&');
        for (let i = 0, len = a.length; i < len; i++) {
          let b = a[i].split('=');
          let value = b[1] || '';
          if (b[0] === 'expression') { value = value.replace(/\+/g, ' '); }
          log.queryObj[decodeURIComponent(b[0])] = decodeURIComponent(value);
        }
      }
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
      this.loading = true;

      let exists = [];
      for (let i = 0, len = this.columns.length; i < len; ++i) {
        let col = this.columns[i];
        if (col.exists) { exists.push(col.sort); }
      }
      if (exists.length) {
        this.query.exists = exists.join();
      } else {
        this.query.exists = undefined;
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
    changePaging: function (pagingValues) {
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

/* table styles -------------------- */
.history-page table {
  margin-top: 160px;
  table-layout: fixed;
}
.history-page table tbody tr td.nowrap {
  white-space: nowrap;
  text-overflow: ellipsis;
  overflow: hidden;
}
/* super tiny input boxes for column filters */
.history-page table thead tr th input.input-filter {
  height: 24px;
  padding: 2px 5px;
  font-size: 12px;
  margin-bottom: 2px;
  margin-top: 2px;
}
.history-page table thead tr th div.header-div {
  display: inline-block;
}
.history-page table thead tr th div.header-div input.checkbox {
  margin-bottom: -2px;
}
/* shrink the toggle button */
.history-page table tbody tr td a.btn-toggle {
  padding: 1px 5px;
  font-size: 12px;
  line-height: 1.5;
  border-radius: 3px;
  margin-top: -2px !important;
}
</style>
