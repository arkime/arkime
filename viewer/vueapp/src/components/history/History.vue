<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>

  <div class="history-page">
    <ArkimeCollapsible>
      <span class="fixed-header">
        <!-- search navbar -->
        <div class="history-search p-1">
          <Clusters
            class="pull-right"
          />
          <button type="button"
            class="btn btn-sm btn-theme-tertiary pull-right ms-1 search-btn"
            @click="loadData">
            <span v-if="!shiftKeyHold">
              Search
            </span>
            <span v-else
              class="enter-icon">
              <span class="fa fa-long-arrow-left fa-lg">
              </span>
              <div class="enter-arm">
              </div>
            </span>
          </button>
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
              @keyup.enter="loadData"
              @input="debounceSearch"
              class="form-control"
              v-model="searchTerm"
              v-focus="focusInput"
              @blur="onOffFocus"
              :placeholder="$t('history.searchHistoryPlaceholder')"
            />
            <button type="button"
              @click="clear"
              :disabled="!searchTerm"
              class="btn btn-outline-secondary btn-clear-input">
              <span class="fa fa-close">
              </span>
            </button>
            <BInputGroupText id="searchHistory">
              <span class="fa fa-lg fa-question-circle text-theme-primary help-cursor">
              </span>
              <BTooltip target="searchHistory">
                <div v-html="$t('history.searchHistoryTip')"></div>
              </BTooltip>
            </BInputGroupText>
          </BInputGroup>
          <arkime-time
            class="mt-1"
            :timezone="user.settings.timezone"
            @timeChange="loadData"
            :hide-bounding="true"
            :hide-interval="true">
          </arkime-time>
        </div> <!-- /search navbar -->

        <!-- paging navbar -->
        <div class="history-paging pt-1">
          <arkime-paging
            class="ms-1 d-inline"
            :length-default="100"
            :records-total="recordsTotal"
            :records-filtered="recordsFiltered"
            @changePaging="changePaging"
          />
          <arkime-toast
            class="ms-2 mb-3 mt-1 d-inline"
            :message="msg"
            :type="msgType"
            :done="messageDone">
          </arkime-toast>
        </div> <!-- /paging navbar -->
      </span>
    </ArkimeCollapsible>

    <table v-if="!error"
      class="table table-sm table-striped small">
      <thead>
        <tr>
          <th width="100px;">
            <button
              id="toggleColFilters"
              class="btn btn-xs btn-primary margined-bottom-sm"
              @click="showColFilters = !showColFilters">
              <span class="fa fa-filter"></span>
              <BTooltip target="toggleColFilters"><span v-i18n-btip="'history.'" /></BTooltip>
            </button>
          </th>
          <th
            :key="column.name"
            v-for="column of columns"
            v-has-permission="column.permission"
            :style="{'width': `${column.width}%`}"
            v-has-role="{user:user,roles:column.role}"
            :class="`cursor-pointer ${column.classes}`">
             <b-form-checkbox
              id="seeAll"
              @input="toggleSeeAll"
              class="d-inline me-2"
              v-if="column.sort == 'userId'">
            </b-form-checkbox>
            <BTooltip target="seeAll" noninteractive placement="bottom" boundary="viewport" teleport-to="body">
              <span v-html="$t('history.seeAllTip')" />
            </BTooltip>
            <input
              type="text"
              @click.stop
              data-lpignore="true"
              @keyup="debounceSearch"
              v-model="filters[column.sort]"
              v-has-permission="column.permission"
              v-if="column.filter && showColFilters"
              :placeholder="$t('history.filterByPlaceholder', { name: column.name })"
              class="form-control form-control-sm input-filter"
              :id="`filter-${column.name}`"
            />
            <div v-if="column.exists"
              :id="`exists-${column.name}`"
              class="me-1 header-div">
              <input
                type="checkbox"
                class="checkbox"
                @change="loadData"
                v-model="column.exists"
              />
              <BTooltip
                placement="bottom"
                triggers="hover"
                :target="`exists-${column.name}`">
                {{ $t('history.existsTip', { name: column.name }) }}
              </BTooltip>
            </div>
            <div class="header-div break-word"
            :id="`column-${column.name}`"
              @click="columnClick(column.sort)">
              <span v-if="column.sort !== undefined">
                <span v-show="sortField === column.sort && !desc" class="fa fa-sort-asc"></span>
                <span v-show="sortField === column.sort && desc" class="fa fa-sort-desc"></span>
                <span v-show="sortField !== column.sort" class="fa fa-sort"></span>
              </span>
              {{ column.name }}
              <BTooltip
                placement="bottom"
                triggers="hover"
                :target="`column-${column.name}`">
                {{ column.help }}
              </BTooltip>
            </div>
          </th>
        </tr>
      </thead>
      <tbody v-if="history">
        <!-- no results -->
        <tr v-if="!history.length">
          <td :colspan="colSpan"
            class="text-danger text-center">
            <span class="fa fa-warning">
            </span>&nbsp;
            <strong>
              {{ $t('history.noHistory') }}
            </strong>
          </td>
        </tr> <!-- /no results -->
        <template v-for="(item, index) of history" :key="item.id">
          <!-- history item -->
          <tr>
            <td class="no-wrap">
              <toggle-btn :opened="item.expanded"
                @toggle="toggleLogDetail(item)">
              </toggle-btn>
              <button
                type="button"
                role="button"
                title="Delete history"
                class="btn btn-xs btn-warning ms-1"
                v-has-role="{user:user,roles:'arkimeAdmin'}"
                v-has-permission="'removeEnabled'"
                @click="deleteLog(item, index)">
                <span class="fa fa-trash-o">
                </span>
              </button>
              <a :id="`openPage-${item.id}`"
                class="btn btn-xs btn-info ms-1"
                v-if="item.uiPage"
                tooltip-placement="right"
                @click="openPage(item)">
                <span class="fa fa-folder-open">
                </span>
                <BTooltip :target="`openPage-${item.id}`">
                  {{ $t('history.openPageTip', { uiPage: item.uiPage }) }}
                </BTooltip>
              </a>
            </td>
            <td class="no-wrap">
              {{ timezoneDateString(item.timestamp * 1000, user.settings.timezone) }}
            </td>
            <td class="no-wrap text-end">
              {{ readableTime(item.range*1000) }}
            </td>
            <td v-has-role="{user:user,roles:'arkimeAdmin'}"
              class="no-wrap">
              {{ item.userId }}
            </td>
            <td class="no-wrap text-end">
              {{ item.queryTime }}ms
            </td>
            <td class="no-wrap">
              {{ item.method }}
            </td>
            <td class="no-wrap"
              :title="item.api">
              {{ item.api }}
            </td>
            <td class="no-wrap">
              {{ item.expression }}
            </td>
            <td class="no-wrap">
              <template v-if="item.view">
                <strong>
                  {{ item.view.name }}
                </strong>
                {{ item.view.expression }}
              </template>
            </td>
          </tr> <!-- /history item -->
          <!-- history item info -->
          <tr :key="item.id+'-detail'"
            v-if="expandedLogs[item.id]">
            <td :colspan="colSpan">
              <dl class="dl-horizontal">
                <!-- forced expression -->
                <div v-has-role="{user:user,roles:'arkimeAdmin'}"
                  v-if="item.forcedExpression !== undefined"
                  class="mt-1">
                  <dt>{{ $t('users.forcedExpression') }}</dt>
                  <dd class="break-word">{{ item.forcedExpression }}</dd>
                </div> <!-- /forced expression -->
                <!-- count info -->
                <div v-if="item.recordsReturned !== undefined"
                  class="mt-1">
                  <dt><h5>
                    Counts
                  </h5></dt>
                  <dd><h5>&nbsp;</h5></dd>
                  <template v-if="item.recordsReturned !== undefined">
                    <dt>{{ $t('history.recordsReturned') }}</dt>
                    <dd>{{ item.recordsReturned }}</dd>
                  </template>
                  <template v-if="item.recordsFiltered !== undefined">
                    <dt>{{ $t('history.recordsFiltered') }}</dt>
                    <dd>{{ item.recordsFiltered }}</dd>
                  </template>
                  <template v-if="item.recordsTotal !== undefined">
                    <dt>{{ $t('history.recordsTotal') }}</dt>
                    <dd>{{ item.recordsTotal }}</dd>
                  </template>
                </div> <!-- /count info -->
                <!-- req body -->
                <div v-if="item.body"
                  class="mt-1">
                  <dt><h5>
                    {{ $t('history.requestBody') }}
                  </h5></dt>
                  <dd><h5>&nbsp;</h5></dd>
                  <template v-for="(value, key) in item.body"
                    :key="key">
                    <dt>{{ key }}</dt>
                    <dd class="break-word">{{ value }}&nbsp;</dd>
                  </template>
                </div> <!-- /req body -->
                <!-- query params -->
                <div class="mt-2">
                  <template v-if="item.query">
                    <dt><h5>
                      {{ $t('history.queryParameters') }}
                      <sup>
                        <span class="fa fa-info-circle text-theme-primary">
                        </span>
                      </sup>
                    </h5></dt>
                    <dd><h5>&nbsp;</h5></dd>
                    <template v-for="(value, key) in item.queryObj"
                      :key="key">
                      <dt>{{ key }}</dt>
                      <dd class="break-word">
                        {{ value }}&nbsp;
                        <template v-if="key === 'view' && item.view && item.view.expression">
                          ({{ item.view.expression }})
                        </template>
                      </dd>
                    </template>
                  </template>
                  <div class="mt-2">
                    <em>
                      <strong v-if="item.query">
                        <span class="fa fa-info-circle text-theme-primary">
                        </span>&nbsp;
                        {{ $t('history.parsedFrom') }}
                      </strong>
                      <span style="word-break:break-all;">
                        {{ item.api }}{{ item.query ? '?' : '' }}{{ item.query }}
                      </span>
                    </em>
                  </div>
                </div> <!-- /query params -->
                <!-- es query -->
                <div v-has-role="{user:user,roles:'arkimeAdmin'}">
                  <div class="mt-3" v-if="item.esQueryIndices">
                    <h5>{{ $t('history.esQueryIndices') }}</h5>
                    <code class="me-3 ms-3">{{ item.esQueryIndices }}</code>
                  </div>
                  <div class="mt-3" v-if="item.esQuery">
                    <h5>{{ $t('history.esQuery') }}</h5>
                    <pre class="me-3 ms-3">{{ JSON.parse(item.esQuery) }}</pre>
                  </div>
                </div>
              </dl>
            </td>
          </tr> <!-- /history item info -->
        </template>
      </tbody>
    </table>

    <!-- loading overlay -->
    <arkime-loading
      v-if="loading && !error">
    </arkime-loading> <!-- /loading overlay -->

    <!-- error -->
    <arkime-error
      v-if="error"
      :message="error"
    /> <!-- /error -->

    <!-- hack to make vue watch expanded logs -->
    <div style="display:none;">
      {{ expandedLogs }}
    </div>

  </div>

</template>

<script>
import qs from 'qs';
import Utils from '../utils/utils';
import ArkimeTime from '../search/Time.vue';
import Clusters from '../utils/Clusters.vue';
import ArkimeToast from '../utils/Toast.vue';
import ArkimeError from '../utils/Error.vue';
import ArkimeLoading from '../utils/Loading.vue';
import ArkimePaging from '../utils/Pagination.vue';
import HistoryService from './HistoryService';
import Focus from '@common/Focus.vue';
import ArkimeCollapsible from '../utils/CollapsibleWrapper.vue';
import ToggleBtn from '@common/ToggleBtn.vue';
import { timezoneDateString, readableTime } from '@common/vueFilters.js';

let searchInputTimeout; // timeout to debounce the search input

export default {
  name: 'ArkimeHistory',
  components: {
    ArkimePaging,
    ArkimeLoading,
    ArkimeError,
    ArkimeTime,
    ToggleBtn,
    ArkimeToast,
    ArkimeCollapsible,
    Clusters
  },
  directives: { Focus },
  data: function () {
    const $t = this.$t.bind(this);
    function intl(obj) {
      const key = obj.sort.replace('.', '');
      obj.name = $t(`history.${key}Name`);
      obj.help = $t(`history.${key}Help`);
      return obj;
    };
    return {
      error: '',
      loading: false,
      history: {},
      recordsTotal: 0,
      recordsFiltered: 0,
      expandedLogs: { change: false },
      showColFilters: false,
      colSpan: 8,
      filters: {},
      sortField: 'timestamp',
      searchTerm: '',
      desc: true,
      msg: '',
      msgType: undefined,
      seeAll: false,
      columns: [
        intl({ sort: 'timestamp', width: 13 }),
        intl({ sort: 'range', width: 11, classes: 'text-end' }),
        intl({ sort: 'userId', width: 10, filter: true, role: 'arkimeAdmin' }),
        intl({ sort: 'queryTime', width: 8, classes: 'text-end' }),
        intl({ sort: 'method', width: 8 }),
        intl({ sort: 'api', width: 15, filter: true }),
        intl({ sort: 'expression', width: 20, exists: false }),
        intl({ sort: 'view.name', width: 15, exists: false })
      ]
    };
  },
  computed: {
    query: function () {
      return { // query defaults
        length: parseInt(this.$route.query.length) || 50,
        start: 0,
        date: this.$store.state.timeRange,
        startTime: this.$store.state.time.startTime,
        stopTime: this.$store.state.time.stopTime,
        cluster: this.$route.query.cluster || undefined
      };
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
    },
    issueSearch: function () {
      return this.$store.state.issueSearch;
    }
  },
  watch: {
    issueSearch: function (newVal, oldVal) {
      if (newVal) { this.loadData(); }
    },
    '$route.query.cluster': {
      handler: function (newVal, oldVal) {
        if (newVal !== oldVal) {
          this.query.cluster = newVal;
          this.loadData();
        }
      }
    }
  },
  created: function () {
    // if the user is an admin, show them all the columns
    if (this.user.roles.includes('arkimeAdmin')) { this.colSpan = 9; }
    // query for the user requested or the current user
    this.filters.userId = this.$route.query.userId || this.user.userId;

    setTimeout(() => {
      // wait query to be computed
      this.loadData();
    });
  },
  methods: {
    readableTime,
    timezoneDateString,
    /* exposed page functions ------------------------------------ */
    toggleSeeAll () {
      this.filters.userId = this.seeAll ? '' : this.user.userId;
      if (this.seeAll) { this.showColFilters = true; }
      this.loadData();
    },
    debounceSearch: function () {
      if (searchInputTimeout) { clearTimeout(searchInputTimeout); }
      // debounce the input so it only issues a request after keyups cease for 400ms
      searchInputTimeout = setTimeout(() => {
        searchInputTimeout = null;
        this.loadData();
      }, 400);
    },
    clear () {
      this.searchTerm = undefined;
      this.loadData();
    },
    onOffFocus: function () {
      this.focusInput = false;
    },
    columnClick: function (colName) {
      this.sortField = colName;
      this.desc = !this.desc;
      this.loadData();
    },
    toggleLogDetail: function (log) {
      log.expanded = !log.expanded;

      this.expandedLogs.change = !this.expandedLogs.change;
      this.expandedLogs[log.id] = !this.expandedLogs[log.id];

      if (log.query && log.expanded && !log.queryObj) {
        log.queryObj = {};
        // match only single & (&& can be in the search expression)
        const a = (log.query[0] === '?' ? log.query.substr(1) : log.query).split(/&(?![&& ])/g);
        for (let i = 0, len = a.length; i < len; i++) {
          const b = a[i].split(/=(.+)/); // splits on first '=';
          let value = b[1] || '';
          if (b[0] === 'expression') { value = value.replace(/\+/g, ' '); }
          log.queryObj[decodeURIComponent(b[0])] = decodeURIComponent(value);
        }
      }
    },
    deleteLog: function (log, index) {
      HistoryService.delete(log.id, log.index)
        .then((response) => {
          this.history.splice(index, 1);
          this.msg = response.text || 'Successfully deleted history item';
          this.msgType = 'success';
        })
        .catch((error) => {
          this.msg = error.text || 'Error deleting history item';
          this.msgType = 'danger';
        });
    },
    /* opens the page that this history item represents */
    openPage (item) {
      let query = item.query;
      if (item.expression && item.query.includes(item.expression)) {
        // need to remove expression because browsers can't handle the &&
        const remove = `&expression=${item.expression}`;
        const splitQuery = item.query.split(remove);
        query = splitQuery.join('');
      }

      this.$router.push({
        query: {
          ...qs.parse(query),
          expression: item.expression
        },
        path: item.uiPage
      });
    },
    /* remove the message when user is done with it or duration ends */
    messageDone: function () {
      this.msg = '';
      this.msgType = undefined;
    },
    /* helper functions ------------------------------------------ */
    loadData: function () {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        this.history = {};
        return;
      }

      this.loading = true;

      const exists = [];
      for (let i = 0, len = this.columns.length; i < len; ++i) {
        const col = this.columns[i];
        if (col.exists) { exists.push(col.sort); }
      }
      if (exists.length) {
        this.query.exists = exists.join();
      } else {
        this.query.exists = undefined;
      }

      if (this.filters && Object.keys(this.filters).length) {
        for (const key in this.filters) {
          this.query[key] = this.filters[key];
        }
      }

      this.query.desc = this.desc;
      this.query.sortField = this.sortField;
      this.query.searchTerm = this.searchTerm;

      HistoryService.get(this.query)
        .then((response) => {
          this.error = '';
          this.loading = false;
          this.history = response.data;
          this.recordsTotal = response.recordsTotal;
          this.recordsFiltered = response.recordsFiltered;
        })
        .catch((error) => {
          this.loading = false;
          this.error = error.text || error;
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
.history-page .history-search {
  z-index: 5;
  border: none;
  background-color: var(--color-secondary-lightest);
  position: relative;
  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

.history-page  .time-range-control {
  -webkit-appearance: none;
}

/* navbar with pagination */
.history-page .history-paging {
  z-index: 4;
  height: 40px;
}

.input-group {
  flex-wrap: none;
  width: auto;
}

/* table styles -------------------- */
.history-page table {
  margin-top: 10px;
  table-layout: fixed;
}
.history-page table tbody tr td.no-wrap {
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

<style>
/* shrink all btn */
.history-page div.all-btn > label {
  height: 18px;
  margin-top: -5px;
  font-size: 0.8rem;
  line-height: 0.9rem;
  padding: 0.1rem 0.3rem;
}
</style>
