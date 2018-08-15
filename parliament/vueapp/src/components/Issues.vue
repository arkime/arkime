<template>

  <div class="container-fluid">

    <!-- page error -->
    <div v-if="error"
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

    <!-- issues table -->
    <table class="table table-hover table-sm">
      <thead>
        <tr>
          <th v-if="loggedIn && issues.length">
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
          <th v-if="loggedIn && issues.length"
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
            </span>
            <!-- remove all issues button -->
            <button class="btn btn-outline-primary btn-xs pull-right cursor-pointer"
              v-b-tooltip.hover.bottom-right
              title="Remove ALL acknowledged issues"
              @click="removeAllAcknowledgedIssues">
              <span class="fa fa-trash fa-fw">
              </span>
            </button> <!-- /remove all issues button -->
          </th>
        </tr>
      </thead>
      <tbody>
        <template v-for="issue of issues">
          <tr :key="getIssueTrackingId(issue)"
            :class="getIssueRowClass(issue)">
            <td v-if="loggedIn">
              <input type="checkbox"
                v-model="issue.selected"
                @change="toggleIssue(issue)"
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
            <td v-if="loggedIn">
              <issue-actions class="issue-btns"
                :issue="issue"
                @issueChange="issueChange">
              </issue-actions>
            </td>
          </tr>
        </template>
      </tbody>
    </table> <!-- /issues table -->

    <!-- no issues -->
    <div v-if="!issues || !issues.length"
      class="info-area vertical-center text-center">
      <div class="text-muted mt-5">
        <span class="fa fa-3x fa-smile-o text-muted-more">
        </span>
        No issues in your Parliament
      </div>
    </div> <!-- no clusters -->

  </div>

</template>

<script>
import ParliamentService from './parliament.service';
import IssueActions from './IssueActions';

let interval;

export default {
  name: 'Issues',
  components: {
    IssueActions
  },
  data: function () {
    return {
      // page error
      error: '',
      // page data
      issues: [],
      allIssuesSelected: false,
      atLeastOneIssueSelected: false
    };
  },
  computed: {
    loggedIn: function () {
      return this.$store.state.loggedIn;
    },
    // data refresh interval
    refreshInterval: function () {
      return this.$store.state.refreshInterval;
    },
    // table sorting
    query: {
      get: function () {
        return {
          sort: this.$route.query.sort,
          order: this.$route.query.order || 'desc'
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
  mounted: function () {
    this.startAutoRefresh();
    this.loadData();
  },
  methods: {
    /* page functions ------------------------------------------------------ */
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
        sort: sort,
        order: order
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
    removeAllAcknowledgedIssues: function () {
      ParliamentService.removeAllAcknowledgedIssues()
        .then((data) => {
          this.error = '';
          this.issues = data.issues;
        })
        .catch((error) => {
          this.error = error.text || 'Error removing all acknowledged issues.';
        });
    },
    toggleIssue: function (issue) {
      // determine if at least one issue has been selected
      if (issue.selected) {
        this.atLeastOneIssueSelected = true;
      } else {
        this.atLeastOneIssueSelected = false;
        for (let issue of this.issues) {
          if (issue.selected) {
            this.atLeastOneIssueSelected = true;
            return;
          }
        }
      }
    },
    toggleAllIssues: function () {
      this.allIssuesSelected = !this.allIssuesSelected;

      this.atLeastOneIssueSelected = this.allIssuesSelected;

      for (let issue of this.issues) {
        this.$set(issue, 'selected', this.allIssuesSelected);
      }
    },
    acknowledgeIssues: function () {
      let selectedIssues = this.getSelectedIssues();

      ParliamentService.acknowledgeIssues(selectedIssues)
        .then((data) => {
          this.allIssuesSelected = false;
          this.atLeastOneIssueSelected = false;
          for (let issue of this.issues) {
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
      let selectedIssues = this.getSelectedIssues();

      ParliamentService.ignoreIssues(selectedIssues, forMs)
        .then((data) => {
          this.allIssuesSelected = false;
          this.atLeastOneIssueSelected = false;
          for (let issue of this.issues) {
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
      let selectedIssues = this.getSelectedIssues();

      ParliamentService.removeIgnoreIssues(selectedIssues)
        .then((data) => {
          this.allIssuesSelected = false;
          this.atLeastOneIssueSelected = false;
          for (let issue of this.issues) {
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
    /* helper functions ---------------------------------------------------- */
    loadData: function () {
      let query = {}; // set up query parameters (order and sort)
      if (this.query.sort) { query = { order: this.query.order, sort: this.query.sort }; }

      ParliamentService.getIssues(query)
        .then((data) => {
          this.error = '';
          this.issues = data.issues;
        })
        .catch((error) => {
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
      let selectedIssues = [];

      for (let issue of this.issues) {
        if (issue.selected) {
          selectedIssues.push(issue);
        }
      }

      return selectedIssues;
    }
  },
  beforeDestroy: function () {
    this.stopAutoRefresh();
  }
};
</script>
