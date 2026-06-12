<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div
    v-if="isUser"
    class="text-end">
    <!-- remove issue button -->
    <v-btn
      v-if="issue.acknowledged"
      size="x-small"
      variant="outlined"
      color="primary"
      class="me-1"
      @click="removeIssue">
      <v-icon icon="mdi-delete" />
      <v-tooltip
        activator="parent"
        location="left">
        {{ $t('parliament.issue.issueFixed') }}
      </v-tooltip>
    </v-btn>
    <!-- /remove issue button -->
    <!-- acknowledge issue button -->
    <v-btn
      v-if="!issue.acknowledged"
      size="x-small"
      variant="outlined"
      color="success"
      class="me-1"
      @click="acknowledgeIssue">
      <v-icon icon="mdi-check" />
      <v-tooltip
        activator="parent"
        location="left">
        {{ $t('parliament.issue.issueAckTip') }}
      </v-tooltip>
    </v-btn>
    <!-- /acknowledge issue button -->
    <!-- (un)ignore until dropdown -->
    <v-menu location="bottom end">
      <template #activator="{ props: activatorProps }">
        <v-btn
          v-bind="activatorProps"
          size="x-small"
          variant="outlined"
          class="d-inline">
          <v-icon :icon="issue.ignoreUntil ? 'mdi-eye-off' : 'mdi-eye'" />
          <v-tooltip
            activator="parent"
            location="left">
            {{ $t(issue.ignoreUntil ? 'parliament.issue.unignoreTip' : 'parliament.issue.ignoreTip') }}
          </v-tooltip>
        </v-btn>
      </template>
      <v-list density="compact">
        <template v-if="issue.ignoreUntil">
          <v-list-item @click="removeIgnore">
            <v-list-item-title>{{ $t('parliament.issue.removeIgnore') }}</v-list-item-title>
          </v-list-item>
          <v-divider />
        </template>
        <v-list-item @click="ignoreIssue(3600000)">
          <v-list-item-title>{{ $t('parliament.issue.ignoreHourCount', 1) }}</v-list-item-title>
        </v-list-item>
        <v-list-item @click="ignoreIssue(21600000)">
          <v-list-item-title>{{ $t('parliament.issue.ignoreHourCount', 6) }}</v-list-item-title>
        </v-list-item>
        <v-list-item @click="ignoreIssue(43200000)">
          <v-list-item-title>{{ $t('parliament.issue.ignoreHourCount', 12) }}</v-list-item-title>
        </v-list-item>
        <v-list-item @click="ignoreIssue(86400000)">
          <v-list-item-title>{{ $t('parliament.issue.ignoreDayCount', 1) }}</v-list-item-title>
        </v-list-item>
        <v-list-item @click="ignoreIssue(604800000)">
          <v-list-item-title>{{ $t('parliament.issue.ignoreWeekCount', 1) }}</v-list-item-title>
        </v-list-item>
        <v-list-item @click="ignoreIssue(2592000000)">
          <v-list-item-title>{{ $t('parliament.issue.ignoreMonthCount', 1) }}</v-list-item-title>
        </v-list-item>
        <v-list-item @click="ignoreIssue(-1)">
          <v-list-item-title>{{ $t('parliament.issue.ignoreForever') }}</v-list-item-title>
        </v-list-item>
      </v-list>
    </v-menu> <!-- /(un)ignore until dropdown -->
  </div>
</template>

<script>
import ParliamentService from './parliament.service.js';

export default {
  name: 'IssueActions',
  emits: ['issueChange'],
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
