<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-container
    fluid
    class="px-4 pt-3 pb-0">
    <!-- page error -->
    <v-alert
      v-if="!!error"
      type="error"
      closable
      class="mb-2"
      @click:close="error = ''">
      {{ error }}
    </v-alert> <!-- /page error -->

    <!-- search & paging -->
    <div class="d-flex align-center flex-nowrap mb-2 ga-2 arkime-toolbar">
      <Pagination
        style="height: 32px;"
        :records-total="recordsTotal"
        :records-filtered="recordsFiltered"
        :length-default="parseInt(query.length) || 50"
        @change-paging="changePaging" />

      <v-menu :close-on-content-click="false">
        <template #activator="{ props: activatorProps }">
          <v-btn
            v-bind="activatorProps"
            color="secondary"
            variant="flat"
            style="height: 32px;">
            <v-icon icon="mdi-filter" />
          </v-btn>
        </template>
        <v-list density="compact">
          <v-list-item
            :active="!filterIgnored"
            @click="toggleFilter('filterIgnored')">
            <v-list-item-title>{{ $t('parliament.issue.ignoredIssues') }}</v-list-item-title>
          </v-list-item>
          <v-list-item
            :active="!filterAckd"
            @click="toggleFilter('filterAckd')">
            <v-list-item-title>{{ $t('parliament.issue.ackedIssues') }}</v-list-item-title>
          </v-list-item>
          <v-list-item
            :active="!filterEsRed"
            @click="toggleFilter('filterEsRed')">
            <v-list-item-title>{{ $t('parliament.issue.esRedIssues') }}</v-list-item-title>
          </v-list-item>
          <v-list-item
            :active="!filterEsDown"
            @click="toggleFilter('filterEsDown')">
            <v-list-item-title>{{ $t('parliament.issue.esDownIssues') }}</v-list-item-title>
          </v-list-item>
          <v-list-item
            :active="!filterEsDropped"
            @click="toggleFilter('filterEsDropped')">
            <v-list-item-title>{{ $t('parliament.issue.esDroppedIssues') }}</v-list-item-title>
          </v-list-item>
          <v-list-item
            :active="!filterOutOfDate"
            @click="toggleFilter('filterOutOfDate')">
            <v-list-item-title>{{ $t('parliament.issue.outOfDateIssues') }}</v-list-item-title>
          </v-list-item>
          <v-list-item
            :active="!filterNoPackets"
            @click="toggleFilter('filterNoPackets')">
            <v-list-item-title>{{ $t('parliament.issue.noPacketsIssues') }}</v-list-item-title>
          </v-list-item>
          <v-list-item
            :active="!filterLowDiskSpace"
            @click="toggleFilter('filterLowDiskSpace')">
            <v-list-item-title>{{ $t('parliament.issue.filterLowDiskSpace') }}</v-list-item-title>
          </v-list-item>
          <v-list-item
            :active="!filterLowDiskSpaceES"
            @click="toggleFilter('filterLowDiskSpaceES')">
            <v-list-item-title>{{ $t('parliament.issue.filterLowDiskSpaceES') }}</v-list-item-title>
          </v-list-item>
        </v-list>
      </v-menu>

      <v-text-field
        v-model="searchTerm"
        :placeholder="$t('parliament.issue.searchTermPlaceholder')"
        prepend-inner-icon="mdi-magnify"
        clearable
        density="compact"
        hide-details
        class="flex-grow-1 issues-search"
        @input="debounceSearchInput"
        @keyup.enter="debounceSearchInput"
        @click:clear="clear" />

      <template v-if="isUser && issues && issues.length">
        <v-btn
          color="error"
          variant="outlined"
          style="height: 32px;"
          @click="removeAllAcknowledgedIssues">
          <v-icon icon="mdi-delete" />
          <transition name="visibility">
            <span
              v-if="removeAllAcknowledgedIssuesConfirm"
              class="ms-1">
              {{ $t('parliament.issue.removeAllAcknowledgedIssuesConfirm') }}
            </span>
          </transition>
          <v-tooltip
            activator="parent"
            location="bottom">
            {{ $t('parliament.issue.removeAllAckIssuesBtnTip') }}
          </v-tooltip>
        </v-btn>
        <transition name="slide-fade">
          <v-btn
            v-if="removeAllAcknowledgedIssuesConfirm"
            color="warning"
            variant="outlined"
            style="height: 32px;"
            @click="cancelRemoveAllAcknowledgedIssues">
            <v-icon icon="mdi-cancel" />
            {{ $t('common.cancel') }}
          </v-btn>
        </transition>
      </template>
    </div> <!-- /search & paging -->

    <!-- table loading -->
    <v-overlay
      v-model="loading"
      contained
      persistent
      class="issues-loading"
      :scrim="theme === 'arkime-dark' ? 'black' : 'white'">
      <div class="text-center">
        <v-icon
          icon="mdi-loading"
          class="mdi-loading"
          size="x-large" />
        <h3>{{ $t('common.loading') }}</h3>
      </div>
    </v-overlay> <!-- /table loading -->

    <!-- issues table -->
    <table
      style="position:relative"
      v-if="issues && issues.length"
      class="arkime-table">
      <thead>
        <tr>
          <th v-if="isUser && issues.length">
            <input
              type="checkbox"
              class="arkime-check-input"
              @click="toggleAllIssues"
              v-model="allIssuesSelected">
          </th>
          <th
            scope="col"
            class="cursor-pointer"
            @click="sortBy('cluster')">
            {{ $t('common.cluster') }}
            <v-icon
              icon="mdi-chevron-up"
              v-if="query.sort === 'cluster' && query.order === 'asc'" />
            <v-icon
              icon="mdi-chevron-down"
              v-if="query.sort === 'cluster' && query.order === 'desc'" />
          </th>
          <th
            scope="col"
            class="cursor-pointer"
            @click="sortBy('title')">
            {{ $t('parliament.issue.table-issue') }}
            <v-icon
              icon="mdi-chevron-up"
              v-if="query.sort === 'title' && query.order === 'asc'" />
            <v-icon
              icon="mdi-chevron-down"
              v-if="query.sort === 'title' && query.order === 'desc'" />
          </th>
          <th
            scope="col"
            class="cursor-pointer"
            @click="sortBy('firstNoticed')">
            {{ $t('parliament.issue.table-firstNoticed') }}
            <v-icon
              icon="mdi-chevron-up"
              v-if="query.sort === 'firstNoticed' && query.order === 'asc'" />
            <v-icon
              icon="mdi-chevron-down"
              v-if="query.sort === 'firstNoticed' && query.order === 'desc'" />
          </th>
          <th
            scope="col"
            class="cursor-pointer"
            @click="sortBy('lastNoticed')">
            {{ $t('parliament.issue.table-lastNoticed') }}
            <v-icon
              icon="mdi-chevron-up"
              v-if="query.sort === 'lastNoticed' && query.order === 'asc'" />
            <v-icon
              icon="mdi-chevron-down"
              v-if="query.sort === 'lastNoticed' && query.order === 'desc'" />
          </th>
          <th scope="col">
            {{ $t('parliament.issue.table-value') }}
          </th>
          <th
            scope="col"
            class="cursor-pointer"
            @click="sortBy('node')">
            {{ $t('parliament.issue.table-node') }}
            <v-icon
              icon="mdi-chevron-up"
              v-if="query.sort === 'node' && query.order === 'asc'" />
            <v-icon
              icon="mdi-chevron-down"
              v-if="query.sort === 'node' && query.order === 'desc'" />
          </th>
          <th
            scope="col"
            class="cursor-pointer"
            @click="sortBy('ignoreUntil')">
            {{ $t('parliament.issue.table-ignoreUntil') }}
            <v-icon
              icon="mdi-chevron-up"
              v-if="query.sort === 'ignoreUntil' && query.order === 'asc'" />
            <v-icon
              icon="mdi-chevron-down"
              v-if="query.sort === 'ignoreUntil' && query.order === 'desc'" />
          </th>
          <th
            scope="col"
            class="cursor-pointer"
            @click="sortBy('acknowledged')">
            {{ $t('parliament.issue.table-ackedAt') }}
            <v-icon
              icon="mdi-chevron-up"
              v-if="query.sort === 'acknowledged' && query.order === 'asc'" />
            <v-icon
              icon="mdi-chevron-down"
              v-if="query.sort === 'acknowledged' && query.order === 'desc'" />
          </th>
          <th
            v-if="isUser && issues && issues.length"
            class="text-end no-wrap"
            width="120px"
            scope="col">
            <span v-if="atLeastOneIssueSelected">
              <!-- remove selected issues button -->
              <v-btn
                size="x-small"
                variant="outlined"
                color="primary"
                class="me-1"
                @click="removeSelectedAcknowledgedIssues">
                <v-icon icon="mdi-delete" />
                <v-tooltip
                  activator="parent"
                  location="bottom">
                  {{ $t('parliament.issue.removeSelectedIssuesBtnTip') }}
                </v-tooltip>
              </v-btn>
              <!-- /remove selected issues button -->
              <!-- acknowledge issues button -->
              <v-btn
                size="x-small"
                variant="outlined"
                color="success"
                class="me-1"
                @click="acknowledgeIssues">
                <v-icon icon="mdi-check" />
                <v-tooltip
                  activator="parent"
                  location="bottom">
                  {{ $t('parliament.issue.acknowledgeIssuesBtnTip') }}
                </v-tooltip>
              </v-btn>
              <!-- /acknowledge issues button -->
              <!-- ignore until dropdown -->
              <v-menu location="bottom end">
                <template #activator="{ props: activatorProps }">
                  <v-btn
                    v-bind="activatorProps"
                    size="x-small"
                    variant="outlined"
                    class="d-inline">
                    <v-icon icon="mdi-eye-off" />
                  </v-btn>
                </template>
                <v-list density="compact">
                  <v-list-item @click="removeIgnore">
                    <v-list-item-title>{{ $t('parliament.issue.removeIgnore') }}</v-list-item-title>
                  </v-list-item>
                  <v-list-item @click="ignoreIssues(3600000)">
                    <v-list-item-title>{{ $t('parliament.issue.ignoreHourCount', 1) }}</v-list-item-title>
                  </v-list-item>
                  <v-list-item @click="ignoreIssues(21600000)">
                    <v-list-item-title>{{ $t('parliament.issue.ignoreHourCount', 6) }}</v-list-item-title>
                  </v-list-item>
                  <v-list-item @click="ignoreIssues(43200000)">
                    <v-list-item-title>{{ $t('parliament.issue.ignoreHourCount', 12) }}</v-list-item-title>
                  </v-list-item>
                  <v-list-item @click="ignoreIssues(86400000)">
                    <v-list-item-title>{{ $t('parliament.issue.ignoreDayCount', 1) }}</v-list-item-title>
                  </v-list-item>
                  <v-list-item @click="ignoreIssues(604800000)">
                    <v-list-item-title>{{ $t('parliament.issue.ignoreWeekCount', 1) }}</v-list-item-title>
                  </v-list-item>
                  <v-list-item @click="ignoreIssues(2592000000)">
                    <v-list-item-title>{{ $t('parliament.issue.ignoreMonthCount', 1) }}</v-list-item-title>
                  </v-list-item>
                  <v-list-item @click="ignoreIssues(-1)">
                    <v-list-item-title>{{ $t('parliament.issue.ignoreForever') }}</v-list-item-title>
                  </v-list-item>
                </v-list>
              </v-menu> <!-- /ignore until dropdown -->
            </span>
          </th>
        </tr>
      </thead>

      <transition-group
        name="list"
        tag="tbody">
        <template
          v-for="(issue, index) of issues"
          :key="getIssueTrackingId(issue)">
          <tr
            class="cursor-pointer"
            :class="getIssueRowClass(issue)"
            @click="navigateToStats(issue)">
            <td
              v-if="isUser"
              @click.stop>
              <input
                type="checkbox"
                class="arkime-check-input"
                v-model="issue.selected"
                @change="toggleIssue(issue, index)">
            </td>
            <td>
              {{ issue.cluster }}
            </td>
            <td>
              {{ issue.title }}
            </td>
            <td>
              {{ moment(issue.firstNoticed, 'YYYY/MM/DD HH:mm:ss') }}
            </td>
            <td>
              {{ moment(issue.lastNoticed, 'YYYY/MM/DD HH:mm:ss') }}
            </td>
            <td>
              {{ getIssueValue(issue.value, issue.type) }}
            </td>
            <td>
              {{ issue.node }}
            </td>
            <td>
              <span v-if="issue.ignoreUntil > -1">
                {{ moment(issue.ignoreUntil, 'YYYY/MM/DD HH:mm:ss') }}
              </span>
              <span v-if="issue.ignoreUntil === -1">
                Forever
              </span>
            </td>
            <td>
              <span v-if="issue.acknowledged">
                {{ moment(issue.acknowledged, 'YYYY/MM/DD HH:mm:ss') }}
              </span>
            </td>
            <td
              v-if="isUser"
              @click.stop>
              <issue-actions
                class="issue-btns"
                :issue="issue"
                @issue-change="issueChange" />
            </td>
          </tr>
        </template>
      </transition-group>
    </table> <!-- /issues table -->

    <!-- no issues -->
    <div v-if="!loading && (!issues || !issues.length)">
      <v-divider class="my-3" />
      <div class="info-area vertical-center text-center">
        <div class="text-medium-emphasis mt-5">
          <span v-if="!searchTerm && !filterIgnored && !filterAckd && !filterEsRed && !filterEsDown && !filterEsDropped && !filterOutOfDate && !filterNoPackets">
            <v-icon
              icon="mdi-emoticon-happy-outline"
              size="x-large"
              class="text-muted-more" />
            {{ $t('parliament.issue.noIssues') }}
          </span>
          <span v-else>
            <v-icon
              icon="mdi-folder-open-outline"
              size="x-large"
              class="text-muted-more" />
            {{ $t('parliament.issue.noIssuesMatch') }}
          </span>
        </div>
      </div>
    </div> <!-- /no issues -->
  </v-container>
</template>

<script>
import { mapGetters } from 'vuex';

import ParliamentService from './parliament.service.js';
import IssueActions from './IssueActions.vue';
import Pagination from '@common/Pagination.vue';
import moment from 'moment-timezone';
import { commaString } from '@common/vueFilters.js';

let interval;
let searchInputTimeout;
let lastChecked = -1;

export default {
  name: 'Issues',
  components: {
    IssueActions,
    Pagination
  },
  data: function () {
    return {
      // page error
      error: '',
      // page data loading
      loading: true,
      // page data
      issues: [],
      allIssuesSelected: false,
      atLeastOneIssueSelected: false,
      // pagination (always start on the first page)
      start: 0,
      recordsFiltered: 0,
      recordsTotal: 0,
      // searching
      searchTerm: undefined,
      // remove ALL ack issues confirm (double click)
      removeAllAcknowledgedIssuesConfirm: false,
      // shift hold for issue multiselect
      shiftHold: false
    };
  },
  mounted () {
    this.startAutoRefresh();
    this.loadData();

    // watch for shift key for multiselecting issues
    window.addEventListener('keyup', this.watchForShiftUp);
    window.addEventListener('keydown', this.watchForShiftDown);
    // if the user focus is not in the web page, remove shift hold
    window.addEventListener('blur', this.onBlur);
  },
  computed: {
    ...mapGetters(['getTheme']),
    theme () {
      return this.getTheme;
    },
    isUser: function () {
      return this.$store.state.isUser;
    },
    // data refresh interval
    refreshInterval: function () {
      return this.$store.state.refreshInterval;
    },
    // parliament data for getting cluster URLs
    parliament: function () {
      return this.$store.state.parliament;
    },
    // table sorting and paging
    query: {
      get: function () {
        return {
          sort: this.$route.query.sort,
          order: this.$route.query.order || 'desc',
          length: this.$route.query.length || 50
        };
      },
      set: function (val) {
        this.$router.replace({
          path: '/issues',
          query: val
        });
      }
    },
    // filter computed properties that sync with URL
    filterIgnored: function () {
      return this.$route.query.filterIgnored === 'true';
    },
    filterAckd: function () {
      return this.$route.query.filterAckd === 'true';
    },
    filterEsRed: function () {
      return this.$route.query.filterEsRed === 'true';
    },
    filterEsDown: function () {
      return this.$route.query.filterEsDown === 'true';
    },
    filterEsDropped: function () {
      return this.$route.query.filterEsDropped === 'true';
    },
    filterOutOfDate: function () {
      return this.$route.query.filterOutOfDate === 'true';
    },
    filterNoPackets: function () {
      return this.$route.query.filterNoPackets === 'true';
    },
    filterLowDiskSpace: function () {
      return this.$route.query.filterLowDiskSpace === 'true';
    },
    filterLowDiskSpaceES: function () {
      return this.$route.query.filterLowDiskSpaceES === 'true';
    },
    clusterIdToUrlMap: function () {
      const map = {};
      if (this.parliament && this.parliament.groups) {
        for (const group of this.parliament.groups) {
          if (group.clusters) {
            for (const cluster of group.clusters) {
              if (cluster.id && cluster.url) {
                map[cluster.id] = cluster.url;
              }
            }
          }
        }
      }
      return map;
    }
  },
  watch: {
    '$route.query': {
      handler () {
        this.loadData();
      },
      deep: true
    },
    atLeastOneIssueSelected: function (newVal) {
      // don't refresh the page when the user has at least one issue selected
      // so that the issue list doesn't change and confuse them
      newVal ? this.stopAutoRefresh() : this.startAutoRefresh();
    }
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    moment (date, format) {
      return moment(date).format(format);
    },
    getIssueValue (input, type) {
      let result = input;

      if (input === undefined || input === null) { return ''; }

      if (type === 'esDropped') {
        result = commaString(input);
      } else if (type === 'outOfDate') {
        result = moment(input).format('YYYY/MM/DD HH:mm:ss');
      } else if (type === 'lowDiskSpace' || type === 'lowDiskSpaceES') {
        result = typeof input === 'number' ? `${input.toFixed(1)}%` : '';
      }

      return result;
    },
    toggleFilter (key) {
      const currentValue = this[key];
      const newQuery = { ...this.$route.query };

      if (!currentValue) {
        newQuery[key] = 'true';
      } else {
        delete newQuery[key];
      }

      this.$router.replace({
        path: this.$route.path,
        query: newQuery
      });
    },
    issueChange: function (changeEvent) {
      this.error = changeEvent.success ? '' : changeEvent.message;

      if (!this.error) {
        // update the list of issues
        this.loadData();
      }
    },
    sortBy: function (property) {
      let order;
      let sort;

      if (this.query.sort === property) { // same sort field, so toggle order direction
        sort = this.query.sort;
        order = this.query.order === 'asc' ? 'desc' : 'asc';
      } else { // new sort field, so set default order (desc)
        sort = property;
        order = 'desc';
      }

      this.query = {
        sort,
        order,
        start: this.start,
        length: this.query.length
      };
    },
    getIssueTrackingId: function (issue) {
      let id = `${issue.groupId}-${issue.clusterId}`;
      if (issue.node) { id += `-${issue.node.replace(/\s/g, '')}`; }
      id += `-${issue.type}`;
      return id;
    },
    getIssueRowClass: function (issue) {
      if (issue.ignoreUntil || issue.acknowledged) {
        return 'arkime-table-row--muted text-medium-emphasis';
      } else if (issue.severity === 'red') {
        return 'arkime-table-row--danger';
      } else if (issue.severity === 'yellow') {
        return 'arkime-table-row--warning';
      }

      return '';
    },
    getClusterUrl: function (issue) {
      if (!issue.clusterId) {
        return null;
      }
      return this.clusterIdToUrlMap[issue.clusterId] || null;
    },
    navigateToStats: function (issue) {
      const clusterUrl = this.getClusterUrl(issue);
      if (clusterUrl) {
        let url = `${clusterUrl}/stats?statsTab=0`;
        if (issue.node) {
          // Encode node identifier for URL safety
          url += `&node=${encodeURIComponent(issue.node)}`;
        }
        window.open(url, '_blank', 'noopener,noreferrer');
      }
    },
    cancelRemoveAllAcknowledgedIssues: function () {
      this.removeAllAcknowledgedIssuesConfirm = false;
    },
    removeAllAcknowledgedIssues: function () {
      if (!this.removeAllAcknowledgedIssuesConfirm) {
        this.removeAllAcknowledgedIssuesConfirm = true;
        return;
      }

      if (this.removeAllAcknowledgedIssuesConfirm) {
        this.removeAllAcknowledgedIssuesConfirm = false;
        ParliamentService.removeAllAcknowledgedIssues()
          .then((data) => {
            this.error = '';
            this.loadData(); // fetch new issues
          })
          .catch((error) => {
            this.error = error || 'Error removing all acknowledged issues.';
          });
      }
    },
    removeSelectedAcknowledgedIssues: function () {
      const selectedIssues = this.getSelectedIssues();

      ParliamentService.removeSelectedAcknowledgedIssues(selectedIssues)
        .then((data) => {
          this.allIssuesSelected = false;
          this.atLeastOneIssueSelected = false;
          this.error = '';
          this.loadData(); // fetch new issues
        })
        .catch((error) => {
          this.error = error || `Unable to remove ${selectedIssues.length} issues`;
        });
    },
    toggleIssue (issue, index) {
      let begin = lastChecked;
      let end = index;

      // shift click (un)selects multiple issues
      if (this.shiftHold && lastChecked > -1) {
        const selected = issue.selected;

        if (lastChecked > index) { // reverse
          end = lastChecked;
          begin = index;
        }

        for (let i = begin; i < end; i++) {
          this.issues[i].selected = selected;
        }
      }

      lastChecked = index;

      // determine if at least one issue has been selected
      if (issue.selected) {
        this.atLeastOneIssueSelected = true;
      } else {
        this.atLeastOneIssueSelected = false;
        for (const i of this.issues) {
          if (i.selected) {
            this.atLeastOneIssueSelected = true;
            return;
          }
        }
      }
    },
    toggleAllIssues: function () {
      this.allIssuesSelected = !this.allIssuesSelected;

      this.atLeastOneIssueSelected = this.allIssuesSelected;

      for (const issue of this.issues) {
        issue.selected = this.allIssuesSelected;
      }
    },
    acknowledgeIssues: function () {
      const selectedIssues = this.getSelectedIssues();

      ParliamentService.acknowledgeIssues(selectedIssues)
        .then((data) => {
          this.allIssuesSelected = false;
          this.atLeastOneIssueSelected = false;
          for (const issue of this.issues) {
            if (issue.selected) {
              issue.selected = false;
              issue.acknowledged = data.acknowledged;
            }
          }
        })
        .catch((error) => {
          this.error = error || `Unable to acknowledge ${selectedIssues.length} issues`;
        });
    },
    ignoreIssues: function (forMs) {
      const selectedIssues = this.getSelectedIssues();

      ParliamentService.ignoreIssues(selectedIssues, forMs)
        .then((data) => {
          this.allIssuesSelected = false;
          this.atLeastOneIssueSelected = false;
          for (const issue of this.issues) {
            if (issue.selected) {
              issue.selected = false;
              issue.ignoreUntil = data.ignoreUntil;
            }
          }
        })
        .catch((error) => {
          this.error = error || `Unable to ignore ${selectedIssues.length} issues`;
        });
    },
    removeIgnore: function () {
      const selectedIssues = this.getSelectedIssues();

      ParliamentService.removeIgnoreIssues(selectedIssues)
        .then((data) => {
          this.allIssuesSelected = false;
          this.atLeastOneIssueSelected = false;
          for (const issue of this.issues) {
            if (issue.selected) {
              issue.selected = false;
              issue.ignoreUntil = undefined;
            }
          }
        })
        .catch((error) => {
          this.error = error || `Unable to unignore ${selectedIssues.length} issues`;
        });
    },
    /* Fired by the shared Pagination component (page-size select or
       page-number click). Emits { start, length, issueQuery }. */
    changePaging: function (args) {
      this.start = args.start;
      this.query = {
        sort: this.query.sort,
        order: this.query.order,
        length: args.length
      };
      this.loadData();
    },
    debounceSearchInput: function () {
      if (searchInputTimeout) { clearTimeout(searchInputTimeout); }
      // debounce the input so it only issues a request after keyups cease for 400ms
      searchInputTimeout = setTimeout(() => {
        clearTimeout(searchInputTimeout);
        searchInputTimeout = null;
        this.loadData();
      }, 400);
    },
    clear: function () {
      this.searchTerm = undefined;
      this.loadData();
    },
    /* helper functions ---------------------------------------------------- */
    loadData: function () {
      this.error = '';
      this.loading = true;

      const query = { // set up query parameters (order, sort, paging, filters)
        start: this.start,
        length: this.query.length,
        hideAckd: this.filterAckd,
        hideEsRed: this.filterEsRed,
        hideEsDown: this.filterEsDown,
        hideIgnored: this.filterIgnored,
        hideOutOfDate: this.filterOutOfDate,
        hideEsDropped: this.filterEsDropped,
        hideNoPackets: this.filterNoPackets,
        hideLowDiskSpace: this.filterLowDiskSpace,
        hideLowDiskSpaceES: this.filterLowDiskSpaceES
      };

      if (this.query.sort) {
        query.sort = this.query.sort;
        query.order = this.query.order;
      }

      if (this.searchTerm) {
        query.filter = this.searchTerm;
      }

      ParliamentService.getIssues(query).then((data) => {
        this.error = '';
        this.loading = false;
        this.issues = data.issues;
        this.recordsFiltered = data.recordsFiltered;
        this.recordsTotal = data.recordsTotal ?? data.recordsFiltered;
      }).catch((error) => {
        this.loading = false;
        this.error = error || 'Error fetching issues. The issues below are likely out of date';
      });
    },
    startAutoRefresh: function () {
      if (!this.refreshInterval) { return; }
      interval = setInterval(() => {
        this.loadData();
      }, this.refreshInterval);
    },
    stopAutoRefresh: function () {
      if (interval) { clearInterval(interval); }
    },
    getSelectedIssues: function () {
      const selectedIssues = [];

      for (const issue of this.issues) {
        if (issue.selected) {
          selectedIssues.push(issue);
        }
      }

      return selectedIssues;
    },
    onBlur () {
      this.shiftHold = false;
    },
    watchForShiftUp (e) {
      if (e.key === 'Shift') { // shift
        this.shiftHold = false;
        return;
      }
    },
    watchForShiftDown (e) {
      if (e.key === 'Shift') { // shift
        this.shiftHold = true;
      }
    }
  },
  beforeUnmount: function () {
    this.stopAutoRefresh();
    window.removeEventListener('blur', this.onBlur);
    window.removeEventListener('keyup', this.watchForShiftUp);
    window.removeEventListener('keydown', this.watchForShiftDown);
  }
};
</script>

<style scoped>
/* loading overlay */
.issues-loading {
  width: 400px;
  height: 200px;
  border-radius: 4px;
  top: 30% !important;
  left: calc(50% - 200px) !important;
}

/* button animation */
.slide-fade-enter-active {
  transition: all .5s ease;
}
.slide-fade-leave-active {
  transition: all .5s ease;
}
.slide-fade-enter, .slide-fade-leave-to {
  transform: translateX(-10px);
  opacity: 0;
}
.visibility-enter-active {
  transition: all .5s ease;
}
.visibility-leave-active {
  transition: all .5s cubic-bezier(1.0, 0.5, 0.8, 1.0);
}
.visibility-enter, .visibility-leave-to {
  opacity: 0;
}

/* issues table animation */
.list-enter-active, .list-leave-active {
  transition: all .5s;
}
.list-enter, .list-leave-to {
  opacity: 0;
  transform: translateX(30px);
}
.list-move {
  transition: transform 1s;
}
</style>
