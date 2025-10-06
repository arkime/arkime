<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div
    v-if="isUser"
    class="text-end"
  >
    <!-- remove issue button -->
    <template v-if="issue.acknowledged">
      <button
        :id="`removeIssueTooltip-${issue.clusterId}-${issue.type}-${issue.firstNoticed}`"
        class="btn btn-outline-primary btn-xs cursor-pointer me-1"
        @click="removeIssue"
      >
        <span class="fa fa-trash fa-fw" />
      </button>
      <BTooltip
        :target="`removeIssueTooltip-${issue.clusterId}-${issue.type}-${issue.firstNoticed}`"
        placement="left"
      >
        Issue fixed! Remove it.
      </BTooltip>
    </template>
    <!-- /remove issue button -->
    <!-- acknowledge issue button -->
    <button
      v-if="!issue.acknowledged"
      :id="`acknowledgeIssueTooltip-${issue.clusterId}-${issue.type}-${issue.firstNoticed}`"
      class="btn btn-outline-success btn-xs cursor-pointer me-1"
      @click="acknowledgeIssue"
    >
      <span class="fa fa-check fa-fw" />
    </button>
    <BTooltip
      :target="`acknowledgeIssueTooltip-${issue.clusterId}-${issue.type}-${issue.firstNoticed}`"
      placement="left"
    >
      Acknowledge this issue. It will be removed automatically or can be removed manually after the issue has been resolved.
    </BTooltip>
    <!-- /acknowledge issue button -->
    <!-- (un)ignore until dropdown -->
    <b-dropdown
      right
      size="sm"
      class="dropdown-btn-xs d-inline"
      variant="outline-dark"
    >
      <template #button-content>
        <span
          v-if="!issue.ignoreUntil"
          class="fa fa-eye fa-fw"
        />
        <span
          v-else
          class="fa fa-eye-slash fa-fw"
        />
        <span class="sr-only">
          Ignore
        </span>
      </template>
      <template v-if="issue.ignoreUntil">
        <b-dropdown-item @click="removeIgnore">
          Remove Ignore
        </b-dropdown-item>
        <b-dropdown-divider />
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
  </div>
</template>

<script>
import ParliamentService from './parliament.service.js';

export default {
  name: 'IssueActions',
  props: {
    issue: {
      type: Object,
      required: true
    },
    groupId: {
      type: String,
      default: function (props) {
        return props.issue.groupId;
      }
    },
    clusterId: {
      type: String,
      default: function (props) {
        return props.issue.clusterId;
      }
    }
  },
  computed: {
    isUser: function () {
      return this.$store.state.isUser;
    }
  },
  methods: {
    /* page functions -------------------------------------------------------- */
    /* Sends a request to acknowledge an issue
     * If successful, updates the issue in the view, otherwise displays error */
    acknowledgeIssue: function () {
      ParliamentService.acknowledgeIssues([this.issue])
        .then((data) => {
          const issueClone = JSON.parse(JSON.stringify(this.issue));
          issueClone.acknowledged = data.acknowledged;
          this.updateIssue(true, 'Issue acknowledged', issueClone);
        })
        .catch((error) => {
          this.updateIssue(false, error || 'Unable to acknowledge this issue');
        });
    },
    /* Sends a request to remove an issue
     * If successful, removes the issue from the view, otherwise displays error */
    removeIssue: function () {
      ParliamentService.removeIssue(this.groupId, this.clusterId, this.issue)
        .then((data) => {
          this.updateIssue(true, 'Issue removed', undefined);
        })
        .catch((error) => {
          this.updateIssue(false, error || 'Unable to remove this issue');
        });
    },
    /**
     * Sends a request to ignore an issue
     * If successful, updates the issue in the view, otherwise displays error
     * @param {number} forMs - the amount of time (in ms) that the issue should be ignored
     */
    ignoreIssue: function (forMs) {
      ParliamentService.ignoreIssues([this.issue], forMs)
        .then((data) => {
          const issueClone = JSON.parse(JSON.stringify(this.issue));
          issueClone.ignoreUntil = data.ignoreUntil;
          this.updateIssue(true, 'Issue ignored', issueClone);
        })
        .catch((error) => {
          this.updateIssue(false, error || 'Unable to ignore this issue');
        });
    },
    /* Sends a request to remove an ignore for an issue
     * If successful, updates the issue in the view, otherwise displays error */
    removeIgnore: function () {
      ParliamentService.removeIgnoreIssues([this.issue])
        .then((data) => {
          const issueClone = JSON.parse(JSON.stringify(this.issue));
          issueClone.ignoreUntil = undefined;
          this.updateIssue(true, 'Issue unignored', issueClone);
        })
        .catch((error) => {
          this.updateIssue(false, error || 'Unable to unignore this issue');
        });
    },
    /* helper functions ---------------------------------------------------- */
    updateIssue: function (success, message, issue) {
      const emit = {
        success,
        message,
        groupId: this.groupId,
        clusterId: this.clusterId
      };

      if (issue) { emit.issue = issue; }

      this.$emit('issueChange', emit);
    }
  }
};
</script>
