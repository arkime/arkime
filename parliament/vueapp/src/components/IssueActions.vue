<template>

  <div>
    <!-- (un)ignore until dropdown -->
    <b-dropdown right
      size="sm"
      class="dropdown-btn-xs pull-right ml-1"
      variant="outline-dark">
      <template slot="button-content">
        <span v-if="!issue.ignoreUntil"
          class="fa fa-eye fa-fw">
        </span>
        <span v-else
          class="fa fa-eye-slash fa-fw">
        </span>
        <span class="sr-only">
          Ignore
        </span>
      </template>
      <template v-if="issue.ignoreUntil">
        <b-dropdown-item @click="removeIgnore">
          Remove Ignore
        </b-dropdown-item>
        <b-dropdown-divider></b-dropdown-divider>
      </template>
      <b-dropdown-item @click="ignoreIssue(3600000)">
        Ignore for 1 hour
      </b-dropdown-item>
      <b-dropdown-item @click="ignoreIssue(21600000)">
        Ignore for 6 hour
      </b-dropdown-item>
      <b-dropdown-item @click="ignoreIssue(43200000)">
        Ignore for 12 hour
      </b-dropdown-item>
      <b-dropdown-item @click="ignoreIssue(86400000)">
        Ignore for 1 day
      </b-dropdown-item>
      <b-dropdown-item @click="ignoreIssue(604800000)">
        Ignore for 1 week
      </b-dropdown-item>
      <b-dropdown-item @click="ignoreIssue(2592000000)">
        Ignore for 1 month
      </b-dropdown-item>
      <b-dropdown-item @click="ignoreIssue(-1)">
        Ignore forever
      </b-dropdown-item>
    </b-dropdown> <!-- /(un)ignore until dropdown -->
    <!-- acknowledge issue button -->
    <button v-if="!issue.acknowledged"
      class="btn btn-outline-success btn-xs pull-right cursor-pointer"
      v-b-tooltip.hover
      title="Acknowledge this issue. It will be removed automatically or can be removed manually after the issue has been resolved."
      @click="acknowledgeIssue">
      <span class="fa fa-check fa-fw">
      </span>
    </button> <!-- /acknowledge issue button -->
    <!-- remove issue button -->
    <button v-if="issue.acknowledged"
      class="btn btn-outline-primary btn-xs pull-right cursor-pointer"
      v-b-tooltip.hover.bottom-right
      title="Issue fixed! Remove it."
      @click="removeIssue">
      <span class="fa fa-trash fa-fw">
      </span>
    </button> <!-- /remove issue button -->
  </div>

</template>

<script>
import ParliamentService from './parliament.service';

export default {
  name: 'IssueActions',
  props: {
    issue: {
      type: Object,
      required: true
    },
    groupId: {
      type: Number,
      default: function () {
        return this.issue.groupId;
      }
    },
    clusterId: {
      type: Number,
      default: function () {
        return this.issue.clusterId;
      }
    }
  },
  methods: {
    /* page functions -------------------------------------------------------- */
    /* Sends a request to acknowledge an issue
     * If succesful, updates the issue in the view, otherwise displays error */
    acknowledgeIssue: function () {
      ParliamentService.acknowledgeIssues([this.issue])
        .then((data) => {
          this.issue.acknowledged = data.acknowledged;
          this.updateIssue(true, 'Issue acknowledged', this.issue);
        })
        .catch((error) => {
          this.updateIssue(false, error.text || 'Unable to acknowledge this issue');
        });
    },
    /* Sends a request to remove an issue
     * If succesful, removes the issue from the view, otherwise displays error */
    removeIssue: function () {
      ParliamentService.removeIssue(this.groupId, this.clusterId, this.issue)
        .then((data) => {
          this.issue = undefined;
          this.updateIssue(true, 'Issue removed', this.issue);
        })
        .catch((error) => {
          this.updateIssue(false, error.text || 'Unable to remove this issue');
        });
    },
    /**
     * Sends a request to ignore an issue
     * If succesful, updates the issue in the view, otherwise displays error
     * @param {number} forMs - the amount of time (in ms) that the issue should be ignored
     */
    ignoreIssue: function (forMs) {
      ParliamentService.ignoreIssues([this.issue], forMs)
        .then((data) => {
          this.issue.ignoreUntil = data.ignoreUntil;
          this.updateIssue(true, 'Issue ignored', this.issue);
        })
        .catch((error) => {
          this.updateIssue(false, error.text || 'Unable to ignore this issue');
        });
    },
    /* Sends a request to remove an ignore for an issue
     * If succesful, updates the issue in the view, otherwise displays error */
    removeIgnore: function () {
      ParliamentService.removeIgnoreIssues([this.issue])
        .then((data) => {
          this.issue.ignoreUntil = undefined;
          this.updateIssue(true, 'Issue unignored', this.issue);
        })
        .catch((error) => {
          this.updateIssue(false, error.text || 'Unable to unignore this issue');
        });
    },
    /* helper functions ---------------------------------------------------- */
    updateIssue: function (success, message, issue) {
      let emit = {
        success: success,
        message: message,
        groupId: this.groupId,
        clusterId: this.clusterId
      };

      if (issue) { emit.issue = issue; }

      this.$emit('issueChange', emit);
    }
  }
};
</script>
