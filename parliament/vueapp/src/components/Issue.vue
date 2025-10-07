<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div
    class="alert alert-sm mt-1 mb-0"
    :class="{'alert-warning':issue.severity==='yellow','alert-danger':issue.severity==='red'}">
    <issue-actions
      v-if="isUser"
      class="issue-btns"
      :issue="issue"
      :group-id="groupId"
      :cluster-id="clusterId"
      @issue-change="issueChange" />
    {{ issue.message }}
    <br>
    <small
      class="cursor-help issue-date"
      :id="`issueDateTooltip-${groupId}-${clusterId}-${index}`">
      {{ moment(issue.lastNoticed || issue.firstNoticed, 'MM/DD HH:mm:ss') }}
    </small>
    <BTooltip :target="`issueDateTooltip-${groupId}-${clusterId}-${index}`">
      <span v-html="issueDateTooltip(issue)" />
    </BTooltip>
  </div>
</template>

<script>
import IssueActions from './IssueActions.vue';
import moment from 'moment-timezone';

export default {
  name: 'Issue',
  emits: ['issueChange'],
  components: {
    IssueActions
  },
  props: {
    issue: {
      type: Object,
      required: true
    },
    groupId: {
      type: String,
      required: true
    },
    clusterId: {
      type: String,
      required: true
    },
    index: {
      type: Number,
      required: true
    }
  },
  computed: {
    isUser () {
      return this.$store.state.isUser;
    }
  },
  methods: {
    moment: function (date, format) {
      return moment(date).format(format);
    },
    issueDateTooltip: function (issue) {
      const firstNoticed = moment(issue.firstNoticed).format('YYYY/MM/DD HH:mm:ss');

      let htmlStr =
      `<small>
        <div>
          <strong>First</strong>
          noticed at:
          <br>
          <strong>
            ${firstNoticed}
          </strong>
        </div>`;

      if (issue.lastNoticed) {
        const lastNoticed = moment(issue.lastNoticed).format('YYYY/MM/DD HH:mm:ss');
        htmlStr +=
          `<div>
            <strong>Last</strong>
            noticed at:
            <br>
            <strong>
              ${lastNoticed}
            </strong>
          </div>`;
      }

      htmlStr += '</small>';
      return htmlStr;
    },
    issueChange: function (changeEvent) {
      // populate the change up to the parliament
      changeEvent.index = this.index;
      this.$emit('issueChange', changeEvent);
    }
  }
};
</script>

<style scoped>
.alert .issue-btns {
  margin-top: -3px;
  display: inline-block;
  float: right;
}

.alert .issue-date {
  color: #676767;
  font-style: italic;
}
</style>
