<template>

  <div class="alert alert-sm"
    :class="{'alert-warning':issue.severity==='yellow','alert-danger':issue.severity==='red'}">
    <issue-actions v-if="loggedIn"
      class="issue-btns"
      :issue="issue"
      :groupId="groupId"
      :clusterId="clusterId"
      @issueChange="issueChange">
    </issue-actions>
    {{ issue.message }}
    <br>
    <small class="cursor-help issue-date"
      v-b-tooltip.hover.top-left.html="issueDateTooltip(issue)">
      {{ issue.lastNoticed || issue.firstNoticed | moment('MM/DD HH:mm:ss') }}
    </small>
  </div>

</template>

<script>
import IssueActions from './IssueActions';

export default {
  name: 'Issue',
  components: {
    IssueActions
  },
  props: {
    issue: {
      type: Object,
      required: true
    },
    groupId: {
      type: Number,
      required: true
    },
    clusterId: {
      type: Number,
      required: true
    },
    index: { // TODO
      type: Number,
      required: true
    },
    loggedIn: Boolean
  },
  methods: {
    issueDateTooltip: function (issue) {
      let firstNoticed = this.$options.filters.moment(issue.firstNoticed, 'YYYY/MM/DD HH:mm:ss');

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
        let lastNoticed = this.$options.filters.moment(issue.lastNoticed, 'YYYY/MM/DD HH:mm:ss');
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
