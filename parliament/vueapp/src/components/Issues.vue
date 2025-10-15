<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="container-fluid">
    <!-- page error -->
    <b-alert
      dismissible
      :show="!!error"
      class="alert alert-danger">
      <span class="fa fa-exclamation-triangle me-2" />
      {{ error }}
    </b-alert> <!-- /page error -->

    <!-- search & paging -->
    <div class="d-flex align-items-center mb-1">
      <div>
        <!-- page size -->
        <select
          number
          class="form-control page-select"
          v-model="query.length"
          @change="updatePaging">
          <option value="10">
            {{ $t('common.perPage', 10) }}
          </option>
          <option value="50">
            {{ $t('common.perPage', 50) }}
          </option>
          <option value="100">
            {{ $t('common.perPage', 100) }}
          </option>
          <option value="200">
            {{ $t('common.perPage', 200) }}
          </option>
          <option value="500">
            {{ $t('common.perPage', 500) }}
          </option>
          <option value="1000">
            {{ $t('common.perPage', 1000) }}
          </option>
        </select> <!-- /page size -->
        <!-- paging -->
        <b-pagination
          size="sm"
          :limit="5"
          hide-ellipsis
          @input="updatePaging"
          v-model="currentPage"
          :total-rows="recordsFiltered"
          :per-page="parseInt(query.length)" /> <!-- paging -->
        <!-- page info -->
        <div class="pagination-info">
          <span v-if="recordsFiltered">
            {{ $t('common.showingRange', {start: start + 1, end: Math.min((start + query.length), recordsFiltered), total: recordsFiltered }) }}
          </span>
          <span v-else>
            {{ $t('common.showingAll', {count: start, total: recordsFiltered }) }}
          </span>
        </div>
        <!-- /page info -->
        <template v-if="isUser && issues && issues.length">
          <!-- remove/cancel all issues button -->
          <button
            id="removeAllAckIssuesBtn"
            class="btn btn-outline-danger btn-sm cursor-pointer ms-2 me-1"
            @click="removeAllAcknowledgedIssues">
            <span class="fa fa-trash fa-fw" />
            <transition name="visibility">
              <span v-if="removeAllAcknowledgedIssuesConfirm">
                {{ $t('parliament.issue.removeAllAcknowledgedIssuesConfirm') }}
              </span>
            </transition>
          </button>
          <BTooltip
            target="removeAllAckIssuesBtn"
            placement="bottom">
            {{ $t('parliament.issue.removeAllAckIssuesBtnTip') }}
          </BTooltip>
          <transition name="slide-fade">
            <button
              class="btn btn-outline-warning btn-sm cursor-pointer"
              v-if="removeAllAcknowledgedIssuesConfirm"
              @click="cancelRemoveAllAcknowledgedIssues">
              <span class="fa fa-ban fa-fw" />&nbsp;
              {{ $t('common.cancel') }}
            </button>
          </transition>
          <!-- /remove/cancel all issues button -->
        </template>
      </div>
      <b-dropdown
        no-caret
        size="sm"
        class="ms-1 me-1"
        variant="secondary">
        <template #button-content>
          <span class="fa fa-filter fa-fw" />
        </template>
        <b-dropdown-item
          :active="!filterIgnored"
          @click.capture.stop.prevent="toggleFilter('filterIgnored')">
          {{ $t('parliament.issue.ignoredIssues') }}
        </b-dropdown-item>
        <b-dropdown-item
          :active="!filterAckd"
          @click.capture.stop.prevent="toggleFilter('filterAckd')">
          {{ $t('parliament.issue.ackedIssues') }}
        </b-dropdown-item>
        <b-dropdown-item
          :active="!filterEsRed"
          @click.capture.stop.prevent="toggleFilter('filterEsRed')">
          {{ $t('parliament.issue.esResIssues') }}
        </b-dropdown-item>
        <b-dropdown-item
          :active="!filterEsDown"
          @click.capture.stop.prevent="toggleFilter('filterEsDown')">
          {{ $t('parliament.issue.esDownIssues') }}
        </b-dropdown-item>
        <b-dropdown-item
          :active="!filterEsDropped"
          @click.capture.stop.prevent="toggleFilter('filterEsDropped')">
          {{ $t('parliament.issue.esDroppedIssues') }}
        </b-dropdown-item>
        <b-dropdown-item
          :active="!filterOutOfDate"
          @click.capture.stop.prevent="toggleFilter('filterOutOfDate')">
          {{ $t('parliament.issue.ofOfDateIssues') }}
        </b-dropdown-item>
        <b-dropdown-item
          :active="!filterNoPackets"
          @click.capture.stop.prevent="toggleFilter('filterNoPackets')">
          {{ $t('parliament.issue.noPacketsIssues') }}
        </b-dropdown-item>
        <b-dropdown-item
          :active="!filterLowDiskSpace"
          @click.capture.stop.prevent="toggleFilter('filterLowDiskSpace')">
          Low Disk Space Issues
        </b-dropdown-item>
        <b-dropdown-item
          :active="!filterLowDiskSpaceES"
          @click.capture.stop.prevent="toggleFilter('filterLowDiskSpaceES')">
          ES Low Disk Space Issues
        </b-dropdown-item>
      </b-dropdown>
      <div class="flex-grow-1 ms-1">
        <!-- search -->
        <BInputGroup size="sm">
          <BInputGroupText>
            <span class="fa fa-search fa-fw" />
          </BInputGroupText>
          <input
            type="text"
            class="form-control"
            v-model="searchTerm"
            @input="debounceSearchInput"
            @keyup.enter="debounceSearchInput"
            :placeholder="$t('parliament.issue.searchTermPlaceholder')">
          <button
            type="button"
            @click="clear"
            class="btn btn-outline-secondary">
            <span class="fa fa-close" />
          </button>
        </BInputGroup> <!-- /search -->
      </div>
    </div> <!-- /search & paging -->

    <!-- table loading -->
    <b-overlay
      no-wrap
      opacity=".8"
      rounded="lg"
      :show="loading"
      :variant="theme"
      class="issues-loading">
      <template #overlay>
        <div class="text-center">
          <span class="fa fa-spin fa-circle-o-notch fa-2x" />
          <h4>{{ $t('common.loading') }}</h4>
        </div>
      </template>
    </b-overlay> <!-- /table loading -->

    <!-- issues table -->
    <table
      style="position:relative"
      v-if="issues && issues.length"
      :class="{ 'table-dark': getTheme === 'dark' }"
      class="table table-hover table-sm">
      <thead>
        <tr>
          <th v-if="isUser && issues.length">
            <input
              type="checkbox"
              @click="toggleAllIssues"
              v-model="allIssuesSelected">
          </th>
          <th
            scope="col"
            class="cursor-pointer"
            @click="sortBy('cluster')">
            {{ $t('common.cluster') }}
            <span
              v-if="query.sort !== 'cluster'"
              class="fa fa-sort fa-fw" />
            <span
              v-if="query.sort === 'cluster' && query.order === 'asc'"
              class="fa fa-sort-asc fa-fw" />
            <span
              v-if="query.sort === 'cluster' && query.order === 'desc'"
              class="fa fa-sort-desc fa-fw" />
          </th>
          <th
            scope="col"
            class="cursor-pointer"
            @click="sortBy('title')">
            {{ $t('common.cluster') }}
            <span
              v-if="query.sort !== 'title'"
              class="fa fa-sort fa-fw" />
            <span
              v-if="query.sort === 'title' && query.order === 'asc'"
              class="fa fa-sort-asc fa-fw" />
            <span
              v-if="query.sort === 'title' && query.order === 'desc'"
              class="fa fa-sort-desc fa-fw" />
          </th>
          <th
            scope="col"
            class="cursor-pointer"
            @click="sortBy('firstNoticed')">
            {{ $t('parliament.issue.table-firstNoticed') }}
            <span
              v-if="query.sort !== 'firstNoticed'"
              class="fa fa-sort fa-fw" />
            <span
              v-if="query.sort === 'firstNoticed' && query.order === 'asc'"
              class="fa fa-sort-asc fa-fw" />
            <span
              v-if="query.sort === 'firstNoticed' && query.order === 'desc'"
              class="fa fa-sort-desc fa-fw" />
          </th>
          <th
            scope="col"
            class="cursor-pointer"
            @click="sortBy('lastNoticed')">
            {{ $t('parliament.issue.table-lastNoticed') }}
            <span
              v-if="query.sort !== 'lastNoticed'"
              class="fa fa-sort fa-fw" />
            <span
              v-if="query.sort === 'lastNoticed' && query.order === 'asc'"
              class="fa fa-sort-asc fa-fw" />
            <span
              v-if="query.sort === 'lastNoticed' && query.order === 'desc'"
              class="fa fa-sort-desc fa-fw" />
          </th>
          <th scope="col">
            Value
          </th>
          <th
            scope="col"
            class="cursor-pointer"
            @click="sortBy('node')">
            {{ $t('parliament.issue.table-node') }}
            <span
              v-if="query.sort !== 'node'"
              class="fa fa-sort fa-fw" />
            <span
              v-if="query.sort === 'node' && query.order === 'asc'"
              class="fa fa-sort-asc fa-fw" />
            <span
              v-if="query.sort === 'node' && query.order === 'desc'"
              class="fa fa-sort-desc fa-fw" />
          </th>
          <th
            scope="col"
            class="cursor-pointer"
            @click="sortBy('ignoreUntil')">
            {{ $t('parliament.issue.table-ignoreUntil') }}
            <span
              v-if="query.sort !== 'ignoreUntil'"
              class="fa fa-sort fa-fw" />
            <span
              v-if="query.sort === 'ignoreUntil' && query.order === 'asc'"
              class="fa fa-sort-asc fa-fw" />
            <span
              v-if="query.sort === 'ignoreUntil' && query.order === 'desc'"
              class="fa fa-sort-desc fa-fw" />
          </th>
          <th
            scope="col"
            class="cursor-pointer"
            @click="sortBy('acknowledged')">
            {{ $t('parliament.issue.table-ackedAt') }}
            <span
              v-if="query.sort !== 'acknowledged'"
              class="fa fa-sort fa-fw" />
            <span
              v-if="query.sort === 'acknowledged' && query.order === 'asc'"
              class="fa fa-sort-asc fa-fw" />
            <span
              v-if="query.sort === 'acknowledged' && query.order === 'desc'"
              class="fa fa-sort-desc fa-fw" />
          </th>
          <th
            v-if="isUser && issues && issues.length"
            class="text-end no-wrap"
            width="120px"
            scope="col">
            <span v-if="atLeastOneIssueSelected">
              <!-- remove selected issues button -->
              <button
                class="btn btn-outline-primary btn-xs cursor-pointer me-1"
                id="removeSelectedIssuesBtn"
                @click="removeSelectedAcknowledgedIssues">
                <span class="fa fa-trash fa-fw" />
              </button>
              <BTooltip
                target="removeSelectedIssuesBtn"
                placement="bottom">
                {{ $t('parliament.issue.removeSelectedIssuesBtnTip') }}
              </BTooltip>
              <!-- /remove selected issues button -->
              <!-- acknowledge issues button -->
              <button
                class="btn btn-outline-success btn-xs cursor-pointer me-1"
                id="acknowledgeIssuesBtn"
                @click="acknowledgeIssues">
                <span class="fa fa-check fa-fw" />
              </button>
              <BTooltip
                target="acknowledgeIssuesBtn"
                placement="bottom">
                {{ $t('parliament.issue.acknowledgeIssuesBtnTip') }}
              </BTooltip>
              <!-- /acknowledge issues button -->
              <!-- ignore until dropdown -->
              <b-dropdown
                right
                size="sm"
                class="dropdown-btn-xs d-inline"
                variant="outline-dark">
                <template #button-content>
                  <span class="fa fa-eye-slash fa-fw" />
                  <span class="sr-only">
                    Ignore
                  </span>
                </template>

                <b-dropdown-item @click="removeIgnore">
                  {{ $t('parliament.issue.removeIgnore') }}
                </b-dropdown-item>
                <b-dropdown-item @click="ignoreIssue(3600000)">
                  {{ $t('parliament.issue.ignoreHourCount', 1) }}
                </b-dropdown-item>
                <b-dropdown-item @click="ignoreIssue(21600000)">
                  {{ $t('parliament.issue.ignoreHourCount', 6) }}
                </b-dropdown-item>
                <b-dropdown-item @click="ignoreIssue(43200000)">
                  {{ $t('parliament.issue.ignoreHourCount', 12) }}
                </b-dropdown-item>
                <b-dropdown-item @click="ignoreIssue(86400000)">
                  {{ $t('parliament.issue.ignoreDayCount', 1) }}
                </b-dropdown-item>
                <b-dropdown-item @click="ignoreIssue(604800000)">
                  {{ $t('parliament.issue.ignoreWeekCount', 1) }}
                </b-dropdown-item>
                <b-dropdown-item @click="ignoreIssue(2592000000)">
                  {{ $t('parliament.issue.ignoreMonthCount', 1) }}
                </b-dropdown-item>
                <b-dropdown-item @click="ignoreIssue(-1)">
                  {{ $t('parliament.issue.ignoreForever') }}
                </b-dropdown-item>
              </b-dropdown> <!-- /ignore until dropdown -->
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
      <hr>
      <div class="info-area vertical-center text-center">
        <div class="text-muted mt-5">
          <span v-if="!searchTerm && !filterIgnored && !filterAckd && !filterEsRed && !filterEsDown && !filterEsDropped && !filterOutOfDate && !filterNoPackets">
            <span class="fa fa-3x fa-smile-o text-muted-more" />
            {{ $t('parliament.issue.noIssues') }}
          </span>
          <span v-else>
            <span class="fa fa-3x fa-folder-open-o text-muted-more" />
            {{ $t('parliament.issue.noIssuesMatch') }}
          </span>
        </div>
      </div>
    </div> <!-- /no issues -->
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import ParliamentService from './parliament.service.js';
import IssueActions from './IssueActions.vue';
import moment from 'moment-timezone';
import { commaString } from '@common/vueFilters.js';

let interval;
let searchInputTimeout;
let lastChecked = -1;

export default {
  name: 'Issues',
  components: {
    IssueActions
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
      currentPage: 1,
      recordsFiltered: 0,
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
        return 'table-secondary text-muted';
      } else if (issue.severity === 'red') {
        return 'table-danger';
      } else if (issue.severity === 'yellow') {
        return 'table-warning';
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
    updatePaging: function () {
      this.start = (this.currentPage - 1) * this.query.length;

      this.query = {
        sort: this.query.sort,
        order: this.query.order,
        length: this.query.length
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
.pagination {
  margin-bottom: 0px;
  display: inline-flex;
}

select.page-select {
  width: 120px;
  font-size: .8rem;
  display: inline-flex;
  height: 31px !important;
  margin-top: 1px;
  margin-right: -5px;
  margin-bottom: var(--px-xs);
  padding-top: var(--px-xs);
  padding-bottom: var(--px-xs);
  border-right: none;
  -webkit-appearance: none;
  border-radius: 4px 0 0 4px;
}

.pagination-info {
  display: inline-block;
  font-size: .8rem;
  color: #495057;
  border: 1px solid #CED4DA;
  padding: 5px 10px;
  margin-left: -6px;
  border-radius: 0 var(--px-sm) var(--px-sm) 0;
  background-color: #FFFFFF;
}

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
