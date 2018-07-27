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
          <th scope="col" width="100px" v-if="loggedIn">
            &nbsp;
          </th>
        </tr>
      </thead>
      <tbody>
        <template v-for="issue of issues">
          <tr v-if="!issue.dismissed"
            :key="getIssueTrackingId(issue)"
            :class="getIssueRowClass(issue)">
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
      <div class="text-muted">
        <span class="fa fa-3x fa-folder-open text-muted-more">
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
      issues: []
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
    issues: {
      deep: true,
      handler: function (newValue) {}
    },
    '$route.query.sort': function (newVal, oldVal) {
      this.loadData();
    },
    '$route.query.order': function (newVal, oldVal) {
      this.loadData();
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
        for (let [index, issue] of this.issues.entries()) {
          if (issue.type === changeEvent.issue.type && issue.clusterId === changeEvent.issue.clusterId &&
            issue.groupId === changeEvent.issue.groupId && issue.node === changeEvent.issue.node) {
            if (changeEvent.issue.dismissed) { // remove dismissed issues
              this.issues.splice(index, 1);
            } else { // update ignored issues
              issue = changeEvent.issue;
            }
          }
        }
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
      if (issue.ignoreUntil) {
        return 'table-secondary text-muted';
      } else if (issue.severity === 'red') {
        return 'table-danger';
      } else if (issue.severity === 'yellow') {
        return 'table-warning';
      }

      return '';
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
      clearInterval(interval);
    }
  }
};
</script>
