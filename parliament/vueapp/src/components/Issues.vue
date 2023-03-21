<template>

  <div class="container-fluid">

    <!-- page error -->
    <div v-if="!loading && error"
      class="alert alert-danger">
      <span class="fa fa-exclamation-triangle">
      </span>&nbsp;
      {{ error }}
      <button type="button"
        class="close cursor-pointer"
        @click="error = false">
        <span>&times;</span>
      </button>
    </div> <!-- /page error -->

    <!-- search & paging -->
    <div class="d-flex align-items-center mb-1">
      <div>
        <!-- page size -->
        <select number
          class="form-control page-select"
          v-model="query.length"
          @change="updatePaging">
          <option value="10">
            10 per page
          </option>
          <option value="50">
            50 per page
          </option>
          <option value="100">
            100 per page
          </option>
          <option value="200">
            200 per page
          </option>
          <option value="500">
            500 per page
          </option>
          <option value="1000">
            1000 per page
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
          :per-page="parseInt(query.length)">
        </b-pagination> <!-- paging -->
        <!-- page info -->
        <div class="pagination-info"
          v-b-tooltip.hover>
          Showing
          <span v-if="recordsFiltered">
            {{ start + 1 }}
          </span>
          <span v-else>
            {{ start }}
          </span>
          <span v-if="recordsFiltered">
            - {{ Math.min((start + query.length), recordsFiltered) }}
          </span>
          of {{ recordsFiltered }} entries
        </div>
        <!-- /page info -->
        <template v-if="isUser">
          <!-- remove/cancel all issues button -->
          <button v-if="loggedIn && issues && issues.length"
            class="btn btn-outline-danger btn-sm cursor-pointer"
            v-b-tooltip.hover.bottom
            title="Remove ALL acknowledged issues across the ENTIRE Parliament"
            @click="removeAllAcknowledgedIssues">
            <span class="fa fa-trash fa-fw">
            </span>
            <transition name="visibility">
              <span v-if="removeAllAcknowledgedIssuesConfirm">
                Click to confirm
              </span>
            </transition>
          </button>
          <transition name="slide-fade">
            <button class="btn btn-outline-warning btn-sm cursor-pointer"
              v-if="isUser && loggedIn && issues && issues.length && removeAllAcknowledgedIssuesConfirm"
              v-b-tooltip.hover.bottom
              title="Cancel removing ALL acknowledged issues"
              @click="cancelRemoveAllAcknowledgedIssues">
              <span class="fa fa-ban fa-fw">
              </span>&nbsp;
              Cancel
            </button>
          </transition>
          <!-- /remove/cancel all issues button -->
        </template>
      </div>
      <b-btn
        size="sm"
        class="ml-1"
        v-b-tooltip.hover
        @click="toggleFilterIgnored"
        :variant="filterIgnored ? 'secondary' : 'outline-secondary'"
        :title="filterIgnored ? 'Click to include ignored issues' : 'Click to remove ignored issues'">
        <span class="fa fa-filter"></span>
      </b-btn>
      <div class="flex-grow-1 ml-1">
        <!-- search -->
        <div class="input-group input-group-sm">
          <div class="input-group-prepend">
            <span class="input-group-text input-group-text-fw">
              <span class="fa fa-search fa-fw">
              </span>
            </span>
          </div>
          <input type="text"
            class="form-control"
            v-model="searchTerm"
            @input="debounceSearchInput"
            @keyup.enter="debounceSearchInput"
            placeholder="Begin typing to search for issues"
          />
          <span class="input-group-append">
            <button
              type="button"
              @click="clear"
              class="btn btn-outline-secondary">
              <span class="fa fa-close">
              </span>
            </button>
          </span>
        </div> <!-- /search -->
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
          <span class="fa fa-spin fa-circle-o-notch fa-2x"></span>
          <h4>Loading issues...</h4>
        </div>
      </template>
    </b-overlay> <!-- /table loading -->

    <!-- issues table -->
    <table
      style="position:relative"
      v-if="issues && issues.length"
      class="table table-hover table-sm">
      <thead>
        <tr>
          <th v-if="isUser && loggedIn && issues.length">
            <input type="checkbox"
              @click="toggleAllIssues"
              v-model="allIssuesSelected"
              v-b-tooltip.hover.top-right
              title="Select/Deselect all issues"
            />
          </th>
          <th scope="col"
            class="cursor-pointer"
            @click="sortBy('cluster')">
            Cluster
            <span v-if="query.sort !== 'cluster'"
              class="fa fa-sort fa-fw">
            </span>
            <span v-if="query.sort === 'cluster' && query.order === 'asc'"
              class="fa fa-sort-asc fa-fw">
            </span>
            <span v-if="query.sort === 'cluster' && query.order === 'desc'"
              class="fa fa-sort-desc fa-fw">
            </span>
          </th>
          <th scope="col"
            class="cursor-pointer"
            @click="sortBy('title')">
            Issue
            <span v-if="query.sort !== 'title'"
              class="fa fa-sort fa-fw">
            </span>
            <span v-if="query.sort === 'title' && query.order === 'asc'"
              class="fa fa-sort-asc fa-fw">
            </span>
            <span v-if="query.sort === 'title' && query.order === 'desc'"
              class="fa fa-sort-desc fa-fw">
            </span>
          </th>
          <th scope="col"
            class="cursor-pointer"
            @click="sortBy('firstNoticed')">
            First Noticed
            <span v-if="query.sort !== 'firstNoticed'"
              class="fa fa-sort fa-fw">
            </span>
            <span v-if="query.sort === 'firstNoticed' && query.order === 'asc'"
              class="fa fa-sort-asc fa-fw">
            </span>
            <span v-if="query.sort === 'firstNoticed' && query.order === 'desc'"
              class="fa fa-sort-desc fa-fw">
            </span>
          </th>
          <th scope="col"
            class="cursor-pointer"
            @click="sortBy('lastNoticed')">
            Last Noticed
            <span v-if="query.sort !== 'lastNoticed'"
              class="fa fa-sort fa-fw">
            </span>
            <span v-if="query.sort === 'lastNoticed' && query.order === 'asc'"
              class="fa fa-sort-asc fa-fw">
            </span>
            <span v-if="query.sort === 'lastNoticed' && query.order === 'desc'"
              class="fa fa-sort-desc fa-fw">
            </span>
          </th>
          <th scope="col">
            Value
          </th>
          <th scope="col"
            class="cursor-pointer"
            @click="sortBy('node')">
            Node
            <span v-if="query.sort !== 'node'"
              class="fa fa-sort fa-fw">
            </span>
            <span v-if="query.sort === 'node' && query.order === 'asc'"
              class="fa fa-sort-asc fa-fw">
            </span>
            <span v-if="query.sort === 'node' && query.order === 'desc'"
              class="fa fa-sort-desc fa-fw">
            </span>
          </th>
          <th scope="col"
            class="cursor-pointer"
            @click="sortBy('ignoreUntil')">
            Ignored Until
            <span v-if="query.sort !== 'ignoreUntil'"
              class="fa fa-sort fa-fw">
            </span>
            <span v-if="query.sort === 'ignoreUntil' && query.order === 'asc'"
              class="fa fa-sort-asc fa-fw">
            </span>
            <span v-if="query.sort === 'ignoreUntil' && query.order === 'desc'"
              class="fa fa-sort-desc fa-fw">
            </span>
          </th>
          <th scope="col"
            class="cursor-pointer"
            @click="sortBy('acknowledged')">
            Acknowledged At
            <span v-if="query.sort !== 'acknowledged'"
              class="fa fa-sort fa-fw">
            </span>
            <span v-if="query.sort === 'acknowledged' && query.order === 'asc'"
              class="fa fa-sort-asc fa-fw">
            </span>
            <span v-if="query.sort === 'acknowledged' && query.order === 'desc'"
              class="fa fa-sort-desc fa-fw">
            </span>
          </th>
          <th v-if="isUser && loggedIn && issues && issues.length"
            width="120px"
            scope="col">
            <span v-if="atLeastOneIssueSelected">
              <!-- ignore until dropdown -->
              <b-dropdown right
                size="sm"
                class="dropdown-btn-xs pull-right ml-1"
                variant="outline-dark">
                <template slot="button-content">
                  <span class="fa fa-eye-slash fa-fw">
                  </span>
                  <span class="sr-only">
                    Ignore
                  </span>
                </template>
                <b-dropdown-item @click="removeIgnore">
                  Remove Ignore
                </b-dropdown-item>
                <b-dropdown-item @click="ignoreIssues(3600000)">
                  Ignore for 1 hour
                </b-dropdown-item>
                <b-dropdown-item @click="ignoreIssues(21600000)">
                  Ignore for 6 hour
                </b-dropdown-item>
                <b-dropdown-item @click="ignoreIssues(43200000)">
                  Ignore for 12 hour
                </b-dropdown-item>
                <b-dropdown-item @click="ignoreIssues(86400000)">
                  Ignore for 1 day
                </b-dropdown-item>
                <b-dropdown-item @click="ignoreIssues(604800000)">
                  Ignore for 1 week
                </b-dropdown-item>
                <b-dropdown-item @click="ignoreIssues(2592000000)">
                  Ignore for 1 month
                </b-dropdown-item>
                <b-dropdown-item @click="ignoreIssues(-1)">
                  Ignore forever
                </b-dropdown-item>
              </b-dropdown> <!-- /ignore until dropdown -->
              <!-- acknowledge issues button -->
              <button class="btn btn-outline-success btn-xs pull-right cursor-pointer ml-1"
                v-b-tooltip.hover.bottom-right
                title="Acknowledge all selected issues. They will be removed automatically or can be removed manually after the issue has been resolved."
                @click="acknowledgeIssues">
                <span class="fa fa-check fa-fw">
                </span>
              </button> <!-- /acknowledge issues button -->
              <!-- remove selected issues button -->
              <button class="btn btn-outline-primary btn-xs pull-right cursor-pointer"
                v-b-tooltip.hover.bottom-right
                title="Remove selected acknowledged issues"
                @click="removeSelectedAcknowledgedIssues">
                <span class="fa fa-trash fa-fw">
                </span>
              </button> <!-- /remove selected issues button -->
            </span>
          </th>
        </tr>
      </thead>

      <transition-group name="list" tag="tbody">
        <template v-for="(issue, index) of issues">
          <tr :key="getIssueTrackingId(issue)"
            :class="getIssueRowClass(issue)">
            <td v-if="isUser && loggedIn">
              <input
                type="checkbox"
                v-model="issue.selected"
                @change="toggleIssue(issue, index)"
              />
            </td>
            <td>
              {{ issue.cluster }}
            </td>
            <td>
              {{ issue.title }}
            </td>
            <td>
              {{ issue.firstNoticed | moment('YYYY/MM/DD HH:mm:ss') }}
            </td>
            <td>
              {{ issue.lastNoticed | moment('YYYY/MM/DD HH:mm:ss') }}
            </td>
            <td>
              {{ issue.value | issueValue(issue.type) }}
            </td>
            <td>
              {{ issue.node }}
            </td>
            <td>
              <span v-if="issue.ignoreUntil > -1">
                {{ issue.ignoreUntil | moment('YYYY/MM/DD HH:mm:ss') }}
              </span>
              <span v-if="issue.ignoreUntil === -1">
                Forever
              </span>
            </td>
            <td>
              <span v-if="issue.acknowledged">
                {{ issue.acknowledged | moment('YYYY/MM/DD HH:mm:ss') }}
              </span>
            </td>
            <td v-if="isUser && loggedIn">
              <issue-actions class="issue-btns"
                :issue="issue"
                @issueChange="issueChange">
              </issue-actions>
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
          <span v-if="!searchTerm">
            <span class="fa fa-3x fa-smile-o text-muted-more">
            </span>
            No issues in your Parliament
          </span>
          <span v-else>
            <span class="fa fa-3x fa-folder-open-o text-muted-more">
            </span>
            No issues match your search
          </span>
        </div>
      </div>
    </div> <!-- /no issues -->

  </div>

</template>

<script>
import ParliamentService from './parliament.service';
import IssueActions from './IssueActions';

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
      filterIgnored: false,
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
    theme () {
      return this.$store.state.theme;
    },
    isUser: function () {
      return this.$store.state.isUser;
    },
    loggedIn: function () {
      return this.$store.state.loggedIn;
    },
    // data refresh interval
    refreshInterval: function () {
      return this.$store.state.refreshInterval;
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
    }
  },
  watch: {
    '$route.query.sort': function (newVal, oldVal) {
      this.loadData();
    },
    '$route.query.order': function (newVal, oldVal) {
      this.loadData();
    },
    atLeastOneIssueSelected: function (newVal, oldVal) {
      // don't refresh the page when the user has at least one issue selected
      // so that the issue list doesn't change and confuse them
      newVal ? this.stopAutoRefresh() : this.startAutoRefresh();
    }
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    toggleFilterIgnored () {
      this.filterIgnored = !this.filterIgnored;
      this.loadData();
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
            this.error = error.text || 'Error removing all acknowledged issues.';
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
          this.error = error.text || `Unable to remove ${selectedIssues.length} issues`;
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
          this.$set(this.issues[i], 'selected', selected);
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
        this.$set(issue, 'selected', this.allIssuesSelected);
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
              this.$set(issue, 'selected', false);
              this.$set(issue, 'acknowledged', data.acknowledged);
            }
          }
        })
        .catch((error) => {
          this.error = error.text || `Unable to acknowledge ${selectedIssues.length} issues`;
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
              this.$set(issue, 'selected', false);
              this.$set(issue, 'ignoreUntil', data.ignoreUntil);
            }
          }
        })
        .catch((error) => {
          this.error = error.text || `Unable to ignore ${selectedIssues.length} issues`;
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
              this.$set(issue, 'selected', false);
              this.$set(issue, 'ignoreUntil', undefined);
            }
          }
        })
        .catch((error) => {
          this.error = error.text || `Unable to unignore ${selectedIssues.length} issues`;
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

      const query = { // set up query parameters (order, sort, paging)
        start: this.start,
        length: this.query.length
      };

      if (this.query.sort) {
        query.sort = this.query.sort;
        query.order = this.query.order;
      }

      if (this.searchTerm) {
        query.filter = this.searchTerm;
      }

      if (this.filterIgnored) {
        query.hideIgnored = true;
      }

      ParliamentService.getIssues(query)
        .then((data) => {
          this.error = '';
          this.loading = false;
          this.issues = data.issues;
          this.recordsFiltered = data.recordsFiltered;
        })
        .catch((error) => {
          this.loading = false;
          this.error = error.text || 'Error fetching issues. The issues below are likely out of date';
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
  beforeDestroy: function () {
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
